#pragma once

//+--------------------------------------------------------------------------
//
// File:        effectsupport.h
//
// NightDriverStrip - (c) 2023 Plummer's Software LLC.  All Rights Reserved.
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
// Description:
//
//    Support functions and definitions for LED effects.
//
// History:     May-23-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"
#include "interfaces.h"

#include <vector>

// Forward declarations
class LEDStripEffect;
class EffectManager;

// RegisterAll
//
// Template helper that registers all effects in a list with an EffectManager.
// The list must be a collection of types that can be instantiated with an
// EffectManager and a DeviceConfig.
template<typename... T>
void RegisterAll(EffectManager& manager, const std::shared_ptr<GFXBase>& device)
{
    (manager.RegisterEffect<T>(device), ...);
}

// RegisterAll
//
// Template helper that registers all effects in a list with an EffectManager
// for all devices in a collection.
template<typename... T>
void RegisterAll(EffectManager& manager, const std::vector<std::shared_ptr<GFXBase>>& devices)
{
    for (auto& device : devices)
        RegisterAll<T...>(manager, device);
}

// Global Palettes
extern const CRGBPalette16 USAColors_p;
extern const CRGBPalette16 rainbowPalette;
extern const CRGBPalette16 spectrumBasicColors;

// Defines used by some StarEffect instances

constexpr float kStarEffectProbability = 1.0f;
constexpr float kStarEffectMusicFactor = 1.0f;
