//+--------------------------------------------------------------------------
//
// File:        es7210.cpp
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
//    ES7210 microphone ADC configuration. The register sequence mirrors the
//    slave-mode, 2-microphone, 16-bit I2S path in Espressif's esp_codec_dev
//    driver (the same path the Waveshare BSP uses), but without pulling in the
//    whole codec framework for one device.
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <algorithm>

#include "es7210.h"

namespace
{
    constexpr int kI2CTimeoutMs = 100;

    constexpr uint8_t kRegReset         = 0x00;
    constexpr uint8_t kRegClockOff      = 0x01;
    constexpr uint8_t kRegMainClock     = 0x02;
    constexpr uint8_t kRegLrckDivH      = 0x04;
    constexpr uint8_t kRegLrckDivL      = 0x05;
    constexpr uint8_t kRegPowerDown     = 0x06;
    constexpr uint8_t kRegOsr           = 0x07;
    constexpr uint8_t kRegModeConfig    = 0x08;
    constexpr uint8_t kRegTimeControl0  = 0x09;
    constexpr uint8_t kRegTimeControl1  = 0x0A;
    constexpr uint8_t kRegSdpInterface1 = 0x11;
    constexpr uint8_t kRegSdpInterface2 = 0x12;
    constexpr uint8_t kRegAdc34Mute     = 0x14;
    constexpr uint8_t kRegAdc12Mute     = 0x15;
    constexpr uint8_t kRegAdc34Hpf2     = 0x20;
    constexpr uint8_t kRegAdc34Hpf1     = 0x21;
    constexpr uint8_t kRegAdc12Hpf1     = 0x22;
    constexpr uint8_t kRegAdc12Hpf2     = 0x23;
    constexpr uint8_t kRegAnalog        = 0x40;
    constexpr uint8_t kRegMic12Bias     = 0x41;
    constexpr uint8_t kRegMic34Bias     = 0x42;
    constexpr uint8_t kRegMic1Gain      = 0x43;
    constexpr uint8_t kRegMic2Gain      = 0x44;
    constexpr uint8_t kRegMic3Gain      = 0x45;
    constexpr uint8_t kRegMic4Gain      = 0x46;
    constexpr uint8_t kRegMic1Power     = 0x47;
    constexpr uint8_t kRegMic2Power     = 0x48;
    constexpr uint8_t kRegMic3Power     = 0x49;
    constexpr uint8_t kRegMic4Power     = 0x4A;
    constexpr uint8_t kRegMic12Power    = 0x4B;
    constexpr uint8_t kRegMic34Power    = 0x4C;

    // Convert dB to the ES7210's 0..14 PGA gain code. Codes above 11 are the
    // odd 34.5/36/37.5 dB steps; everything else is a multiple of 3 dB.
    uint8_t GainCode(float db)
    {
        if (db >= 37.5f)
            return 14;
        if (db >= 36.0f)
            return 13;
        if (db >= 34.5f)
            return 12;
        if (db <= 0.0f)
            return 0;

        const int step = static_cast<int>((db + 1.5f) / 3.0f);
        return static_cast<uint8_t>(std::clamp(step, 0, 11));
    }

    int MicrophoneCount(uint8_t mask)
    {
        int count = 0;
        for (int i = 0; i < 4; ++i)
            if (mask & (1U << i))
                ++count;
        return count;
    }
}

bool ES7210::Begin(i2c_master_bus_handle_t bus, uint8_t address)
{
    if (_handle)
        return true;
    if (!bus)
        return false;

    if (i2c_master_probe(bus, address, kI2CTimeoutMs) != ESP_OK)
    {
        debugE("ES7210: no response at I2C address 0x%02X", address);
        return false;
    }

    i2c_device_config_t deviceConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 400000,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = 0},
    };

    if (i2c_master_bus_add_device(bus, &deviceConfig, &_handle) != ESP_OK)
    {
        _handle = nullptr;
        debugE("ES7210: failed to add I2C device at 0x%02X", address);
        return false;
    }

    debugI("ES7210: attached at I2C address 0x%02X", address);
    return true;
}

bool ES7210::WriteRegister(uint8_t reg, uint8_t value)
{
    if (!_handle)
        return false;

    const uint8_t buffer[2] = {reg, value};
    if (i2c_master_transmit(_handle, buffer, sizeof(buffer), kI2CTimeoutMs) != ESP_OK)
    {
        debugE("ES7210: write 0x%02X=0x%02X failed", reg, value);
        return false;
    }
    return true;
}

bool ES7210::ReadRegister(uint8_t reg, uint8_t &value)
{
    if (!_handle)
        return false;

    if (i2c_master_transmit_receive(_handle, &reg, 1, &value, 1, kI2CTimeoutMs) != ESP_OK)
    {
        debugE("ES7210: read 0x%02X failed", reg);
        return false;
    }
    return true;
}

bool ES7210::UpdateRegister(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current = 0;
    if (!ReadRegister(reg, current))
        return false;
    current = static_cast<uint8_t>((current & ~mask) | (mask & value));
    return WriteRegister(reg, current);
}

bool ES7210::SetGain(uint8_t microphoneMask, float gainDb)
{
    const uint8_t code = GainCode(gainDb);
    bool ok = true;

    for (int i = 0; i < 4; ++i)
    {
        if (microphoneMask & (1U << i))
            ok = UpdateRegister(kRegMic1Gain + i, 0x0F, code) && ok;
    }
    return ok;
}

bool ES7210::SelectMicrophones(uint8_t microphoneMask, float gainDb)
{
    // Mask off unsupported bits so a bad constant cannot enable nothing.
    microphoneMask &= (kMic1 | kMic2 | kMic3 | kMic4);
    if (microphoneMask == 0)
    {
        debugE("ES7210: no microphones selected");
        return false;
    }

    bool ok = true;

    // Start with every mic path off, then power only the requested ones.
    for (int i = 0; i < 4; ++i)
        ok = UpdateRegister(kRegMic1Gain + i, 0x10, 0x00) && ok;

    ok = WriteRegister(kRegMic12Power, 0xFF) && ok;
    ok = WriteRegister(kRegMic34Power, 0xFF) && ok;

    if (microphoneMask & kMic1)
    {
        ok = UpdateRegister(kRegClockOff, 0x0B, 0x00) && ok;
        ok = WriteRegister(kRegMic12Power, 0x00) && ok;
        ok = UpdateRegister(kRegMic1Gain, 0x10, 0x10) && ok;
    }
    if (microphoneMask & kMic2)
    {
        ok = UpdateRegister(kRegClockOff, 0x0B, 0x00) && ok;
        ok = WriteRegister(kRegMic12Power, 0x00) && ok;
        ok = UpdateRegister(kRegMic2Gain, 0x10, 0x10) && ok;
    }
    if (microphoneMask & kMic3)
    {
        ok = UpdateRegister(kRegClockOff, 0x15, 0x00) && ok;
        ok = WriteRegister(kRegMic34Power, 0x00) && ok;
        ok = UpdateRegister(kRegMic3Gain, 0x10, 0x10) && ok;
    }
    if (microphoneMask & kMic4)
    {
        ok = UpdateRegister(kRegClockOff, 0x15, 0x00) && ok;
        ok = WriteRegister(kRegMic34Power, 0x00) && ok;
        ok = UpdateRegister(kRegMic4Gain, 0x10, 0x10) && ok;
    }

    // Three or more mics have to be packed into TDM slots; two mics use the
    // normal left/right I2S frame this driver is built around.
    ok = WriteRegister(kRegSdpInterface2, MicrophoneCount(microphoneMask) >= 3 ? 0x02 : 0x00) && ok;
    ok = SetGain(microphoneMask, gainDb) && ok;

    return ok;
}

bool ES7210::Start(uint8_t microphoneMask, float gainDb)
{
    bool ok = true;

    ok = WriteRegister(kRegClockOff, _clockOffValue) && ok;
    ok = WriteRegister(kRegPowerDown, 0x00) && ok;
    ok = WriteRegister(kRegAnalog, 0x43) && ok;
    ok = WriteRegister(kRegMic1Power, 0x08) && ok;
    ok = WriteRegister(kRegMic2Power, 0x08) && ok;
    ok = WriteRegister(kRegMic3Power, 0x08) && ok;
    ok = WriteRegister(kRegMic4Power, 0x08) && ok;
    ok = SelectMicrophones(microphoneMask, gainDb) && ok;
    ok = WriteRegister(kRegAnalog, 0x43) && ok;
    ok = WriteRegister(kRegReset, 0x71) && ok;
    ok = WriteRegister(kRegReset, 0x41) && ok;

    // Unmute ADC1/ADC2 (and ADC3/ADC4 harmlessly).
    ok = UpdateRegister(kRegAdc12Mute, 0x03, 0x00) && ok;
    ok = UpdateRegister(kRegAdc34Mute, 0x03, 0x00) && ok;

    return ok;
}

bool ES7210::Configure(uint8_t microphoneMask, float gainDb)
{
    if (!_handle)
    {
        debugE("ES7210: Configure() called before Begin()");
        return false;
    }

    bool ok = true;

    // Soft reset, then settle the chip's state machines.
    ok = WriteRegister(kRegReset, 0xFF) && ok;
    delay(10);
    ok = WriteRegister(kRegReset, 0x41) && ok;
    ok = WriteRegister(kRegClockOff, 0x3F) && ok;
    ok = WriteRegister(kRegTimeControl0, 0x30) && ok;
    ok = WriteRegister(kRegTimeControl1, 0x30) && ok;
    ok = WriteRegister(kRegAdc12Hpf2, 0x2A) && ok;
    ok = WriteRegister(kRegAdc12Hpf1, 0x0A) && ok;
    ok = WriteRegister(kRegAdc34Hpf2, 0x0A) && ok;
    ok = WriteRegister(kRegAdc34Hpf1, 0x2A) && ok;

    // The ESP32 is the I2S master, so the ES7210 is a clock slave.
    ok = UpdateRegister(kRegModeConfig, 0x01, 0x00) && ok;

    // Analog power, 2.87 V mic bias, VMID, and the standard clock setup used
    // by the vendor driver. In slave mode the ESP32's MCLK drives everything;
    // the LRCK divider registers are only consulted in master mode.
    ok = WriteRegister(kRegAnalog, 0x43) && ok;
    ok = WriteRegister(kRegMic12Bias, 0x70) && ok;
    ok = WriteRegister(kRegMic34Bias, 0x70) && ok;
    ok = WriteRegister(kRegOsr, 0x20) && ok;
    ok = WriteRegister(kRegMainClock, 0xC1) && ok;
    ok = WriteRegister(kRegLrckDivH, 0x00) && ok;
    ok = WriteRegister(kRegLrckDivL, 0x00) && ok;

    ok = SelectMicrophones(microphoneMask, gainDb) && ok;

    // 16-bit samples in standard I2S format. (Reg 0x11 bits 6:5 = 0b11 for
    // 16-bit; bits 1:0 = 0b00 for Philips I2S.)
    uint8_t interface = 0;
    ok = ReadRegister(kRegSdpInterface1, interface) && ok;
    interface = static_cast<uint8_t>((interface & 0x1F) | 0x60);
    interface &= 0xFC;
    ok = WriteRegister(kRegSdpInterface1, interface) && ok;

    // Remember the post-select clock gate value; Start() restores it.
    ok = ReadRegister(kRegClockOff, _clockOffValue) && ok;
    ok = Start(microphoneMask, gainDb) && ok;

    if (ok)
        debugI("ES7210: configured 16-bit I2S, mic mask 0x%02X, gain %.1f dB", microphoneMask, gainDb);
    else
        debugE("ES7210: configuration sequence failed");

    return ok;
}
