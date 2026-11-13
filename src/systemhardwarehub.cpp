#include "globals.h"
#include "systemhardwarehub.h"

#if HAS_SYSTEM_HARDWARE_HUB

#include <ctime>
#include <esp_log.h>
#include <esp_timer.h>
#include <sys/time.h>

#include "byte_utils.h"
#include "values.h"

#if MATRIX_S3
#define BOARD_ADAFRUIT_MATRIX_S3 1
#elif WAVESHARE_ESP32_S3_RGB_MATRIX
#define BOARD_WAVESHARE_S3 1
#endif

// static const char *TAG = "HW_HUB";

// =========================================================
// BOARD-SPECIFIC PROFILES & REGISTER CODES
// =========================================================
#if defined(BOARD_ADAFRUIT_MATRIX_S3)
#define PIN_SDA GPIO_NUM_2
#define PIN_SCL GPIO_NUM_3
#define SENSOR_ADDR 0x19 // LIS3DH Accel
#define REG_DATA_START (0x28 | 0x80)
#define HAS_LIS3DH
// Assuming your BME280 is added as a sidecar sensor on this board layout
#define HAS_BME280
#define CLIMATE_ADDR 0x76
#elif defined(BOARD_WAVESHARE_S3)
// Waveshare ESP32-S3-RGB-Matrix. The I2C bus is shared by the ES7210,
// ES8311, QMI8658, SHTC3, and PCF85063. GPIO 47/48 are the board's
// I2C_SDA/I2C_SCL nets, not the memory-interface pins (those are 33-37).
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 47
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 48
#endif
#define PIN_SDA ((gpio_num_t)I2C_SDA_PIN)
#define PIN_SCL ((gpio_num_t)I2C_SCL_PIN)
#define SENSOR_ADDR 0x6B // QMI8658 Accel (SA0 pulled high on Waveshare board)
#define REG_DATA_START 0x35
#define HAS_QMI8658
#define HAS_SHTC3 // Waveshare's environment chip selection
#define CLIMATE_ADDR 0x70
#define HAS_PCF85063
#define RTC_ADDR 0x51
#endif

SystemHardwareHub &SystemHardwareHub::Instance()
{
    static SystemHardwareHub instance;
    return instance;
}

bool SystemHardwareHub::Begin()
{
    if (bus_handle)
        return true;

    // 1. Fire up the physical master I2C bus config
    i2c_master_bus_config_t bus_config = {.i2c_port = I2C_NUM_0,
                                          .sda_io_num = PIN_SDA,
                                          .scl_io_num = PIN_SCL,
                                          .clk_source = I2C_CLK_SRC_DEFAULT,
                                          .glitch_ignore_cnt = 7,
                                          .flags = {.enable_internal_pullup = 1}};
    if (i2c_new_master_bus(&bus_config, &bus_handle) != ESP_OK)
    {
        bus_handle = nullptr;
        debugE("I2C: failed to create master bus on SDA:%d SCL:%d", I2C_SDA_PIN, I2C_SCL_PIN);
        return false;
    }

    ScanI2CBus();

    // 2. Attach the primary structural IMU target
    sensor_handle = AddDevice(SENSOR_ADDR);
    if (!sensor_handle)
        debugE("I2C: failed to add IMU device at 0x%02X", SENSOR_ADDR);

#if defined(CLIMATE_ADDR)
    climate_handle = AddDevice(CLIMATE_ADDR);
    if (!climate_handle)
        debugE("I2C: failed to add climate sensor at 0x%02X", CLIMATE_ADDR);
#endif

#if defined(RTC_ADDR)
    rtc_handle = AddDevice(RTC_ADDR);
    if (!rtc_handle)
    {
        debugE("I2C: failed to add RTC device at 0x%02X", RTC_ADDR);
    }
    else
    {
        SyncSystemTimeFromRTC();
    }
#endif

    ConfigureSensorRegisters();
    return true;
}

i2c_master_bus_handle_t SystemHardwareHub::GetI2CBus()
{
    return Begin() ? bus_handle : nullptr;
}

i2c_master_dev_handle_t SystemHardwareHub::AddDevice(uint8_t address, uint32_t sclSpeedHz)
{
    if (!Begin())
        return nullptr;

    i2c_device_config_t dev_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                      .device_address = address, // 7-bit address, no shift
                                      .scl_speed_hz = sclSpeedHz,
                                      .scl_wait_us = 0, // 0 defaults to the driver's hardware wait state
                                      .flags = {
                                          .disable_ack_check = 0 // Keep ACK checking active for error tracking
                                      }};

    i2c_master_dev_handle_t device = nullptr;
    if (i2c_master_bus_add_device(bus_handle, &dev_config, &device) != ESP_OK)
        return nullptr;
    return device;
}

void SystemHardwareHub::ScanI2CBus()
{
    int found = 0;
    for (uint8_t address = 0x08; address < 0x78; ++address)
    {
        if (i2c_master_probe(bus_handle, address, 5) == ESP_OK)
        {
            debugI("I2C: device at 0x%02X", address);
            ++found;
        }
    }
    if (!found)
        debugW("I2C: no devices found on SDA:%d SCL:%d", I2C_SDA_PIN, I2C_SCL_PIN);
}

void SystemHardwareHub::ConfigureSensorRegisters()
{
    if (!sensor_handle)
        return;

    // Initialization sequences are directly managed without library noise
#if defined(HAS_LIS3DH)
    uint8_t cmd[] = {0x20, 0x57};
    i2c_master_transmit(sensor_handle, cmd, 2, -1);
#elif defined(HAS_QMI8658)
    // QMI8658 initialization:
    uint8_t who_am_i_reg = 0x00;
    uint8_t who_am_i_val = 0;
    if (i2c_master_transmit_receive(sensor_handle, &who_am_i_reg, 1, &who_am_i_val, 1, 50) == ESP_OK)
    {
        debugI("QMI8658 WHO_AM_I = 0x%02X (expected 0x05)", who_am_i_val);
    }
    // CTRL1 (0x02): auto-increment enabled (0x60)
    uint8_t ctrl1[] = {0x02, 0x60};
    i2c_master_transmit(sensor_handle, ctrl1, sizeof(ctrl1), 100);
    // CTRL2 (0x03): Accelerometer +/-4g (0x10), 128Hz ODR (0x05) -> 0x15
    uint8_t ctrl2[] = {0x03, 0x15};
    i2c_master_transmit(sensor_handle, ctrl2, sizeof(ctrl2), 100);
    // CTRL7 (0x08): Enable accelerometer (bit 0 = 1) -> 0x01
    uint8_t ctrl7[] = {0x08, 0x01};
    i2c_master_transmit(sensor_handle, ctrl7, sizeof(ctrl7), 100);
#endif
}

void SystemHardwareHub::Tick()
{
    uint64_t now = esp_timer_get_time();

    // Fast loop: IMU Gravity operations at ~50Hz (20ms)
    if (now - last_fast_tick >= 20000)
    {
        last_fast_tick = now;
        PollInertial();
    }

    // Non-blocking asynchronous climate state machine
    ProcessClimateState(now);
}

void SystemHardwareHub::PollInertial()
{
    if (!sensor_handle)
        return;

    uint8_t start_reg = REG_DATA_START;
    uint8_t raw_buffer[6] = {0};

    if (i2c_master_transmit_receive(sensor_handle, &start_reg, 1, raw_buffer, 6, 50) == ESP_OK)
    {
        int16_t raw_x = ReadFromMemory<int16_t>(&raw_buffer[0]);
        int16_t raw_y = ReadFromMemory<int16_t>(&raw_buffer[2]);
        int16_t raw_z = ReadFromMemory<int16_t>(&raw_buffer[4]);

        std::lock_guard<std::mutex> lock(cache_mutex);
#if defined(HAS_LIS3DH)
        cached_inertial.ax = (raw_x >> 4) * 0.001f * 9.80665f;
        cached_inertial.ay = (raw_y >> 4) * 0.001f * 9.80665f;
        cached_inertial.az = (raw_z >> 4) * 0.001f * 9.80665f;
#elif defined(HAS_QMI8658)
        // QMI8658 at +/-4g has 8192 LSB/g sensitivity
        cached_inertial.ax = (raw_x / 8192.0f) * 9.80665f;
        cached_inertial.ay = (raw_y / 8192.0f) * 9.80665f;
        cached_inertial.az = (raw_z / 8192.0f) * 9.80665f;
#endif
        cached_inertial.valid = true;
    }
}

void SystemHardwareHub::ProcessClimateState(uint64_t now)
{
#if defined(HAS_SHTC3)
    if (!climate_handle)
        return;

    switch (climate_state)
    {
    case ClimateState::Idle:
        // Poll every 10 seconds without blocking the loop
        if (now - last_slow_tick >= 10000000)
        {
            last_slow_tick = now;
            // SHTC3 Wakeup: send 0x35 0x17
            uint8_t wakeup_cmd[] = {0x35, 0x17};
            if (i2c_master_transmit(climate_handle, wakeup_cmd, sizeof(wakeup_cmd), 50) == ESP_OK)
            {
                climate_state = ClimateState::Waking;
                climate_state_timestamp = now;
            }
        }
        break;

    case ClimateState::Waking:
        // SHTC3 requires <= 240 us to wake up; advance after >= 1 ms
        if (now - climate_state_timestamp >= 1000)
        {
            // Start measurement: Normal power, clock stretching disabled, T first (0x78 0x66)
            uint8_t measure_cmd[] = {0x78, 0x66};
            if (i2c_master_transmit(climate_handle, measure_cmd, sizeof(measure_cmd), 50) == ESP_OK)
            {
                climate_state = ClimateState::Measuring;
                climate_state_timestamp = now;
            }
            else
            {
                climate_state = ClimateState::Idle;
            }
        }
        break;

    case ClimateState::Measuring:
        // Measurement takes typ 10.8ms, max 12.1ms; read back after >= 15 ms
        if (now - climate_state_timestamp >= 15000)
        {
            float current_ambient = 0.0f;
            uint8_t buf[6] = {0};
            if (i2c_master_receive(climate_handle, buf, sizeof(buf), 50) == ESP_OK)
            {
                // Sensirion SHTC3 CRC-8: poly 0x31, init 0xFF
                auto checkCRC = [](const uint8_t *data, size_t len, uint8_t checksum) -> bool {
                    uint8_t crc = 0xFF;
                    for (size_t i = 0; i < len; ++i)
                    {
                        crc ^= data[i];
                        for (int bit = 0; bit < 8; ++bit)
                        {
                            if (crc & 0x80)
                                crc = (crc << 1) ^ 0x31;
                            else
                                crc <<= 1;
                        }
                    }
                    return crc == checksum;
                };

                if (checkCRC(&buf[0], 2, buf[2]))
                {
                    uint16_t raw_t = (buf[0] << 8) | buf[1];
                    float tempC = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
                    float tempF = tempC * 1.8f + 32.0f;
                    current_ambient = tempC;
                    g_Values.AmbientTemp = tempF;
                }

                if (checkCRC(&buf[3], 2, buf[5]))
                {
                    uint16_t raw_rh = (buf[3] << 8) | buf[4];
                    float rh = 100.0f * ((float)raw_rh / 65535.0f);
                    g_Values.AmbientHumidity = rh;
                }

                debugV("Climate: Ambient %.1f F, Humidity %.1f%%",
                       g_Values.AmbientTemp.value_or(0.0f), g_Values.AmbientHumidity.value_or(0.0f));
            }

            // Put SHTC3 back to sleep to minimize self-heating (0xB0 0x98)
            uint8_t sleep_cmd[] = {0xB0, 0x98};
            i2c_master_transmit(climate_handle, sleep_cmd, sizeof(sleep_cmd), 50);

            EvaluateThermalMetrics(now, current_ambient);
            climate_state = ClimateState::Idle;
        }
        break;
    }
#elif defined(HAS_BME280)
    if (now - last_slow_tick >= 10000000)
    {
        last_slow_tick = now;
        EvaluateThermalMetrics(now, 24.2f);
    }
#endif
}

void SystemHardwareHub::EvaluateThermalMetrics(uint64_t now, float current_ambient)
{
    float current_die_temp = 0.0f;

    // Die temperature from internal sensor (managed by SensorManager)
    if (g_Values.InternalTemp.has_value())
    {
        current_die_temp = (*g_Values.InternalTemp - 32.0f) * (5.0f / 9.0f);
    }

    // =========================================================
    // CRITICAL MATH BOUNDARY: EVALUATING THERMAL RATE OF RISE
    // =========================================================
    float time_delta_minutes = (float)(now - last_delta_timestamp) / 60000000.0f;
    float rate_of_rise = 0.0f;

    if (last_delta_timestamp > 0 && time_delta_minutes > 0.001f)
    {
        // Calculate velocity of temperature growth (°C per Minute)
        rate_of_rise = (current_die_temp - previous_die_temp) / time_delta_minutes;
    }

    last_delta_timestamp = now;
    previous_die_temp = current_die_temp;

    // Update shared cache blocks
    std::lock_guard<std::mutex> lock(cache_mutex);
    cached_thermal.ambient_temp = current_ambient;
    cached_thermal.die_temp = current_die_temp;
    cached_thermal.die_rate_of_rise = rate_of_rise;

    // EMERGENCY TRIPWIRE: Flag active if absolute temp > 80C OR if we surge > 10°C/minute
    if (current_die_temp > 80.0f || rate_of_rise > 10.0f)
    {
        cached_thermal.thermal_emergency = true;
    }
    else
    {
        cached_thermal.thermal_emergency = false;
    }
    cached_thermal.valid = true;
}

InertialMetrics SystemHardwareHub::GetInertial() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cached_inertial;
}

ThermalMetrics SystemHardwareHub::GetThermal() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    return cached_thermal;
}

namespace
{
inline uint8_t BcdToDec(uint8_t val)
{
    return static_cast<uint8_t>(((val >> 4) * 10) + (val & 0x0F));
}

inline uint8_t DecToBcd(uint8_t val)
{
    return static_cast<uint8_t>(((val / 10) << 4) | (val % 10));
}
} // namespace

bool SystemHardwareHub::ReadRTC(struct tm &timeinfo)
{
#if defined(HAS_PCF85063)
    if (!rtc_handle)
        return false;

    uint8_t start_reg = 0x04; // Seconds register
    uint8_t buf[7] = {0};

    if (i2c_master_transmit_receive(rtc_handle, &start_reg, 1, buf, sizeof(buf), 50) != ESP_OK)
    {
        debugW("RTC: failed to read PCF85063 time registers");
        return false;
    }

    // Bit 7 of seconds register is OS (Oscillator Stop flag).
    // If set, power was lost or oscillator stopped; time integrity is unverified.
    if (buf[0] & 0x80)
    {
        debugW("RTC: oscillator stop flag detected (uninitialized or battery depleted)");
        return false;
    }

    timeinfo.tm_sec = BcdToDec(buf[0] & 0x7F);
    timeinfo.tm_min = BcdToDec(buf[1] & 0x7F);
    timeinfo.tm_hour = BcdToDec(buf[2] & 0x3F);
    timeinfo.tm_mday = BcdToDec(buf[3] & 0x3F);
    timeinfo.tm_wday = buf[4] & 0x07;
    timeinfo.tm_mon = BcdToDec(buf[5] & 0x1F) - 1; // 0-indexed month
    timeinfo.tm_year = BcdToDec(buf[6]) + 100;     // Years since 1900 (e.g. 26 + 100 = 126 for 2026)
    timeinfo.tm_isdst = -1;

    return true;
#else
    return false;
#endif
}

bool SystemHardwareHub::WriteRTC(const struct tm &timeinfo)
{
#if defined(HAS_PCF85063)
    if (!rtc_handle)
        return false;

    // 1. Halt the divider chain by asserting STOP bit (bit 5 in Control_1, 0x00)
    uint8_t stop_cmd[] = {0x00, 0x20};
    if (i2c_master_transmit(rtc_handle, stop_cmd, sizeof(stop_cmd), 50) != ESP_OK)
    {
        debugW("RTC: failed to assert STOP bit");
        return false;
    }

    // 2. Burst write time/calendar registers (0x04..0x0A)
    uint8_t tx_buf[8];
    tx_buf[0] = 0x04; // Starting register address
    tx_buf[1] = DecToBcd(timeinfo.tm_sec) & 0x7F; // Bit 7 (OS) cleared to 0
    tx_buf[2] = DecToBcd(timeinfo.tm_min) & 0x7F;
    tx_buf[3] = DecToBcd(timeinfo.tm_hour) & 0x3F;
    tx_buf[4] = DecToBcd(timeinfo.tm_mday) & 0x3F;
    tx_buf[5] = static_cast<uint8_t>(timeinfo.tm_wday & 0x07);
    tx_buf[6] = DecToBcd(timeinfo.tm_mon + 1) & 0x1F;
    tx_buf[7] = DecToBcd((timeinfo.tm_year >= 100) ? (timeinfo.tm_year - 100) : timeinfo.tm_year);

    if (i2c_master_transmit(rtc_handle, tx_buf, sizeof(tx_buf), 50) != ESP_OK)
    {
        debugW("RTC: failed to write time registers");
        return false;
    }

    // 3. Clear STOP bit (restart clock and reset divider chain)
    uint8_t start_cmd[] = {0x00, 0x00};
    if (i2c_master_transmit(rtc_handle, start_cmd, sizeof(start_cmd), 50) != ESP_OK)
    {
        debugW("RTC: failed to clear STOP bit");
        return false;
    }

    debugI("RTC: updated PCF85063 time to %04d-%02d-%02d %02d:%02d:%02d UTC",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return true;
#else
    return false;
#endif
}

bool SystemHardwareHub::SyncSystemTimeFromRTC()
{
    struct tm timeinfo = {};
    if (!ReadRTC(timeinfo))
        return false;

    time_t t = mktime(&timeinfo);
    if (t < 1700000000) // Sanity check (prior to late 2023 is invalid)
    {
        debugW("RTC: timestamp %lld before valid epoch floor", static_cast<long long>(t));
        return false;
    }

    struct timeval tv = {.tv_sec = t, .tv_usec = 0};
    settimeofday(&tv, nullptr);

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
    debugI("RTC: synchronized system time from PCF85063: %s", time_str);
    return true;
}

bool SystemHardwareHub::SyncRTCTimeFromSystem()
{
    time_t now = time(nullptr);
    if (now < 1700000000)
        return false;

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo); // Store UTC in RTC
    return WriteRTC(timeinfo);
}
#endif

