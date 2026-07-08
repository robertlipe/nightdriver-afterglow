#pragma once

//+--------------------------------------------------------------------------
//
// File:        ws281xgfx.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
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
//
// Description:
//
//   Provides an Adafruit_GFX implementation for our RGB LED panel so that
//   we can use primitives such as lines and fills on it.
//
// History:     Oct-9-2018         Davepl      Created from other projects
//---------------------------------------------------------------------------

#include "globals.h"

#include "gfxbase.h"

// WS281xGfx
//
// A derivation of GFXBase that adds LED-strip-specific functionality

class WS281xGFX : public GFXBase
{
protected:
    static void AddLEDs(std::vector<std::shared_ptr<GFXBase>>& devices);

public:

    WS281xGFX(size_t w, size_t h);

    ~WS281xGFX() override;

    static void InitializeHardware(std::vector<std::shared_ptr<GFXBase>>& devices);

    // PostProcessFrame
    //
    // PostProcessFrame sends the data to the LED strip.  If it's fewer than the size of the strip, we only send that many.

    void PostProcessFrame(uint16_t localPixelsDrawn, uint16_t wifiPixelsDrawn) override;
};


