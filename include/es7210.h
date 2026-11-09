#pragma once

//+--------------------------------------------------------------------------
//
// File:        es7210.h
//
// NightDriverStrip - (c) 2026 Robert Lipe. All Rights Reserved.
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
//    Minimal ES7210 4-channel microphone ADC driver for the Waveshare
//    ESP32-S3-RGB-Matrix. The codec is configured over I2C; the ESP32
//    remains the I2S master and supplies MCLK/BCLK/LRCK.
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <driver/i2c_master.h>

class ES7210
{
  public:
    // 7-bit I2C address. The ES7210 datasheet/driver header quotes 0x80 as an
    // 8-bit write address; the ESP-IDF master API wants the 7-bit value.
    static constexpr uint8_t kDefaultAddress = 0x40;

    // Bitmask matching the ES7210 input select register.
    static constexpr uint8_t kMic1 = 0x01;
    static constexpr uint8_t kMic2 = 0x02;
    static constexpr uint8_t kMic3 = 0x04;
    static constexpr uint8_t kMic4 = 0x08;

    ES7210() = default;

    // Attach to an already-created I2C master bus. Returns false if the codec
    // does not respond. Safe to call more than once; returns true if already
    // attached.
    bool Begin(i2c_master_bus_handle_t bus, uint8_t address = kDefaultAddress);

    // Run the full reset/init/enable sequence. The I2S clocks must already be
    // running before this is called. Only 16-bit stereo I2S is currently used.
    bool Configure(uint8_t microphoneMask = kMic1 | kMic2, float gainDb = 37.5f);

    bool IsPresent() const
    {
        return _handle != nullptr;
    }

    bool WriteRegister(uint8_t reg, uint8_t value);
    bool ReadRegister(uint8_t reg, uint8_t &value);
    bool UpdateRegister(uint8_t reg, uint8_t mask, uint8_t value);

    // Change the analog PGA gain on the selected microphones (0..37.5 dB in
    // 3 dB steps, plus 34.5/36/37.5 dB).
    bool SetGain(uint8_t microphoneMask, float gainDb);

  private:
    i2c_master_dev_handle_t _handle = nullptr;
    uint8_t _clockOffValue = 0x3F;

    bool SelectMicrophones(uint8_t microphoneMask, float gainDb);
    bool Start(uint8_t microphoneMask, float gainDb);
};
