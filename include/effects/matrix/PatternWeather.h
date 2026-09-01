#pragma once


//+--------------------------------------------------------------------------
//
// File:        PatternWeather.h
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
//   Gets the weather for a given zip code
//
// History:     Jun-25-202          Davepl          Adapted from own code
//              Nov-15-2023         mggates         Updated to use better weather icons
//
//---------------------------------------------------------------------------

#ifndef PatternWeather_H
#define PatternWeather_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <HTTPClient.h>
#include <map>
#include <mutex>
#include <thread>
#include <UrlEncode.h>

#include "effects.h"

#include "systemcontainer.h"
#include "TJpg_Decoder.h"

#include "types.h"

// Use centralized Apple5x7 font across all targets
extern const GFXfont Apple5x7 PROGMEM;

static constexpr auto WEATHER_INTERVAL = std::chrono::seconds(600);
static constexpr uint32_t WEATHER_CHECK_WIFI_WAIT = 5000;

extern const uint8_t brokenclouds_start[]           asm("_binary_assets_bmp_brokenclouds_jpg_start");
extern const uint8_t brokenclouds_end[]             asm("_binary_assets_bmp_brokenclouds_jpg_end");
extern const uint8_t brokenclouds_night_start[]     asm("_binary_assets_bmp_brokencloudsnight_jpg_start");
extern const uint8_t brokenclouds_night_end[]       asm("_binary_assets_bmp_brokencloudsnight_jpg_end");
extern const uint8_t clearsky_start[]               asm("_binary_assets_bmp_clearsky_jpg_start");
extern const uint8_t clearsky_end[]                 asm("_binary_assets_bmp_clearsky_jpg_end");
extern const uint8_t clearsky_night_start[]         asm("_binary_assets_bmp_clearnight_jpg_start");
extern const uint8_t clearsky_night_end[]           asm("_binary_assets_bmp_clearnight_jpg_end");
extern const uint8_t fewclouds_start[]              asm("_binary_assets_bmp_fewclouds_jpg_start");
extern const uint8_t fewclouds_end[]                asm("_binary_assets_bmp_fewclouds_jpg_end");
extern const uint8_t fewclouds_night_start[]        asm("_binary_assets_bmp_fewcloudsnight_jpg_start");
extern const uint8_t fewclouds_night_end[]          asm("_binary_assets_bmp_fewcloudsnight_jpg_end");
extern const uint8_t mist_start[]                   asm("_binary_assets_bmp_mist_jpg_start");
extern const uint8_t mist_end[]                     asm("_binary_assets_bmp_mist_jpg_end");
extern const uint8_t mist_night_start[]             asm("_binary_assets_bmp_mistnight_jpg_start");
extern const uint8_t mist_night_end[]               asm("_binary_assets_bmp_mistnight_jpg_end");
extern const uint8_t rain_start[]                   asm("_binary_assets_bmp_rain_jpg_start");
extern const uint8_t rain_end[]                     asm("_binary_assets_bmp_rain_jpg_end");
extern const uint8_t rain_night_start[]             asm("_binary_assets_bmp_rainnight_jpg_start");
extern const uint8_t rain_night_end[]               asm("_binary_assets_bmp_rainnight_jpg_end");
extern const uint8_t scatteredclouds_start[]        asm("_binary_assets_bmp_scatteredclouds_jpg_start");
extern const uint8_t scatteredclouds_end[]          asm("_binary_assets_bmp_scatteredclouds_jpg_end");
extern const uint8_t scatteredclouds_night_start[]  asm("_binary_assets_bmp_scatteredcloudsnight_jpg_start");
extern const uint8_t scatteredclouds_night_end[]    asm("_binary_assets_bmp_scatteredcloudsnight_jpg_end");
extern const uint8_t showerrain_start[]             asm("_binary_assets_bmp_showerrain_jpg_start");
extern const uint8_t showerrain_end[]               asm("_binary_assets_bmp_showerrain_jpg_end");
extern const uint8_t showerrain_night_start[]       asm("_binary_assets_bmp_showerrainnight_jpg_start");
extern const uint8_t showerrain_night_end[]         asm("_binary_assets_bmp_showerrainnight_jpg_end");
extern const uint8_t snow_start[]                   asm("_binary_assets_bmp_snow_jpg_start");
extern const uint8_t snow_end[]                     asm("_binary_assets_bmp_snow_jpg_end");
extern const uint8_t snow_night_start[]             asm("_binary_assets_bmp_snownight_jpg_start");
extern const uint8_t snow_night_end[]               asm("_binary_assets_bmp_snownight_jpg_end");
extern const uint8_t thunderstorm_start[]           asm("_binary_assets_bmp_thunderstorm_jpg_start");
extern const uint8_t thunderstorm_end[]             asm("_binary_assets_bmp_thunderstorm_jpg_end");
extern const uint8_t thunderstorm_night_start[]     asm("_binary_assets_bmp_thunderstormnight_jpg_start");
extern const uint8_t thunderstorm_night_end[]       asm("_binary_assets_bmp_thunderstormnight_jpg_end");
static constexpr std::array pszDaysOfWeek = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

static const std::map<const String, EmbeddedFile, std::less<const String>>& GetWeatherIcons()
{
    static const std::map<const String, EmbeddedFile, std::less<const String>> weatherIcons =
    {
        { "01d", EmbeddedFile(clearsky_start, clearsky_end) },
        { "02d", EmbeddedFile(fewclouds_start, fewclouds_end) },
        { "03d", EmbeddedFile(scatteredclouds_start, scatteredclouds_end) },
        { "04d", EmbeddedFile(brokenclouds_start, brokenclouds_end) },
        { "09d", EmbeddedFile(showerrain_start, showerrain_end) },
        { "10d", EmbeddedFile(rain_start, rain_end) },
        { "11d", EmbeddedFile(thunderstorm_start, thunderstorm_end) },
        { "13d", EmbeddedFile(snow_start, snow_end) },
        { "50d", EmbeddedFile(mist_start, mist_end) },
        { "01n", EmbeddedFile(clearsky_night_start, clearsky_night_end) },
        { "02n", EmbeddedFile(fewclouds_night_start, fewclouds_night_end) },
        { "03n", EmbeddedFile(scatteredclouds_night_start, scatteredclouds_night_end) },
        { "04n", EmbeddedFile(brokenclouds_night_start, brokenclouds_night_end) },
        { "09n", EmbeddedFile(showerrain_night_start, showerrain_night_end) },
        { "10n", EmbeddedFile(rain_night_start, rain_night_end) },
        { "11n", EmbeddedFile(thunderstorm_night_start, thunderstorm_night_end) },
        { "13n", EmbeddedFile(snow_night_start, snow_night_end) },
        { "50n", EmbeddedFile(mist_night_start, mist_night_end) }
    };
    return weatherIcons;
}

/**
 * @brief This class implements the Weather Data effect
 *
 */

class PatternWeather : public EffectWithId<PatternWeather>
{
  private:

    String strLocationName    = "";
    String strLocation        = "";
    String strCountryCode     = "";
    String strLatitude        = "0.0";
    String strLongitude       = "0.0";
    int    dayOfWeek          = 0;
    String iconToday          = "";
    String iconTomorrow       = "";
    const EmbeddedFile* iconTodayFile    = nullptr;
    const EmbeddedFile* iconTomorrowFile = nullptr;
    float  temperature        = 0.0f;
    float  highToday          = 0.0f;
    float  loToday            = 0.0f;
    float  highTomorrow       = 0.0f;
    float  loTomorrow         = 0.0f;

    bool   dataReady          = false;
    size_t readerIndex        = SIZE_MAX;
    std::chrono::system_clock::time_point latestUpdate = std::chrono::system_clock::from_time_t(0);

    mutable std::mutex                  _dataMutex;
    std::atomic<bool>                   _isFetching{false};
    std::shared_ptr<std::atomic<bool>>  _isAlive = std::make_shared<std::atomic<bool>>(true);

    /**
     * @brief Should this effect show its title.
     * The weather is obviously weather, and we don't want text overlaid on top of our text
     *
     * @return bool - false No title rquired
     */
    virtual bool ShouldShowTitle() const
    {
        return false;
    }

    /**
     * @brief How many frames per second does this effect want
     *
     * @return size_t - 25 FPS
     */
    size_t DesiredFramesPerSecond() const override
    {
        return 25;
    }

    /**
     * @brief Does this effect requre double buffering support
     *
     * @return bool - false No double buffering needed
     */
    bool RequiresDoubleBuffering() const override
    {
        return false;
    }

    /**
     * @brief Convert Kelvin to Farenheit
     *
     * @param K  Temperature in Kelvin
     * @return float - Farenheit temperature
     */
    static inline float KelvinToFarenheit(float K)
    {
        return (K - 273.15f) * 9.0f/5.0f + 32.0f;
    }

    /**
     * @brief Convert Kelvin to Celsius
     *
     * @param K Temperature in Kelvin
     * @return float - Celsius temperature
     */
    static inline float KelvinToCelsius(float K)
    {
        return K - 273.15f;
    }

    /**
     * @brief Convert Kelvin temperature to local units
     * based on the device configuration flag Use Celsius
     *
     * @param K Temperature in Kelvin
     * @return float - temperature in selected units
     */
    static inline float KelvinToLocal(float K)
    {
        if (g_ptrSystem->GetDeviceConfig().UseCelsius())
            return KelvinToCelsius(K);
        else
            return KelvinToFarenheit(K);
    }

    /**
     * @brief Update the latitude and longitude for the
     * selected city or zip code from the device configuration
     *
     * @return bool - true if the lat/log location is updated
     */
    bool updateCoordinates()
    {
        HTTPClient http;
        String url;

        if (!HasLocationChanged())
            return false;

        const String configLocation = g_ptrSystem->GetDeviceConfig().GetLocation();
        const String configCountryCode = g_ptrSystem->GetDeviceConfig().GetCountryCode();
        const bool configLocationIsZip = g_ptrSystem->GetDeviceConfig().IsLocationZip();

        if (configLocationIsZip)
            url = "http://api.openweathermap.org/geo/1.0/zip"
                "?zip=" + urlEncode(configLocation) + "," + urlEncode(configCountryCode) + "&appid=" + urlEncode(g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey());
        else
            url = "http://api.openweathermap.org/geo/1.0/direct"
                "?q=" + urlEncode(configLocation) + "," + urlEncode(configCountryCode) + "&limit=1&appid=" + urlEncode(g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey());

        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode <= 0)
        {
            debugE("Error fetching coordinates for location: %s", configLocation.c_str());
            http.end();
            return false;
        }

        auto doc = CreateJsonDocument();
        deserializeJson(doc, http.getString());
        JsonObject coordinates = configLocationIsZip ? doc.as<JsonObject>() : doc[0].as<JsonObject>();

        String lat = coordinates["lat"].as<String>();
        String lon = coordinates["lon"].as<String>();

        http.end();

        {
            std::lock_guard<std::mutex> lock(_dataMutex);
            strLatitude = lat;
            strLongitude = lon;
            strLocation = configLocation;
            strCountryCode = configCountryCode;
        }

        return true;
    }

    /**
     * @brief Get the forcast for Tomorrow from the API
     *
     * Tommorow's expected high and low temperatures,
     * and an icon for tomorrow's weather forcast
     *
     * @param highTemp address to store the high temperature
     * @param lowTemp address to store the low temperature
     * @return bool - true if valid weather data retrieved
     */
    bool getTomorrowTemps(float& highTemp, float& lowTemp)
    {
        HTTPClient http;
        String lat, lon, loc, country;
        {
            std::lock_guard<std::mutex> lock(_dataMutex);
            lat = strLatitude;
            lon = strLongitude;
            loc = strLocation;
            country = strCountryCode;
        }

        String url = "http://api.openweathermap.org/data/2.5/forecast"
            "?lat=" + lat + "&lon=" + lon + "&cnt=16&appid=" + urlEncode(g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey());
        http.begin(url);
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0)
        {
            // Needs to be this large to process all the returned JSON
            auto doc = CreateJsonDocument();
            deserializeJson(doc, http.getString());
            JsonArray list = doc["list"];

            // Get tomorrow's date
            auto tomorrow = std::chrono::system_clock::now() + std::chrono::hours(24);
            auto tomorrowTime = std::chrono::system_clock::to_time_t(tomorrow);
            auto tomorrowLocal = localtime(&tomorrowTime);
            char dateStr[11];
            strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", tomorrowLocal);

            float dailyMinimum = 999.0;
            float dailyMaximum = 0.0;
            int slot = 0;

            String newIconTomorrow = "";
            const EmbeddedFile* newIconTomorrowFile = nullptr;

            // Look for the temperature data for tomorrow
            for (size_t i = 0; i < list.size(); ++i)
            {
                JsonObject entry = list[i];

                // convert the entry UTC to localtime
                time_t entryTime = entry["dt"];
                tm* entryLocal = localtime(&entryTime);
                char entryStr[11];
                strftime(entryStr, sizeof(entryStr), "%Y-%m-%d", entryLocal);

                // if it is tomorrow then figure out the min and max and get the icon
                if (strcmp(dateStr, entryStr) == 0)
                {
                    slot++;
                    JsonObject main = entry["main"];

                    // Identify the maximum of the 3 hour maximum temperature
                    float entryMaximum = main["temp_max"];
                    dailyMaximum = std::max(dailyMaximum, entryMaximum);

                    // Identify the minimum of the 3 hour mimimum temperatures
                    float entryMinimum = main["temp_min"];
                    if (entryMinimum > 0)
                        dailyMinimum = std::min(dailyMinimum, entryMinimum);

                    // Use the noon slot for the icon
                    if (slot == 4)
                    {
                        newIconTomorrow = entry["weather"][0]["icon"].as<String>();
                        auto dictEntry = GetWeatherIcons().find(newIconTomorrow);
                        newIconTomorrowFile = (dictEntry != GetWeatherIcons().end()) ? &dictEntry->second : nullptr;
                    }
                }
            }

            highTemp        = KelvinToLocal(dailyMaximum);
            lowTemp         = KelvinToLocal(dailyMinimum);

            {
                std::lock_guard<std::mutex> lock(_dataMutex);
                iconTomorrow = newIconTomorrow;
                iconTomorrowFile = newIconTomorrowFile;
            }

            debugI("Got tomorrow's temps: Lo %d, Hi %d, Icon %s", (int)lowTemp, (int)highTemp, newIconTomorrow.c_str());

            http.end();
            return true;
        }
        else
        {
            debugE("Error fetching forecast data for location: %s in country: %s", loc.c_str(), country.c_str());
            http.end();
            return false;
        }
    }

    /**
     * @brief Get the Weather Data from the API
     *
     * Current temperature, expected high and low temperatures,
     * and an icon for the current weather
     *
     * @return bool - true if valid weather data retrieved
     */
    bool getWeatherData()
    {
        HTTPClient http;
        String lat, lon, loc, country;
        {
            std::lock_guard<std::mutex> lock(_dataMutex);
            lat = strLatitude;
            lon = strLongitude;
            loc = strLocation;
            country = strCountryCode;
        }

        String url = "http://api.openweathermap.org/data/2.5/weather"
            "?lat=" + lat + "&lon=" + lon + "&appid=" + urlEncode(g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey());
        http.begin(url);
        int httpResponseCode = http.GET();
        if (httpResponseCode > 0)
        {
            auto jsonDoc = CreateJsonDocument();
            deserializeJson(jsonDoc, http.getString());

            float newTemp = KelvinToLocal(jsonDoc["main"]["temp"]);
            float newHigh = KelvinToLocal(jsonDoc["main"]["temp_max"]);
            float newLo   = KelvinToLocal(jsonDoc["main"]["temp_min"]);

            String newIconToday = jsonDoc["weather"][0]["icon"].as<String>();
            auto dictEntry = GetWeatherIcons().find(newIconToday);
            const EmbeddedFile* newIconFile = (dictEntry != GetWeatherIcons().end()) ? &dictEntry->second : nullptr;
            const char * pszName = jsonDoc["name"];

            {
                std::lock_guard<std::mutex> lock(_dataMutex);
                iconToday = newIconToday;
                iconTodayFile = newIconFile;
                temperature = newTemp;
                highToday   = newHigh;
                loToday     = newLo;
                if (0 < jsonDoc["main"]["temp"])
                    dataReady = true;
                if (pszName)
                    strLocationName = pszName;
            }

            debugI("Got today's temps: Now %d Lo %d, Hi %d, Icon %s", (int)newTemp, (int)newLo, (int)newHigh, newIconToday.c_str());

            http.end();
            return true;
        }
        else
        {
            debugE("Error fetching Weather data for location: %s in country: %s", loc.c_str(), country.c_str());
            http.end();
            return false;
        }
    }

    /**
     * @brief Hook called from the Network Reader Thread
     * This drives the collection of the weather data.
     *
     */
    void DoUpdateWeather()
    {
        if (!nd_network::IsWiFiConnected())
        {
            debugW("WiFi not connected, skipping Weather update.");
            return;
        }

        std::shared_ptr<std::atomic<bool>> isAlive = _isAlive;

        // Only try to update if we have an API Key
        if (!g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey().isEmpty())
        {
            if (!isAlive || !*isAlive) return;
            updateCoordinates();

            if (!isAlive || !*isAlive) return;
            float hiTom = 0.0f;
            float loTom = 0.0f;
            if (getWeatherData())
            {
                if (!isAlive || !*isAlive) return;
                if (getTomorrowTemps(hiTom, loTom))
                {
                    if (!isAlive || !*isAlive) return;
                    std::lock_guard<std::mutex> lock(_dataMutex);
                    highTomorrow = hiTom;
                    loTomorrow   = loTom;
                }
            }
        }
    }

    void UpdateWeather()
    {
        if (_isFetching.exchange(true))
        {
            debugI("Weather update already in progress, skipping.");
            return;
        }

        debugI("Starting async update of weather...");

        struct FetchParams
        {
            PatternWeather* self;
            std::shared_ptr<std::atomic<bool>> isAlive;
        };

        auto params = std::make_unique<FetchParams>(FetchParams{this, _isAlive});

        TaskHandle_t taskHandle = nullptr;
        BaseType_t result = xTaskCreatePinnedToCore(
            [](void* arg) {
                auto p = std::unique_ptr<FetchParams>(static_cast<FetchParams*>(arg));
                std::shared_ptr<std::atomic<bool>> isAlive = p->isAlive;
                PatternWeather* self = p->self;

                if (isAlive && *isAlive)
                {
                    self->DoUpdateWeather();
                }
                self->_isFetching = false;
                vTaskDelete(NULL);
            },
            "WeatherFetchTask",
            8192,
            params.get(),
            1, // low priority background task
            &taskHandle,
            1  // app core
        );

        if (result == pdPASS)
        {
            params.release(); // Task took ownership
        }
        else
        {
            _isFetching = false;
            debugE("Failed to create WeatherFetchTask");
        }
    }

    /**
     * @brief Determine if the device configuration has changed the
     * location selection
     *
     * @return bool - true if the location has changed
     */
    bool HasLocationChanged() const
    {
        std::lock_guard<std::mutex> lock(_dataMutex);
        bool locationChanged = g_ptrSystem->GetDeviceConfig().GetLocation() != strLocation;
        bool countryChanged = g_ptrSystem->GetDeviceConfig().GetCountryCode() != strCountryCode;

        return locationChanged || countryChanged;
    }

public:

    /**
     * @brief Construct a new Pattern Weather object
     *
     */
    PatternWeather() : EffectWithId<PatternWeather>("Weather") {}

    /**
     * @brief Construct a new Pattern Weather object
     *
     * @param jsonObject Configuration JSON Object
     */
    PatternWeather(const JsonObjectConst&  jsonObject) : EffectWithId<PatternWeather>(jsonObject) {}

    /**
     * @brief Destroy the Pattern Weather object
     * Cancel the Network Reader thread for this index
     */
    ~PatternWeather()
    {
        *_isAlive = false;
        g_ptrSystem->GetNetworkReader().CancelReader(readerIndex);
        while (_isFetching)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    /**
     * @brief
     *
     * @param gfx Graphic Base engine
     * @return bool - true if successfully initialized
     */
    bool Init(std::vector<std::shared_ptr<GFXBase>>& gfx) override
    {
        if (!LEDStripEffect::Init(gfx))
            return false;

        // Register a Network Reader task with no interval.  Will manually flag in Draw()
        readerIndex = g_ptrSystem->GetNetworkReader().RegisterReader([this] { UpdateWeather(); });

        return true;
    }

    /**
     * @brief This handles the drawing of the weather data.
     * It also triggers the network reader on intervals.
     * Will tell the user if there is no API Key configured
     *
     */
    void Draw() override
    {
        const int fontHeight = 7;
        const int fontWidth  = 5;
        const int xHalf      = MATRIX_WIDTH / 2 - 1;

        g()->fillScreen(BLACK16);
        g()->fillRect(0, 0, MATRIX_WIDTH, 9, g()->to16bit(CRGB(0,0,128)));
        g()->setFont(&Apple5x7);

        auto now = std::chrono::system_clock::now();

        auto secondsSinceLastUpdate = now - latestUpdate;

        // If location and/or country have changed, trigger an update regardless of timer, but
        // not more than once every half a minute
        if (secondsSinceLastUpdate >= WEATHER_INTERVAL || (HasLocationChanged() && secondsSinceLastUpdate >= std::chrono::seconds(30)))
        {
            latestUpdate = now;

            debugI("Triggering thread to check weather now...");
            // Trigger the weather reader.
            g_ptrSystem->GetNetworkReader().FlagReader(readerIndex);
        }

        // Thread-safe local snapshot of weather state for rendering
        const EmbeddedFile* localIconTodayFile = nullptr;
        const EmbeddedFile* localIconTomorrowFile = nullptr;
        String localIconTodayStr;
        String localIconTomorrowStr;
        String localShowLocation;
        String localLocationName;
        bool localDataReady = false;
        float localTemp = 0.0f;
        float localHighToday = 0.0f;
        float localLoToday = 0.0f;
        float localHighTomorrow = 0.0f;
        float localLoTomorrow = 0.0f;

        {
            std::lock_guard<std::mutex> lock(_dataMutex);
            localIconTodayFile = iconTodayFile;
            localIconTomorrowFile = iconTomorrowFile;
            localIconTodayStr = iconToday;
            localIconTomorrowStr = iconTomorrow;
            localShowLocation = strLocation;
            localLocationName = strLocationName;
            localDataReady = dataReady;
            localTemp = temperature;
            localHighToday = highToday;
            localLoToday = loToday;
            localHighTomorrow = highTomorrow;
            localLoTomorrow = loTomorrow;
        }

        // Draw the graphics
        if (localIconTodayFile)
        {
            if (JDR_OK != TJpgDec.drawJpg(0, 10, localIconTodayFile->contents, localIconTodayFile->length))        // Draw the image
                debugW("Could not display icon %s", localIconTodayStr.c_str());
        }

        if (localIconTomorrowFile)
        {
            if (JDR_OK != TJpgDec.drawJpg(xHalf+1, 10, localIconTomorrowFile->contents, localIconTomorrowFile->length))        // Draw the image
                debugW("Could not display icon %s", localIconTomorrowStr.c_str());
        }

        // Print the town/city name
        int x = 0;
        int y = fontHeight + 1;
        g()->setCursor(x, y);
        g()->setTextColor(WHITE16);
        localShowLocation.toUpperCase();
        if (g_ptrSystem->GetDeviceConfig().GetOpenWeatherAPIKey().isEmpty())
            g()->print("No API Key");
        else
            g()->print((localLocationName.isEmpty() ? localShowLocation : localLocationName).substring(0, (MATRIX_WIDTH - 2 * fontWidth)/fontWidth));

        // Display the temperature, right-justified

        if (localDataReady)
        {
            String strTemp((int)localTemp);
            x = MATRIX_WIDTH - fontWidth * strTemp.length();
            g()->setCursor(x, y);
            g()->setTextColor(g()->to16bit(CRGB(192,192,192)));
            g()->print(strTemp);
        }

        // Draw the separator lines

        y+=1;

        g()->drawLine(0, y, MATRIX_WIDTH-1, y, CRGB(0,0,128));
        g()->drawLine(xHalf, y, xHalf, MATRIX_HEIGHT-1, CRGB(0,0,128));
        y+=2 + fontHeight;

        // Figure out which day of the week it is

        time_t today = time(nullptr);
        const tm * todayTime = localtime(&today);
        const char * pszToday = pszDaysOfWeek[todayTime->tm_wday];
        const char * pszTomorrow = pszDaysOfWeek[ (todayTime->tm_wday + 1) % 7 ];

        // Draw the day of the week and tomorrow's day as well
        g()->setTextColor(WHITE16);
        g()->setCursor(0, MATRIX_HEIGHT);
        g()->print(pszToday);
        g()->setCursor(xHalf+2, MATRIX_HEIGHT);
        g()->print(pszTomorrow);

        // Draw the temperature in lighter white

        if (localDataReady)
        {
            g()->setTextColor(g()->to16bit(CRGB(192,192,192)));
            String strHi((int) localHighToday);
            String strLo((int) localLoToday);

            // Draw today's HI and LO temperatures

            x = xHalf - fontWidth * strHi.length();
            y = MATRIX_HEIGHT - fontHeight;
            g()->setCursor(x,y);
            g()->print(strHi);
            x = xHalf - fontWidth * strLo.length();
            y+= fontHeight;
            g()->setCursor(x,y);
            g()->print(strLo);

            // Draw tomorrow's HI and LO temperatures

            strHi = String((int)localHighTomorrow);
            strLo = String((int)localLoTomorrow);
            x = MATRIX_WIDTH - fontWidth * strHi.length();
            y = MATRIX_HEIGHT - fontHeight;
            g()->setCursor(x,y);
            g()->print(strHi);
            x = MATRIX_WIDTH - fontWidth * strLo.length();
            y+= fontHeight;
            g()->setCursor(x,y);
            g()->print(strLo);
        }
    }
};

#endif
