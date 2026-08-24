#pragma once
//+--------------------------------------------------------------------------
//
// File:        BouncingBallEffect.h
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
//    Draws bouncing balls using a kinematics formula
//
// History:     Apr-17-2019         Davepl      Adapted from NightDriver
//
//---------------------------------------------------------------------------


#include "effects.h"
#include "values.h"
#include "random_utils.h"

// BouncingBallEffect
//
// Draws a set of N bouncing balls using a simple little kinematics formula.  Clears the section first.

static constexpr std::array ballColors =
{
    CRGB::Green,
    CRGB::Red,
    CRGB::Blue,
    CRGB::Orange,
    CRGB::Purple,
    CRGB::Yellow,
    CRGB::Indigo,
};

class BouncingBallEffect : public EffectWithId<BouncingBallEffect>
{
  private:

    size_t  _iOffset;
    size_t  _cLength;
    size_t  _cBalls;
    size_t  _cBallSize;
    bool    _bMirrored;
    int     _speed = 30;

    const bool _bErase;

    DECLARE_EFFECT_SETTING_SPECS(mySettingSpecs);

    static constexpr float Gravity = -9.81f;
    static constexpr float StartHeight = 1.0f;
    // Note: VSCode flags sqrtf() for calling a non-constexpr builtin function, but it compiles and runs
    static inline const float ImpactVelocityStart = sqrtf(-2.0f * Gravity * StartHeight);

    std::vector<double> ClockTimeSinceLastBounce;
    std::vector<double> TimeSinceLastBounce;
    std::vector<float>  Height;
    std::vector<float>  ImpactVelocity;
    std::vector<float>  Dampening;
    std::vector<CRGB>   Colors;

  public:

    BouncingBallEffect(size_t ballCount = 3, bool bMirrored = true, bool bErase = false, int ballSize = 5)
        : EffectWithId<BouncingBallEffect>("Bouncing Balls"),
          _cBalls(ballCount),
          _cBallSize(ballSize),
          _bMirrored(bMirrored),
          _bErase(bErase)
    {
    }

    explicit BouncingBallEffect(const JsonObjectConst& jsonObject)
        : EffectWithId<BouncingBallEffect>(jsonObject),
          _cBalls(jsonObject["blc"]),
          _cBallSize(jsonObject["bls"]),
          _bMirrored(jsonObject[PTY_MIRORRED]),
          _bErase(jsonObject[PTY_ERASE])
    {
        if (jsonObject[PTY_SPEED].is<int>()) _speed = jsonObject[PTY_SPEED].as<int>();
    }

    EffectSettingSpecs* FillSettingSpecs() override
    {
        if (mySettingSpecs.size() == 0)
        {
            mySettingSpecs.emplace_back(PTY_SPEED, "Speed", SettingSpec::SettingType::Integer, 1.0, 100.0);
        }
        return &mySettingSpecs;
    }

    bool SetSetting(const String& name, const String& value) override
    {
        RETURN_IF_SET(name, PTY_SPEED, _speed, value);
        return LEDStripEffect::SetSetting(name, value);
    }

    bool SerializeToJSON(JsonObject& jsonObject) override
    {
        auto jsonDoc = CreateJsonDocument();

        JsonObject root = jsonDoc.to<JsonObject>();
        LEDStripEffect::SerializeToJSON(root);

        jsonDoc["blc"] = _cBalls;
        jsonDoc["bls"] = _cBallSize;
        jsonDoc[PTY_MIRORRED] = _bMirrored;
        jsonDoc[PTY_ERASE] = _bErase;
        jsonDoc[PTY_SPEED] = _speed;

        return SetIfNotOverflowed(jsonDoc, jsonObject, __PRETTY_FUNCTION__);
    }

    size_t DesiredFramesPerSecond() const override
    {
        return 61;
    }

    bool Init(std::vector<std::shared_ptr<GFXBase>>& gfx) override
    {
        if (!LEDStripEffect::Init(gfx))
            return false;

        _cLength = gfx[0]->GetLEDCount();

        ClockTimeSinceLastBounce.resize(_cBalls);
        TimeSinceLastBounce.resize(_cBalls);
        Height.resize(_cBalls);
        ImpactVelocity.resize(_cBalls);
        Dampening.resize(_cBalls);
        Colors.resize(_cBalls);

        for (size_t i = 0; i < _cBalls; i++)
        {
            Height[i]                   = StartHeight;
            ImpactVelocity[i]           = ImpactVelocityStart;
            ClockTimeSinceLastBounce[i] = g_Values.AppTime.FrameStartTime();
            Dampening[i]                = 1.0f - i / powf(_cBalls, 2);               // Was 0.9
            TimeSinceLastBounce[i]      = 0;
            Colors[i]                   = ballColors[i % std::size(ballColors)];
        }
        return true;
    }

    // Draw
    //
    // Draw each of the balls.  When any ball gets too little energy it would just sit at the base so it is re-kicked with new energy.#pragma endregion

    void Draw() override
    {
        // Erase the drawing area
        if (_bErase)
        {
            setAllOnAllChannels(0,0,0);
        }
        else
        {
            for (int j = 0; j<_cLength; j++)                            // fade brightness all LEDs one step
                if (random_range(0, 10)>5)
                    fadePixelToBlackOnAllChannelsBy(j, 50);
        }

        // Draw each of the the balls
        const float timeScale = _speed / 300.0f;
        for (size_t i = 0; i < _cBalls; i++)
        {
            TimeSinceLastBounce[i] = (g_Values.AppTime.FrameStartTime() - ClockTimeSinceLastBounce[i]) * timeScale;
            Height[i] = 0.5f * Gravity * powf(TimeSinceLastBounce[i], 2.0f) + ImpactVelocity[i] * TimeSinceLastBounce[i];

            if (Height[i] < 0)
            {
                Height[i] = 0;
                ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
                ClockTimeSinceLastBounce[i] = g_Values.AppTime.FrameStartTime();

                if (ImpactVelocity[i] < 0.5f * ImpactVelocityStart)                                    // Was .01 and not multiplied by anything
                    ImpactVelocity[i] = ImpactVelocityStart;
            }

            float position = Height[i] * (_cLength - 1) / StartHeight;
            setPixelsOnAllChannels(position, _cBallSize, Colors[i % std::size(ballColors)]);
            if (_bMirrored)
                setPixelsOnAllChannels(_cLength-1-position, _cBallSize, Colors[i % std::size(ballColors)], true);
        }
    }
};
