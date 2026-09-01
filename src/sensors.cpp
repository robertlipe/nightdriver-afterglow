#include "globals.h"

// NightDriverStrip - (c) 2026 Robert Lipe All Rights Reserved.
//
// Description:
//
//    Implements SensorManager to read DHT11 and internal ESP32 chip temperatures.
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------

#include "sensors.h"
#include "values.h"

#include <esp_cpu.h>
#include <esp_timer.h>
#include <soc/soc_caps.h>

#if SOC_TEMP_SENSOR_SUPPORTED
#define HAS_TEMP_SENSOR 1
#include <driver/temperature_sensor.h>
static temperature_sensor_handle_t s_temp_sensor = nullptr;
#else
#define HAS_TEMP_SENSOR 0
#endif

#ifdef DHT11_PIN
static bool s_dhtSensorPresent = false;

namespace
{

struct InterruptGuard
{
    InterruptGuard()
    {
        noInterrupts();
    }

    ~InterruptGuard()
    {
        interrupts();
    }

    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;
};

struct PulseMeasurer
{
    int pin;
    uint32_t cyclesPerUs;

    bool WaitForState(bool targetState, uint32_t timeoutCycles) const
    {
        uint32_t start = esp_cpu_get_cycle_count();
        while (digitalRead(pin) != targetState)
        {
            if (esp_cpu_get_cycle_count() - start > timeoutCycles)
            {
                return false;
            }
        }
        return true;
    }

    int32_t MeasurePulse(bool state, uint32_t timeoutCycles) const
    {
        uint32_t start = esp_cpu_get_cycle_count();
        uint32_t transitionCycle = 0;
        const uint32_t stableCycles = 3 * cyclesPerUs; // 3 microseconds stability window

        while (true)
        {
            if (digitalRead(pin) != state)
            {
                if (transitionCycle == 0)
                {
                    transitionCycle = esp_cpu_get_cycle_count();
                }

                uint32_t checkStart = esp_cpu_get_cycle_count();
                bool stable = true;
                while (esp_cpu_get_cycle_count() - checkStart < stableCycles)
                {
                    if (digitalRead(pin) == state)
                    {
                        stable = false;
                        transitionCycle = 0; // Reset, it was a transient glitch
                        break;
                    }
                }

                if (stable)
                {
                    break;
                }
            }

            if ((esp_cpu_get_cycle_count() - start) > timeoutCycles)
            {
                return -1;
            }
        }
        return transitionCycle - start;
    }
};

void SendStartSignal(int pin)
{
    // 1. Send start signal
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(20); // Keep low for at least 18ms

    // 2. End start signal by setting it HIGH for a brief period
    digitalWrite(pin, HIGH);
    delayMicroseconds(2); // Short active-high drive to overcome line capacitance

    // 3. Switch to input with pullup and let line stabilize
    pinMode(pin, INPUT_PULLUP);
}

enum class HandshakeStatus
{
    Success,
    DidNotGoLow,
    DidNotGoHigh,
    LowPremature,
    Bit0StartTimeout
};

enum class BitstreamStatus
{
    Success,
    LowTimeout,
    HighTimeout,
    Bit39RecoveredZero,
    Bit39RecoveredOne,
    Bit39RecoveryFailed
};

struct BitstreamResult
{
    BitstreamStatus status = BitstreamStatus::Success;
    int failedBitIndex = -1;
    uint8_t expectedSum = 0;
    uint8_t checksumZero = 0;
    uint8_t checksumOne = 0;
};

HandshakeStatus PerformHandshake(const PulseMeasurer& pm, uint32_t handshakeTimeoutCycles, uint32_t cyclesPerUs)
{
    // Wait for DHT to pull LOW (start of response)
    if (!pm.WaitForState(LOW, handshakeTimeoutCycles))
    {
        return HandshakeStatus::DidNotGoLow;
    }

    // Wait for DHT to release LOW and go HIGH (start of 80us HIGH response)
    if (!pm.WaitForState(HIGH, handshakeTimeoutCycles))
    {
        return HandshakeStatus::DidNotGoHigh;
    }

    // Delay 30us to get past any rise-time oscillations and reach stable middle of 80us HIGH phase
    uint32_t delayStart = esp_cpu_get_cycle_count();
    uint32_t delayCycles = 30 * cyclesPerUs;
    while (esp_cpu_get_cycle_count() - delayStart < delayCycles)
    {
        // Busy wait
    }

    // Verify line is still HIGH
    if (digitalRead(pm.pin) != HIGH)
    {
        return HandshakeStatus::LowPremature;
    }

    // Wait for line to go LOW to start Bit 0
    if (!pm.WaitForState(LOW, handshakeTimeoutCycles))
    {
        return HandshakeStatus::Bit0StartTimeout;
    }

    return HandshakeStatus::Success;
}

BitstreamResult TryRecoverBit39(uint8_t data[5])
{
    BitstreamResult res;
    res.expectedSum = data[0] + data[1] + data[2] + data[3];
    res.checksumZero = data[4];
    res.checksumOne  = data[4] | 1;

    if (res.checksumZero == res.expectedSum)
    {
        data[4] = res.checksumZero;
        res.status = BitstreamStatus::Bit39RecoveredZero;
        return res;
    }

    if (res.checksumOne == res.expectedSum)
    {
        data[4] = res.checksumOne;
        res.status = BitstreamStatus::Bit39RecoveredOne;
        return res;
    }

    res.status = BitstreamStatus::Bit39RecoveryFailed;
    return res;
}

BitstreamResult ReadBitstream(const PulseMeasurer& pm, uint32_t bitTimeoutCycles, uint8_t data[5])
{
    for (int i = 0; i < 40; ++i)
    {
        int32_t lowDuration = pm.MeasurePulse(LOW, bitTimeoutCycles);
        if (lowDuration < 0)
        {
            return {BitstreamStatus::LowTimeout, i};
        }

        int32_t highDuration = pm.MeasurePulse(HIGH, bitTimeoutCycles);
        int byteIndex = i / 8;
        data[byteIndex] <<= 1;

        if (highDuration < 0)
        {
            if (i == 39)
            {
                return TryRecoverBit39(data);
            }

            return {BitstreamStatus::HighTimeout, i};
        }

        if (highDuration > lowDuration)
        {
            data[byteIndex] |= 1;
        }
    }

    return {BitstreamStatus::Success, -1};
}

bool DecodeAndValidate(const uint8_t data[5], float& tempF, float& humidity)
{
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4])
    {
        debugD("DHT11 checksum mismatch: calculated %02x, received %02x (data: %02x %02x %02x %02x)",
               checksum, data[4], data[0], data[1], data[2], data[3]);
        return false;
    }

    humidity = static_cast<float>(data[0]);
    float tempC = static_cast<float>(data[2]);
    tempF = (tempC * 9.0f / 5.0f) + 32.0f;
    return true;
}

} // namespace

bool SensorManager::ReadDHT11(int pin, float& tempF, float& humidity)
{
    uint8_t data[5] = {0};

    SendStartSignal(pin);

    const uint32_t cyclesPerUs = ESP.getCpuFreqMHz();
    const uint32_t handshakeTimeoutCycles = 500 * cyclesPerUs;
    const uint32_t bitTimeoutCycles = 500 * cyclesPerUs;
    const PulseMeasurer pm{pin, cyclesPerUs};

    uint32_t elapsedCycles = 0;
    HandshakeStatus handshakeStatus = HandshakeStatus::Success;
    BitstreamResult bitstreamResult;

    {
        InterruptGuard guard;
        uint32_t startCycleCount = esp_cpu_get_cycle_count();

        handshakeStatus = PerformHandshake(pm, handshakeTimeoutCycles, cyclesPerUs);
        if (handshakeStatus == HandshakeStatus::Success)
        {
            bitstreamResult = ReadBitstream(pm, bitTimeoutCycles, data);
            elapsedCycles = esp_cpu_get_cycle_count() - startCycleCount;
        }
    }

    // Process Handshake logging outside of critical section
    switch (handshakeStatus)
    {
    case HandshakeStatus::DidNotGoLow:
        debugD("DHT11 handshake failed: Pin did not go LOW");
        return false;
    case HandshakeStatus::DidNotGoHigh:
        debugD("DHT11 handshake failed: Pin did not go HIGH");
        return false;
    case HandshakeStatus::LowPremature:
        debugD("DHT11 handshake failed: Pin went LOW prematurely during 80us HIGH phase");
        return false;
    case HandshakeStatus::Bit0StartTimeout:
        debugD("DHT11 handshake failed: Pin did not go LOW to start Bit 0");
        return false;
    case HandshakeStatus::Success:
        break;
    }

    // Process Bitstream logging outside of critical section
    switch (bitstreamResult.status)
    {
    case BitstreamStatus::LowTimeout:
        debugD("DHT11 bit %d low timeout", bitstreamResult.failedBitIndex);
        return false;
    case BitstreamStatus::HighTimeout:
        debugD("DHT11 bit %d high timeout", bitstreamResult.failedBitIndex);
        return false;
    case BitstreamStatus::Bit39RecoveredZero:
        debugD("DHT11 bit 39 high timeout recovered as 0 via checksum matching.");
        break;
    case BitstreamStatus::Bit39RecoveredOne:
        debugD("DHT11 bit 39 high timeout recovered as 1 via checksum matching.");
        break;
    case BitstreamStatus::Bit39RecoveryFailed:
        debugD("DHT11 bit 39 high timeout recovery failed. Expected sum: %02x, got %02x or %02x",
               bitstreamResult.expectedSum, bitstreamResult.checksumZero, bitstreamResult.checksumOne);
        return false;
    case BitstreamStatus::Success:
        break;
    }

    if (elapsedCycles > (6000 * cyclesPerUs))
    {
        debugD("DHT11 read took too long: %u cycles", elapsedCycles);
        return false;
    }

    return DecodeAndValidate(data, tempF, humidity);
}
#endif

void SensorManager::begin()
{
#if HAS_TEMP_SENSOR
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_sensor_config, &s_temp_sensor) == ESP_OK)
    {
        temperature_sensor_enable(s_temp_sensor);
    }
#endif

#ifdef DHT11_PIN
    s_dhtSensorPresent = false;
    for (int i = 0; i < 3; ++i)
    {
        float temp, hum;
        if (ReadDHT11(DHT11_PIN, temp, hum))
        {
            s_dhtSensorPresent = true;
            g_Values.AmbientTemp = temp;
            g_Values.AmbientHumidity = hum;
            debugI("DHT11 sensor detected on GPIO %d. Temp: %.1f F, Humidity: %.1f%%", DHT11_PIN, temp, hum);
            break;
        }
        delay(100);
    }
    if (!s_dhtSensorPresent)
    {
        debugI("No DHT11 sensor detected on GPIO %d during startup. Will retry in background.", DHT11_PIN);
    }
#endif
}

void SensorManager::Update()
{
#if HAS_TEMP_SENSOR
    if (s_temp_sensor != nullptr)
    {
        float tsens_out = 0;
        if (temperature_sensor_get_celsius(s_temp_sensor, &tsens_out) == ESP_OK)
        {
            g_Values.InternalTemp = (tsens_out * 9.0f / 5.0f) + 32.0f;
        }
    }
#endif

#ifdef DHT11_PIN
    if (s_dhtSensorPresent)
    {
        float temp = 0, hum = 0;
        if (ReadDHT11(DHT11_PIN, temp, hum))
        {
            g_Values.AmbientTemp = temp;
            g_Values.AmbientHumidity = hum;

            // Thermal Protection Check: if temp > 120F, throttle brightness
            if (temp > 120.0f)
            {
                Serial.printf("!!! THERMAL WARNING: Cabinet temperature %.1f F !!! Throttling brightness.\n", temp);
                g_Values.Brite = std::min(g_Values.Brite, 10.0f);
            }
        }
    }
    else
    {
        // Retry detection periodically in background (up to 12 times = 2 minutes of uptime)
        static int s_retryCount = 0;
        if (s_retryCount < 12)
        {
            s_retryCount++;
            float temp = 0, hum = 0;
            if (ReadDHT11(DHT11_PIN, temp, hum))
            {
                s_dhtSensorPresent = true;
                g_Values.AmbientTemp = temp;
                g_Values.AmbientHumidity = hum;
                debugI("DHT11 sensor detected on GPIO %d after background retry %d. Temp: %.1f F, Humidity: %.1f%%",
                       DHT11_PIN, s_retryCount, temp, hum);
            }
        }
    }
#endif
}
