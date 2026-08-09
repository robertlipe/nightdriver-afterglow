#pragma once

//+--------------------------------------------------------------------------
//
// File:        interfaces.h
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
//    Common interfaces and specification structures to decouple core
//    logic from heavy system headers.
//
//---------------------------------------------------------------------------

#include <ArduinoJson.h>
#include <memory>
#include <optional>
#include <type_traits>



struct IJSONSerializable
{
    virtual bool SerializeToJSON(JsonObject& jsonObject) = 0;
    virtual bool DeserializeFromJSON(const JsonObjectConst& jsonObject) { return false; }
    virtual ~IJSONSerializable() = default;
};

struct SettingSpec
{
    // Note that if this enum is expanded, TypeName() must be also!
    enum class SettingType : int
    {
        Integer,
        PositiveBigInteger,
        Float,
        Boolean,
        String,
        Palette,
        Color,
        Slider
    };

    enum class SettingAccess : char
    {
        ReadOnly,
        WriteOnly,
        ReadWrite
    };

    // "Technical" name of the setting, as in the (JSON) property it is stored in.
    const char* Name{};

    // "Friendly" name of the setting, as in the one to be presented to the user in a user interface.
    const char* FriendlyName{};

    // Description of the purpose and/or value of the setting
    const char* Description{};

    // Value type of the setting
    SettingType Type;

    // Indication if validation for the setting's value is available
    bool HasValidation = false;

    // Indication if a setting is read-only, write-only or read/write
    SettingAccess Access = SettingAccess::ReadWrite;

    // Indication if an empty value is allowed for the setting. This only applies to String settings.
    std::optional<bool> EmptyAllowed = {};

    // Minimum valid value for the setting. This only applies to numeric settings.
    std::optional<double> MinimumValue = {};

    // Maximum valid value for the setting. This only applies to numeric settings.
    std::optional<double> MaximumValue = {};

    // Finishes the initialization of the spec, and then validates the consistency of its overall contents.
    // Note that it does the latter quite rudely: it uses assert() on things it feels should be in order.
    // This function is called by this struct's constructors that initialize values, but this being a struct
    // allows itself to be called from the outside as well.
    void FinishAndValidateInitialization();

    SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type);

    SettingSpec(const char* name, const char* friendlyName, SettingType type);

    // Constructor that sets both minimum and maximum values
    SettingSpec(const char* name, const char* friendlyName, const char* description, SettingType type, double min, double max);

    // Constructor that sets both minimum and maximum values
    SettingSpec(const char* name, const char* friendlyName, SettingType type, double min, double max);

    SettingSpec() = default;
    virtual ~SettingSpec() = default;

    virtual String TypeName() const;
};
