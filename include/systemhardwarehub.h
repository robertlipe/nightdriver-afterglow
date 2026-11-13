#pragma once

#include "globals.h"

#include <cstdint>
#include <ctime>
#include <driver/i2c_master.h>
#include <mutex>

struct InertialMetrics
{
    float ax, ay, az;
    bool valid;
};

struct ThermalMetrics
{
    float ambient_temp;     // From external I2C (BME280 or SHTC3)
    float die_temp;         // From internal ESP32-C6 silicon junction
    float die_rate_of_rise; // °C shifted over the last window
    bool thermal_emergency; // Urgent flag: drop load / dim LEDs immediately!
    bool valid;
};

#if WAVESHARE_ESP32_S3_RGB_MATRIX || MATRIX_S3
#define HAS_SYSTEM_HARDWARE_HUB 1
#else
#define HAS_SYSTEM_HARDWARE_HUB 0
#endif

#if HAS_SYSTEM_HARDWARE_HUB
class SystemHardwareHub
{
  public:
    static SystemHardwareHub &Instance();

    // Creates the shared I2C master bus on first call. Safe to call repeatedly.
    bool Begin();
    void Tick(); // Non-blocking: drives register reads & delta calculations

    InertialMetrics GetInertial() const;
    ThermalMetrics GetThermal() const;

    // Real-Time Clock (PCF85063) operations
    bool ReadRTC(struct tm &timeinfo);
    bool WriteRTC(const struct tm &timeinfo);
    bool SyncSystemTimeFromRTC();
    bool SyncRTCTimeFromSystem();

    // The one I2C bus for all on-board peripherals. Codec drivers and future
    // sensors must attach here rather than creating a second master bus.
    i2c_master_bus_handle_t GetI2CBus();

    // Attach a device to the shared bus. Returns nullptr on failure. The caller
    // owns the handle and is responsible for any required cleanup at teardown.
    i2c_master_dev_handle_t AddDevice(uint8_t address, uint32_t sclSpeedHz = 400000);

  private:
    SystemHardwareHub() = default;
    ~SystemHardwareHub() = default;

    i2c_master_bus_handle_t bus_handle = nullptr;
    i2c_master_dev_handle_t sensor_handle = nullptr;
    i2c_master_dev_handle_t climate_handle = nullptr;
    i2c_master_dev_handle_t rtc_handle = nullptr;

    void ScanI2CBus();
    void PollInertial();

    mutable std::mutex cache_mutex;
    InertialMetrics cached_inertial{0, 0, 0, false};
    ThermalMetrics cached_thermal{0, 0, 0, false, false};

    uint64_t last_fast_tick = 0;
    uint64_t last_slow_tick = 0;

    enum class ClimateState
    {
        Idle,
        Waking,
        Measuring
    };
    ClimateState climate_state = ClimateState::Idle;
    uint64_t climate_state_timestamp = 0;

    // Thermal tracking states for delta measurements
    uint64_t last_delta_timestamp = 0;
    float previous_die_temp = 0.0f;

    void ConfigureSensorRegisters();
    void ProcessClimateState(uint64_t now);
    void EvaluateThermalMetrics(uint64_t now, float current_ambient);
};
#else
class SystemHardwareHub
{
  public:
    static SystemHardwareHub &Instance()
    {
        static SystemHardwareHub instance;
        return instance;
    }

    bool Begin() { return false; }
    void Tick() {}

    InertialMetrics GetInertial() const { return {}; }
    ThermalMetrics GetThermal() const { return {}; }

    bool ReadRTC(struct tm &) { return false; }
    bool WriteRTC(const struct tm &) { return false; }
    bool SyncSystemTimeFromRTC() { return false; }
    bool SyncRTCTimeFromSystem() { return false; }

    i2c_master_bus_handle_t GetI2CBus() { return nullptr; }
    i2c_master_dev_handle_t AddDevice(uint8_t, uint32_t = 400000) { return nullptr; }

  private:
    SystemHardwareHub() = default;
    ~SystemHardwareHub() = default;
};
#endif

