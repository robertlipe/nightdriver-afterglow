#pragma once

//+--------------------------------------------------------------------------
//
// File:        deviceconfig.h
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
// Description:
//
//    Abstraction layer for getting and setting persistent device configuration,
//    as stored in non-volatile storage (NVS)
//
// History:     May-15-2023         Rbergen      Created
//
//---------------------------------------------------------------------------

#include "globals.h"
#include "interfaces.h"

#include <ArduinoJson.h>
#include <map>
#include <memory>
#include <nvs.h>
#include <vector>

class DeviceConfig
{
  public:
    enum class ValidateResponse : uint8_t
    {
        Valid,
        Invalid,
        InvalidValueType
    };

    struct ValidationResult
    {
        ValidateResponse Response;
        String Message;
    };

  private:
    std::vector<SettingSpec, psram_allocator<SettingSpec>> settingSpecs;

    String hostname;
    uint32_t currentEffectIndex;
    String currentPaletteName;
    uint32_t currentPaletteIndex;
    uint32_t effectInterval;
    uint32_t brightness;
    bool effectsEnabled;
    bool lockEffect;
    bool randomizeEffect;
    bool randomizePalette;
    bool ntpEnabled;

    // Persist a single string setting to NVS
    bool PersistStringSetting(const char* key, const String& value);

    // Persist a single numeric setting to NVS
    template<typename T, std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
    bool PersistNumericSetting(const char* key, T value)
    {
        nvs_handle_t nvsRWHandle;
        if (nvs_open("storage", NVS_READWRITE, &nvsRWHandle) != ESP_OK)
            return false;

        bool success = false;
        if (std::is_same<T, bool>::value)
            success = nvs_set_u8(nvsRWHandle, key, static_cast<uint8_t>(value)) == ESP_OK;
        else if (std::is_floating_point<T>::value)
        {
            // NVS doesn't support floats, so we'll have to store them as strings
            String strValue = String(value, 6);
            success = nvs_set_str(nvsRWHandle, key, strValue.c_str()) == ESP_OK;
        }
        else if (std::is_signed<T>::value)
            success = nvs_set_i32(nvsRWHandle, key, static_cast<int32_t>(value)) == ESP_OK;
        else
            success = nvs_set_u32(nvsRWHandle, key, static_cast<uint32_t>(value)) == ESP_OK;

        if (success)
            nvs_commit(nvsRWHandle);

        nvs_close(nvsRWHandle);
        return success;
    }

  public:
    DeviceConfig();

    const std::vector<SettingSpec, psram_allocator<SettingSpec>>& GetSettingSpecs() const { return settingSpecs; }

    // Load configuration from NVS
    void Load();

    // Getters
    const String& GetHostname() const { return hostname; }
    uint32_t GetCurrentEffectIndex() const { return currentEffectIndex; }
    const String& GetCurrentPaletteName() const { return currentPaletteName; }
    uint32_t GetCurrentPaletteIndex() const { return currentPaletteIndex; }
    uint32_t GetEffectInterval() const { return effectInterval; }
    uint32_t GetBrightness() const { return brightness; }
    bool GetEffectsEnabled() const { return effectsEnabled; }
    bool GetLockEffect() const { return lockEffect; }
    bool GetRandomizeEffect() const { return randomizeEffect; }
    bool GetRandomizePalette() const { return randomizePalette; }
    bool GetNtpEnabled() const { return ntpEnabled; }

    // Setters (with persistence)
    bool SetHostname(const String& value);
    bool SetCurrentEffectIndex(uint32_t value);
    bool SetCurrentPaletteName(const String& value);
    bool SetCurrentPaletteIndex(uint32_t value);
    bool SetEffectInterval(uint32_t value);
    bool SetBrightness(uint32_t value);
    bool SetEffectsEnabled(bool value);
    bool SetLockEffect(bool value);
    bool SetRandomizeEffect(bool value);
    bool SetRandomizePalette(bool value);
    bool SetNtpEnabled(bool value);

    // Validate a setting value against its spec
    ValidationResult ValidateSetting(const String& name, const String& value) const;

    // Apply a setting by name
    bool ApplySetting(const String& name, const String& value);
};

// Global instance of DeviceConfig
extern std::unique_ptr<DeviceConfig> g_ptrDeviceConfig;

// Global settings class
class CGlobalSettings : public IJSONSerializable
{
  private:
    static std::vector<SettingSpec, psram_allocator<SettingSpec>> settingSpecs;

  public:
    const std::vector<SettingSpec, psram_allocator<SettingSpec>>& GetSettingSpecs() const { return settingSpecs; }

    bool SerializeToJSON(JsonObject& jsonObject) override;
    bool DeserializeFromJSON(const JsonObjectConst& jsonObject) override;

    // Update global settings based on the current state of the system
    void UpdateFromSystem();

    // Apply global settings to the system
    void ApplyToSystem();
};
