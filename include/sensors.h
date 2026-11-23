#pragma once

#include "globals.h"


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

#include <optional>

enum class ThermalState
{
    Normal,
    Warning,
    Critical
};

class SensorManager
{
public:
    static void begin();
    static void Update();

    // Evaluates un-calibrated sensor temperatures against thermal limits and rate of climb.
    static void EvaluateThermal(std::optional<float> rawAmbientTempF, std::optional<float> rawDieTempF);

    static ThermalState GetThermalState();
    static const char* GetThermalStateName();
    static float GetAmbientRiseRate();
    static float GetDieRiseRate();
    static std::optional<float> GetRawAmbientTemp();

private:
#ifdef DHT11_PIN
    static bool ReadDHT11(int pin, float& tempF, float& humidity);
#endif
};
