//+--------------------------------------------------------------------------
//
// File:        sensors.cpp
//
// NightDriverStrip - (c) 2026 Plummer's Software LLC.  All Rights Reserved.
//
// Description:
//
//    Implements SensorManager to read DHT11 and internal ESP32 chip temperatures.
//
//---------------------------------------------------------------------------

#include "globals.h"
#include "sensors.h"
#include "values.h"

#include <esp_timer.h>
#include <esp_cpu.h>
#include <soc/soc_caps.h>

#if SOC_TEMP_SENSOR_SUPPORTED
#include <driver/temperature_sensor.h>
static temperature_sensor_handle_t s_temp_sensor = nullptr;
#endif

#ifdef DHT11_PIN
static bool s_dhtSensorPresent = false;

bool SensorManager::ReadDHT11(int pin, float& tempF, float& humidity)
{
    uint8_t data[5] = {0};

    // 1. Send start signal
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(20); // Keep low for at least 18ms

    // 2. End the start signal by setting it HIGH for a brief period
    digitalWrite(pin, HIGH);
    delayMicroseconds(2); // Short active-high drive to overcome line capacitance

    // 3. Switch to input with pullup and let the line stabilize
    pinMode(pin, INPUT_PULLUP);

    // Disable interrupts for the 4ms critical timing window to prevent FreeRTOS ticks from preempting
    noInterrupts();

    // Dynamically calculate timeout cycles based on the CPU frequency to support any clock speed.
    const uint32_t cyclesPerUs = ESP.getCpuFreqMHz();
    const uint32_t handshakeTimeoutCycles = 500 * cyclesPerUs; // 500us timeout
    const uint32_t bitTimeoutCycles = 500 * cyclesPerUs;       // 500us timeout

    // We do NOT use esp_timer_get_time() for elapsed check inside critical section to avoid function call overhead.
    // Instead, we measure the elapsed cycles.
    uint32_t startCycleCount = esp_cpu_get_cycle_count();

    // Lambda helper to measure pulse durations in CPU cycles. Returns -1 on timeout.
    auto measurePulse = [pin](bool state, uint32_t timeoutCycles) -> int32_t
    {
        uint32_t start = esp_cpu_get_cycle_count();
        uint32_t transitionCycle = 0;
        const uint32_t cyclesPerUs = ESP.getCpuFreqMHz();
        const uint32_t stableCycles = 3 * cyclesPerUs; // 3 microseconds stability window
        
        while (true)
        {
            if (digitalRead(pin) != state)
            {
                if (transitionCycle == 0)
                {
                    transitionCycle = esp_cpu_get_cycle_count();
                }
                
                // Verify the transition is stable by ensuring it stays in the new state for 3us.
                // 3us is safely below any valid DHT11 pulse width (min ~26us) but far above
                // nanosecond-scale contact bounce or signal rise-time oscillations.
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
    };

    // Wait for response handshake (DHT pulls low ~80us, then high ~80us)
    int32_t initialHigh = measurePulse(HIGH, handshakeTimeoutCycles);
    int32_t responseLow = measurePulse(LOW, handshakeTimeoutCycles);
    int32_t responseHigh = measurePulse(HIGH, handshakeTimeoutCycles);

    if (initialHigh < 0 || responseLow < 0 || responseHigh < 0)
    {
        interrupts(); // Re-enable interrupts before returning
        debugI("DHT11 handshake failed. Pin: %d, initialHigh: %d, responseLow: %d, responseHigh: %d",
               pin, initialHigh, responseLow, responseHigh);
        return false;
    }

    // 3. Read the 40-bit transmission
    for (int i = 0; i < 40; ++i)
    {
        // Every bit starts with a 50us low pulse
        int32_t lowDuration = measurePulse(LOW, bitTimeoutCycles);
        if (lowDuration < 0)
        {
            interrupts(); // Re-enable interrupts before returning
            debugI("DHT11 bit %d low timeout", i);
            return false;
        }

        // The duration of the high pulse determines if it's a 0 (~28us) or a 1 (~70us)
        int32_t highDuration = measurePulse(HIGH, bitTimeoutCycles);
        
        int byteIndex = i / 8;
        data[byteIndex] <<= 1;

        if (highDuration < 0)
        {
            // If we timed out on the high pulse of the very last bit (index 39), we can reconstruct it
            // because the 40th bit is the LSB of the 8-bit checksum. The expected checksum is uniquely
            // determined by the sum of the first 4 bytes, so we can verify if 0 or 1 matches.
            if (i == 39)
            {
                interrupts(); // Re-enable interrupts
                
                uint8_t expectedSum = data[0] + data[1] + data[2] + data[3];
                uint8_t checksumWithZero = data[4]; // Already shifted left
                uint8_t checksumWithOne  = data[4] | 1;
                
                if (checksumWithZero == expectedSum)
                {
                    data[4] = checksumWithZero;
                    humidity = static_cast<float>(data[0]);
                    float tempC = static_cast<float>(data[2]);
                    tempF = (tempC * 9.0f / 5.0f) + 32.0f;
                    debugD("DHT11 bit 39 high timeout recovered as 0 via checksum matching.");
                    return true;
                }
                else if (checksumWithOne == expectedSum)
                {
                    data[4] = checksumWithOne;
                    humidity = static_cast<float>(data[0]);
                    float tempC = static_cast<float>(data[2]);
                    tempF = (tempC * 9.0f / 5.0f) + 32.0f;
                    debugD("DHT11 bit 39 high timeout recovered as 1 via checksum matching.");
                    return true;
                }
                else
                {
                    debugI("DHT11 bit 39 high timeout recovery failed. Expected sum: %02x, got %02x or %02x",
                           expectedSum, checksumWithZero, checksumWithOne);
                    return false;
                }
            }
            else
            {
                interrupts(); // Re-enable interrupts before returning
                debugI("DHT11 bit %d high timeout", i);
                return false;
            }
        }
        
        // Robust comparison: if the HIGH pulse duration is greater than the LOW pulse duration, it's a 1.
        if (highDuration > lowDuration)
        {
            data[byteIndex] |= 1;
        }
    }

    uint32_t elapsedCycles = esp_cpu_get_cycle_count() - startCycleCount;
    
    interrupts(); // Re-enable interrupts

    // If it took longer than 6ms (6000us), discard.
    if (elapsedCycles > (6000 * cyclesPerUs))
    {
        debugI("DHT11 read took too long: %u cycles", elapsedCycles);
        return false;
    }

    // 4. Verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4])
    {
        debugI("DHT11 checksum mismatch: calculated %02x, received %02x (data: %02x %02x %02x %02x)",
               checksum, data[4], data[0], data[1], data[2], data[3]);
        return false;
    }

    // 5. Decode variables
    humidity = static_cast<float>(data[0]);
    float tempC = static_cast<float>(data[2]);
    tempF = (tempC * 9.0f / 5.0f) + 32.0f;

    return true;
}
#endif

void SensorManager::begin()
{
#if SOC_TEMP_SENSOR_SUPPORTED
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
#if SOC_TEMP_SENSOR_SUPPORTED
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
