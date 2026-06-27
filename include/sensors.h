#pragma once

#include "globals.h"
#include <mutex>
#include <stdint.h>
#include "driver/i2c_master.h"

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

    bool Begin();
    void Tick(); // Non-blocking: drives register reads & delta calculations

    InertialMetrics GetInertial() const;
    ThermalMetrics GetThermal() const;

  private:
    SystemHardwareHub() = default;
    ~SystemHardwareHub() = default;

    i2c_master_bus_handle_t bus_handle = nullptr;
    i2c_master_dev_handle_t sensor_handle = nullptr;

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

//+--------------------------------------------------------------------------
//
// File:        sensors.h
//
// NightDriverStrip - (c) 2026 Robert Lipe.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
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

#include "globals.h"

class SensorManager
{
public:
    static void begin();
    static void Update();

private:
#ifdef DHT11_PIN
    static bool ReadDHT11(int pin, float& tempF, float& humidity);
#endif
};
