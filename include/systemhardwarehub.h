#pragma once

#include "globals.h"

#include <cstdint>
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

class SystemHardwareHub
{
  public:
    static SystemHardwareHub &Instance();

    // Creates the shared I2C master bus on first call. Safe to call repeatedly.
    bool Begin();
    void Tick(); // Non-blocking: drives register reads & delta calculations

    InertialMetrics GetInertial() const;
    ThermalMetrics GetThermal() const;

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

    void ScanI2CBus();
    void PollInertial();

    mutable std::mutex cache_mutex;
    InertialMetrics cached_inertial{0, 0, 0, false};
    ThermalMetrics cached_thermal{0, 0, 0, false, false};

    uint64_t last_fast_tick = 0;
    uint64_t last_slow_tick = 0;

    // Thermal tracking states for delta measurements
    uint64_t last_delta_timestamp = 0;
    float previous_die_temp = 0.0f;

    void ConfigureSensorRegisters();
    void PollThermal();
};

