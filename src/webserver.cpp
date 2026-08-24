//+--------------------------------------------------------------------------
//
// File:        webserver.cpp
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
//   Implementations for some of the web server methods declared in webserver.h
//
// History:     Apr-18-2023         Rbergen     Created
//              Apr-28-2023         Rbergen     Reduce code duplication
//---------------------------------------------------------------------------

#include "globals.h"

#if ENABLE_WEBSERVER

#include "nd_network.h"
#include "webserver.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <esp_partition.h>

#include <memory>
#include <utility>

#include "deviceconfig.h"
#include "effectmanager.h"
#include "improvserial.h"
#include "ledstripeffect.h"
#include "soundanalyzer.h"
#include "systemcontainer.h"
#include "taskmgr.h"
#include "values.h"

// Static member initializers

#if ENABLE_WIFI
const char captivePortalHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<title>NightDriver WiFi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body {
    background: linear-gradient(135deg, #1f1c2c, #928dab);
    color: #ffffff;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    margin: 0;
    padding: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
  }
  .container {
    background: rgba(255, 255, 255, 0.08);
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    border: 1px solid rgba(255, 255, 255, 0.15);
    border-radius: 16px;
    padding: 32px;
    width: 90%;
    max-width: 400px;
    box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
  }
  h1 {
    font-size: 24px;
    margin-top: 0;
    margin-bottom: 8px;
    font-weight: 700;
    text-align: center;
    background: linear-gradient(45deg, #ff7b00, #ffae00);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
  }
  p {
    font-size: 14px;
    color: #ccc;
    text-align: center;
    margin-bottom: 24px;
  }
  label {
    display: block;
    font-size: 12px;
    font-weight: 600;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: #aaa;
  }
  .input-field {
    width: 100%;
    padding: 12px;
    background: rgba(255, 255, 255, 0.1);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 8px;
    color: #fff;
    font-size: 15px;
    margin-bottom: 20px;
    box-sizing: border-box;
    transition: all 0.3s ease;
  }
  .input-field:focus {
    outline: none;
    border-color: #ffae00;
    background: rgba(255, 255, 255, 0.15);
  }
  select.input-field {
    cursor: pointer;
  }
  .btn {
    width: 100%;
    padding: 14px;
    background: linear-gradient(45deg, #ff7b00, #ffae00);
    border: none;
    border-radius: 8px;
    color: #fff;
    font-size: 16px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.3s ease;
    box-shadow: 0 4px 15px rgba(255, 123, 0, 0.4);
  }
  .btn:hover {
    transform: translateY(-2px);
    box-shadow: 0 6px 20px rgba(255, 123, 0, 0.6);
  }
  .btn:disabled {
    background: #555;
    box-shadow: none;
    cursor: not-allowed;
    transform: none;
  }
</style>
<script>
function get_strength_bar(rssi) {
    if (rssi > -55) return '▇';
    if (rssi > -65) return '▆';
    if (rssi > -75) return '▅';
    if (rssi > -85) return '▄';
    return '▃';
}
function toggle_password() {
    var pwd = document.getElementById("password");
    if (pwd.type === "password") {
        pwd.type = "text";
    } else {
        pwd.type = "password";
    }
}
function scan_ssids() {
    var sel = document.getElementById('ssid_select');
    sel.innerHTML = '';
    var opt = document.createElement('option');
    opt.innerHTML = 'Scanning networks...';
    opt.disabled = true;
    sel.appendChild(opt);
    fetch('/scan.json')
    .then(response => response.json())
    .then(data => {
        data.sort((a, b) => b.rssi - a.rssi);
        sel.innerHTML = '';
        var opt = document.createElement('option');
        opt.innerHTML = 'Select a Network';
        opt.disabled = true;
        opt.selected = true;
        sel.appendChild(opt);
        for (var i = 0; i < data.length; i++) {
            var opt = document.createElement('option');
            opt.value = data[i].ssid;
            opt.innerHTML = get_strength_bar(data[i].rssi) + ' ' + data[i].ssid;
            sel.appendChild(opt);
        }
    });
}
function set_ssid_text(value) {
    document.getElementById('ssid_text').value = value;
}
function handle_submit(event) {
    event.preventDefault();
    const form = event.target;
    const data = new FormData(form);
    const ssid = data.get('ssid');
    const password = data.get('password');

    document.getElementById('submit_btn').disabled = true;
    document.getElementById('submit_btn').value = 'Saving...';

    fetch('/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams({ ssid, password }).toString()
    }).then(response => {
        if (response.ok) {
            response.text().then(html => {
                document.open();
                document.write(html);
                document.close();
            });
        } else {
            response.text().then(text => {
                alert('Failed to save credentials: ' + text + '\nPlease try again.');
                document.getElementById('submit_btn').disabled = false;
                document.getElementById('submit_btn').value = 'Save';
            });
        }
    }).catch(error => {
        alert('An error occurred: ' + error + '\nPlease check your connection and try again.');
        document.getElementById('submit_btn').disabled = false;
        document.getElementById('submit_btn').value = 'Save';
    });
}
window.onload = scan_ssids;
</script>
</head>
<body>
<div class="container">
  <h1>NightDriver Setup</h1>
  <p>Configure device WiFi connection details</p>
  <form method="POST" action="/wifi" onsubmit="handle_submit(event)">
    <label for="ssid_select">SSID</label>
    <select id="ssid_select" class="input-field" onchange="set_ssid_text(this.value)"></select>
    <input type="text" id="ssid_text" class="input-field" name="ssid" placeholder="Or type SSID manually">

    <label for="password">Password</label>
    <input type="password" id="password" class="input-field" name="password" placeholder="Enter password" style="margin-bottom: 8px;">

    <div style="display: flex; align-items: center; margin-bottom: 20px;">
        <input type="checkbox" id="show_password" onclick="toggle_password()" style="margin-right: 8px; cursor: pointer;">
        <label for="show_password" style="margin-bottom: 0; color: #ccc; text-transform: none; font-size: 14px; font-weight: normal; cursor: pointer;">Show Password</label>
    </div>
    <input type="submit" id="submit_btn" class="btn" value="Save Credentials">
  </form>
</div>
</body>
</html>
)rawliteral";
#endif

// Static member initializers


// Maps settings for which a validator is available to the invocation thereof
const std::map<String, CWebServer::ValueValidator> CWebServer::settingValidators
{
    { DeviceConfig::OpenWeatherApiKeyTag, [](const String& value) { return g_ptrSystem->GetDeviceConfig().ValidateOpenWeatherAPIKey(value); } },
    { DeviceConfig::PowerLimitTag,        [](const String& value) { return g_ptrSystem->GetDeviceConfig().ValidatePowerLimit(value); } },
    { DeviceConfig::BrightnessTag,        [](const String& value) { return g_ptrSystem->GetDeviceConfig().ValidateBrightness(value); } }
};

std::vector<SettingSpec> CWebServer::mySettingSpecs = {};
std::vector<std::reference_wrapper<SettingSpec>> CWebServer::deviceSettingSpecs{};

// Member function template specializations

// Push param that represents a bool. Values considered true are text "true" and any whole number not equal to 0
template<>
bool CWebServer::PushPostParamIfPresent<bool>(const AsyncWebServerRequest * pRequest, const String &paramName, ValueSetter<bool> setter)
{
    return PushPostParamIfPresent<bool>(pRequest, paramName, std::move(setter), [](const AsyncWebParameter * param)
    {
        return BoolFromText(param->value());
    });
}

// Push param that represents a size_t
template<>
bool CWebServer::PushPostParamIfPresent<size_t>(const AsyncWebServerRequest * pRequest, const String &paramName, ValueSetter<size_t> setter)
{
    return PushPostParamIfPresent<size_t>(pRequest, paramName, std::move(setter), [](const AsyncWebParameter * param)
    {
        return strtoul(param->value().c_str(), nullptr, 10);
    });
}

// Push param that represents an int
template<>
bool CWebServer::PushPostParamIfPresent<int>(const AsyncWebServerRequest * pRequest, const String &paramName, ValueSetter<int> setter)
{
    return PushPostParamIfPresent<int>(pRequest, paramName, std::move(setter), [](const AsyncWebParameter * param)
    {
        return std::stoi(param->value().c_str());
    });
}

// Push param that represents a color
template<>
bool CWebServer::PushPostParamIfPresent<CRGB>(const AsyncWebServerRequest * pRequest, const String &paramName, ValueSetter<CRGB> setter)
{
    return PushPostParamIfPresent<CRGB>(pRequest, paramName, std::move(setter), [](const AsyncWebParameter * param)
    {
        return CRGB(strtoul(param->value().c_str(), nullptr, 10));
    });
}

// Add CORS header to and send JSON response
template<>
void CWebServer::AddCORSHeaderAndSendResponse<AsyncJsonResponse>(AsyncWebServerRequest * pRequest, AsyncJsonResponse * pResponse)
{
    pResponse->setLength();
    AddCORSHeaderAndSendResponse<AsyncWebServerResponse>(pRequest, pResponse);
}

// Member function implementations

// begin - register page load handlers and start serving pages
void CWebServer::SetupStationMode()

{
    [[maybe_unused]] extern const uint8_t html_start[] asm("_binary_site_dist_index_html_gz_start");
    [[maybe_unused]] extern const uint8_t html_end[] asm("_binary_site_dist_index_html_gz_end");
    [[maybe_unused]] extern const uint8_t js_start[] asm("_binary_site_dist_index_js_gz_start");
    [[maybe_unused]] extern const uint8_t js_end[] asm("_binary_site_dist_index_js_gz_end");
    [[maybe_unused]] extern const uint8_t ico_start[] asm("_binary_site_dist_favicon_ico_gz_start");
    [[maybe_unused]] extern const uint8_t ico_end[] asm("_binary_site_dist_favicon_ico_gz_end");
    [[maybe_unused]] extern const uint8_t timezones_start[] asm("_binary_config_timezones_json_start");
    [[maybe_unused]] extern const uint8_t timezones_end[] asm("_binary_config_timezones_json_end");

    EmbeddedWebFile html_file(html_start, html_end, "text/html", "gzip");
    EmbeddedWebFile js_file(js_start, js_end, "application/javascript", "gzip");
    EmbeddedWebFile ico_file(ico_start, ico_end, "image/vnd.microsoft.icon", "gzip");
    EmbeddedWebFile timezones_file(timezones_start, timezones_end - 1, "text/json"); // end - 1 because of zero-termination

    debugI("Embedded html file size: %zu", (size_t)html_file.length);
    debugI("Embedded jsx file size: %zu", (size_t)js_file.length);
    debugI("Embedded ico file size: %zu", (size_t)ico_file.length);
    debugI("Embedded timezones file size: %zu", (size_t)timezones_file.length);

    _staticStats.HeapSize = ESP.getHeapSize();
    _staticStats.DmaHeapSize = heap_caps_get_total_size(MALLOC_CAP_DMA);
    _staticStats.PsramSize = ESP.getPsramSize();
    _staticStats.ChipModel = ESP.getChipModel();
    _staticStats.ChipCores = ESP.getChipCores();
    _staticStats.CpuFreqMHz = ESP.getCpuFreqMHz();
    _staticStats.SketchSize = 0; // ESP.getSketchSize();  // Known to trigger Task WDT on ESP32 Core v3
    _staticStats.FreeSketchSpace = 0; // ESP.getFreeSketchSpace();
    _staticStats.FlashChipSize = ESP.getFlashChipSize();

    debugI("Connecting Web Endpoints");

    // UserFS file requests

    _server.on("/effectsConfig",         HTTP_GET,  [](AsyncWebServerRequest* pRequest) { pRequest->send(UserFS, EFFECTS_CONFIG_FILE,   "text/json"); });
    #if ENABLE_IMPROV_LOGGING
        _server.on(IMPROV_LOG_FILE,      HTTP_GET,  [](AsyncWebServerRequest* pRequest) { pRequest->send(UserFS, IMPROV_LOG_FILE,       "text/plain"); });
    #endif

    // Instance handler requests

    _server.on("/statistics/static",     HTTP_GET,  [this](AsyncWebServerRequest* pRequest)
                                                    { this->GetStatistics(pRequest, StatisticsType::Static); });
    _server.on("/statistics/dynamic",    HTTP_GET,  [this](AsyncWebServerRequest* pRequest)
                                                    { this->GetStatistics(pRequest, StatisticsType::Dynamic); });
    _server.on("/statistics",            HTTP_GET,  [this](AsyncWebServerRequest* pRequest)
                                                    { this->GetStatistics(pRequest); });
    _server.on("/getStatistics",         HTTP_GET,  [this](AsyncWebServerRequest* pRequest)
                                                    { this->GetStatistics(pRequest); });

    _server.on("/coredump", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
        if (part != NULL)
        {
            AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", part->size, [part](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
            {
                size_t len = std::min(maxLen, (size_t)(part->size - index));
                if (len > 0)
                {
                    esp_err_t err = esp_partition_read(part, index, buffer, len);
                    if (err != ESP_OK)
                    {
                        return 0;
                    }
                }
                return len;
            });
            response->addHeader("Content-Disposition", "attachment; filename=\"coredump.bin\"");
            AddCORSHeaderAndSendResponse(request, response);
        }
        else
        {
            request->send(404, "text/plain", "Coredump partition not found");
        }
    });

    // Static handler requests

    _server.on("/effects",               HTTP_GET,  GetEffectListText);
    _server.on("/getEffectList",         HTTP_GET,  GetEffectListText);
    _server.on("/nextEffect",            HTTP_POST, NextEffect);
    _server.on("/previousEffect",        HTTP_POST, PreviousEffect);

    _server.on("/currentEffect",         HTTP_POST, SetCurrentEffectIndex);
    _server.on("/setCurrentEffectIndex", HTTP_POST, SetCurrentEffectIndex);
    _server.on("/enableEffect",          HTTP_POST, EnableEffect);
    _server.on("/disableEffect",         HTTP_POST, DisableEffect);
    _server.on("/moveEffect",            HTTP_POST, MoveEffect);
    _server.on("/copyEffect",            HTTP_POST, CopyEffect);
    _server.on("/deleteEffect",          HTTP_POST, DeleteEffect);

    _server.on("/settings/effect/specs", HTTP_GET,  GetEffectSettingSpecs);
    _server.on("/settings/effect",       HTTP_GET,  GetEffectSettings);
    _server.on("/settings/effect",       HTTP_POST, SetEffectSettings);
    _server.on("/settings/validated",    HTTP_POST, ValidateAndSetSetting);
    _server.on("/settings/specs",        HTTP_GET,  GetSettingSpecs);
    _server.on("/settings",              HTTP_GET,  GetSettings);
    _server.on("/settings",              HTTP_POST, SetSettings);

    _server.on("/reset",                 HTTP_POST, Reset);

    // Embedded file requests

    ServeEmbeddedFile("/timezones.json", timezones_file);

    #if ENABLE_WEB_UI
        debugI("Web UI URL pathnames enabled");

        ServeEmbeddedFile("/", html_file);
        ServeEmbeddedFile("/index.html", html_file);
        ServeEmbeddedFile("/index.js", js_file);
        ServeEmbeddedFile("/favicon.ico", ico_file);
    #endif

    // Not found handler

    _server.onNotFound([](AsyncWebServerRequest *request)
    {
        if (request->method() == HTTP_OPTIONS) {
            request->send(CWebServer::HttpOk);                                // Apparently needed for CORS: https://github.com/me-no-dev/ESPAsyncWebServer
        } else {
                debugW("Failed GET for %s\n", request->url().c_str() );
            request->send(CWebServer::HttpNotFound);
        }
    });

    _server.begin();

    debugI("HTTP server started");
}

void CWebServer::begin(bool captivePortalMode)
{
    debugI("CWebServer::begin() - _isInitialized: %d, captivePortalMode: %d", _isInitialized.load(), captivePortalMode);

    if (_isInitialized.exchange(true))
    {
        _server.begin(); // Ensure the server is listening even if already configured
        debugI("CWebServer::begin() - Server already initialized, ensuring listen.");
        return;
    }

    if (captivePortalMode)
    {
        SetupCaptivePortalMode();
    }
    else
    {
        SetupStationMode();
    }

    _server.begin();
    debugI("HTTP server started");
}

void CWebServer::Stop()
{
    if (_isInitialized.exchange(false))
    {
        debugI("CWebServer::Stop() - Stopping HTTP server and DNS server.");
        _server.end();
        if (_dnsServer) {
            _dnsServer->stop();
            _dnsServer.reset();
        }
#if ENABLE_WIFI
        _captivePortalActive = false;
#endif
    }
}

#if ENABLE_WIFI
void CWebServer::SetupCaptivePortalMode()
{
    debugW("Starting Captive Portal AP setup.");

    WiFi.persistent(false);

    // Use the robust function to set AP mode
    bool setModeSuccess = nd_network::SetWiFiMode(nd_network::WiFiMode::AP);
    if (!setModeSuccess)
    {
        debugE("Failed to robustly set WiFi mode to WIFI_AP for Captive Portal setup.");
        SetCaptivePortalActive(false);
        return;
    }

    String mac = get_mac_address();
    String unique_id = mac.substring(mac.length() - 6);
    unique_id.toUpperCase();
    String ap_name = "NightDriver-Setup-" + unique_id;

    debugW("Calling softAP() for '%s'...", ap_name.c_str());
    bool softAPSuccess = nd_network::StartSoftAP(ap_name);
    if (!softAPSuccess)
    {
        debugE("Failed to start softAP (returned false)");
        SetCaptivePortalActive(false);
        return;
    }
    debugW("softAP() call succeeded.");
    delay(200); // Small delay for AP to stabilize

    IPAddress apIP = WiFi.softAPIP();
    debugW("AP IP address: %s", apIP.toString().c_str());

    debugW("Scanning for networks...");
    int n = nd_network::ScanNetworks();
    _availableNetworks.clear();
    if (n > 0)
    {
        _availableNetworks.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            _availableNetworks.push_back({WiFi.SSID(i), WiFi.RSSI(i)});
        }
        debugW("Found %d networks.", n);
    }
    else
    {
        debugW("WiFi scan failed or found 0 networks. Result: %d", n);
    }
    WiFi.scanDelete(); // Free memory allocated by scanNetworks()

    if (WiFi.getMode() != WIFI_AP) {
        debugW("CWebServer::SetupCaptivePortalMode: WiFi mode changed during scan, resetting to WIFI_AP.");
        // Do NOT use nd_network::SetWiFiMode here, as it calls softAPdisconnect() and destroys the AP we just created!
        WiFi.mode(WIFI_AP);
    }

    if (_dnsServer) {
        _dnsServer->stop();
    }
    _dnsServer = std::make_unique<DNSServer>();
    _dnsServer->start(53, "*", apIP);
    debugW("DNS server started.");

    RegisterCaptivePortalHandlers();
}

void CWebServer::RegisterCaptivePortalHandlers()
{
    _server.on("/scan.json", HTTP_GET, [this](AsyncWebServerRequest *request)
    {
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        for (const auto& net : _availableNetworks)
        {
            JsonObject obj = array.add<JsonObject>();
            obj["ssid"] = net.ssid;
            obj["rssi"] = net.rssi;
        }
        String json;
        serializeJson(doc, json);

        request->send(200, "application/json", json);
    });

    _server.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest *request)
    {
        HandleWifiSave(request);
    });

    _server.onNotFound([](AsyncWebServerRequest *request)
    {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", captivePortalHtml);
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "Fri, 01 Jan 1990 00:00:00 GMT");
        request->send(response);
    });
}

void CWebServer::HandleWifiSave(AsyncWebServerRequest *request)
{
    const int params = request->params();
    for (int i = 0; i < params; ++i)
    {
        const auto* p = request->getParam(i);
        if (p->isFile())
        {
            debugI("PARAM[FILE][%s]: %s, size: %u", p->name().c_str(), p->value().c_str(), p->size());
        }
        else
        {
            if (strcmp(p->name().c_str(), "password") == 0)
            {
                debugI("PARAM[POST][%s]: ********", p->name().c_str());
            }
            else
            {
                debugI("PARAM[POST][%s]: %s", p->name().c_str(), p->value().c_str());
            }
        }
    }

    String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : String();
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : String();

    if (ssid.length() > 0)
    {
        // Don't log PII:  debugI("Captive portal received new WiFi credentials for SSID: %s", ssid.c_str());

        g_Analyzer.Pause();
        // Clear old credentials to make space
        ClearWiFiConfig(WifiCredSource::CompileTimeCreds);
        ClearWiFiConfig(WifiCredSource::ImprovCreds);
        ClearWiFiConfig(WifiCredSource::CaptivePortal);

        if (WriteWiFiConfig(WifiCredSource::CaptivePortal, ssid, password))
        {
            String hostname = g_ptrSystem->GetDeviceConfig().GetHostname();
            if (hostname.isEmpty())
            {
                String mac = get_mac_address();
                hostname = "NightDriver-" + mac.substring(mac.length() - 6);
                hostname.toUpperCase();
            }

            AsyncResponseStream *response = request->beginResponseStream("text/html");
            response->print(F("<html><head><title>Rebooting...</title>"));
            response->print(F("</head><body style=\"font-family: sans-serif;\">"));
            response->print(F("<h1>Credentials Saved. Rebooting...</h1>"));
            response->printf(PSTR("<p>Your device is now rebooting and will attempt to connect to the <b>%s</b> network.</p>"), ssid.c_str());
            response->printf(PSTR("<p>Please reconnect your client device (phone/computer) to the <b>%s</b> network.</p>"), ssid.c_str());
            response->print(F("<p>Check your router's connected devices list to find the IP address of your NightDriver device.</p>"));
            response->print(F("</body></html>"));

            response->addHeader("Connection", "close");
            request->send(response);

            // Trigger global system reboot request
            nd_network::RequestSystemReboot(3000);
        }
        else
        {
            g_Analyzer.Resume();
            debugE("Failed to write WiFi credentials to NVS.");
            request->send(500, "text/plain", "Failed to save credentials. NVS full?");
        }
    }
    else
    {
        debugE("Received empty SSID in /wifi POST.");
        request->send(400, "text/plain", "SSID cannot be empty.");
    }
}

void CWebServer::ProcessDnsRequests()
{
    if (_dnsServer)
    {
        _dnsServer->processNextRequest();
    }
}

void CWebServer::SetCaptivePortalActive(bool active)
{
    _captivePortalActive = active;
}

bool CWebServer::IsCaptivePortalActive() const
{
    return _captivePortalActive;
}
#endif // ENABLE_WIFI

bool CWebServer::IsPostParamTrue(AsyncWebServerRequest * pRequest, const String & paramName)
{
    bool returnValue = false;

    PushPostParamIfPresent<bool>(pRequest, paramName, [&returnValue](auto value) { returnValue = value; return true; });

    return returnValue;
}

long CWebServer::GetEffectIndexFromParam(AsyncWebServerRequest * pRequest, bool post)
{
    if (!pRequest->hasParam("effectIndex", post, false))
        return -1;

    return strtol(pRequest->getParam("effectIndex", post, false)->value().c_str(), nullptr, 10);
}

void CWebServer::SendBufferOverflowResponse(AsyncWebServerRequest * pRequest)
{
    AddCORSHeaderAndSendResponse(
        pRequest,
        pRequest->beginResponse(
            CWebServer::HttpInternalServerError,
            "text/json",
            "{\"message\": \"JSON response buffer overflow\"}"
        )
    );
}

void CWebServer::GetEffectListText(AsyncWebServerRequest * pRequest)
{
    debugV("GetEffectListText");

    auto response = std::make_unique<AsyncJsonResponse>();
    auto& j = response->getRoot();
    auto& effectManager = g_ptrSystem->GetEffectManager();

    j["currentEffect"]         = effectManager.GetCurrentEffectIndex();
    j["millisecondsRemaining"] = effectManager.GetTimeRemainingForCurrentEffect();
    j["eternalInterval"]       = effectManager.IsIntervalEternal();
    j["effectInterval"]        = effectManager.GetInterval();

    for (const auto& effect : effectManager.EffectsList())
    {
        auto effectDoc = CreateJsonDocument();

        effectDoc["name"]    = effect->FriendlyName();
        effectDoc["enabled"] = effect->IsEnabled();
        effectDoc["core"]    = effect->IsCoreEffect();

        if (!j["Effects"].add(effectDoc))
        {
            debugV("JSON response buffer overflow!");
            SendBufferOverflowResponse(pRequest);
            return;
        }
    }

    AddCORSHeaderAndSendResponse(pRequest, response.release());
}

void CWebServer::GetStatistics(AsyncWebServerRequest * pRequest, StatisticsType statsType) const
{
    auto response = std::make_unique<AsyncJsonResponse>();
    auto& j = response->getRoot();

    if ((statsType & StatisticsType::Static) != StatisticsType::None)
    {
        j["MATRIX_WIDTH"]          = MATRIX_WIDTH;
        j["MATRIX_HEIGHT"]         = MATRIX_HEIGHT;
        j["FRAMES_SOCKET"]         = !!COLORDATA_WEB_SOCKET_ENABLED;
        j["EFFECTS_SOCKET"]        = !!EFFECTS_WEB_SOCKET_ENABLED;
        j["CHIP_MODEL"]            = _staticStats.ChipModel;
        j["CHIP_CORES"]            = _staticStats.ChipCores;
        j["CHIP_SPEED"]            = _staticStats.CpuFreqMHz;
        j["PROG_SIZE"]             = _staticStats.SketchSize;
        j["CODE_SIZE"]             = _staticStats.SketchSize;
        j["FLASH_SIZE"]            = _staticStats.FlashChipSize;
        j["HEAP_SIZE"]             = _staticStats.HeapSize;
        j["DMA_SIZE"]              = _staticStats.DmaHeapSize;
        j["PSRAM_SIZE"]            = _staticStats.PsramSize;
        j["CODE_FREE"]             = _staticStats.FreeSketchSpace;
    }

    if ((statsType & StatisticsType::Dynamic) != StatisticsType::None)
    {
        j["LED_FPS"]               = g_Values.FPS;
        j["SERIAL_FPS"]            = g_Analyzer.SerialFPS();
        j["AUDIO_FPS"]             = g_Analyzer.AudioFPS();
        j["HEAP_FREE"]             = ESP.getFreeHeap();
        j["HEAP_MIN"]              = ESP.getMinFreeHeap();
        j["DMA_FREE"]              = heap_caps_get_free_size(MALLOC_CAP_DMA);
        j["DMA_MIN"]               = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        j["PSRAM_FREE"]            = ESP.getFreePsram();
        j["PSRAM_MIN"]             = ESP.getMinFreePsram();
        auto& taskManager = g_ptrSystem->GetTaskManager();

        j["CPU_USED"]              = taskManager.GetCPUUsagePercent();
        j["CPU_USED_CORE0"]        = taskManager.GetCPUUsagePercent(0);
        j["CPU_USED_CORE1"]        = taskManager.GetCPUUsagePercent(1);
    }

    AddCORSHeaderAndSendResponse(pRequest, response.release());
}

void CWebServer::SetCurrentEffectIndex(AsyncWebServerRequest * pRequest)
{
    debugV("SetCurrentEffectIndex");
    PushPostParamIfPresent<size_t>(pRequest, "currentEffectIndex", SET_VALUE(g_ptrSystem->GetEffectManager().SetCurrentEffectIndex(value)));
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::EnableEffect(AsyncWebServerRequest * pRequest)
{
    debugV("EnableEffect");
    PushPostParamIfPresent<size_t>(pRequest, "effectIndex", SET_VALUE(g_ptrSystem->GetEffectManager().EnableEffect(value)));
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::DisableEffect(AsyncWebServerRequest * pRequest)
{
    debugV("DisableEffect");
    PushPostParamIfPresent<size_t>(pRequest, "effectIndex", SET_VALUE(g_ptrSystem->GetEffectManager().DisableEffect(value)));
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::MoveEffect(AsyncWebServerRequest * pRequest)
{
    debugV("MoveEffect");

    auto fromIndex = GetEffectIndexFromParam(pRequest, true);
    if (fromIndex == -1)
    {
        AddCORSHeaderAndSendOKResponse(pRequest);
        return;
    }

    PushPostParamIfPresent<size_t>(pRequest, "newIndex", SET_VALUE(g_ptrSystem->GetEffectManager().MoveEffect(fromIndex, value)));
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::CopyEffect(AsyncWebServerRequest * pRequest)
{
    debugV("CopyEffect");

    auto index = GetEffectIndexFromParam(pRequest, true);
    if (index == -1)
    {
        AddCORSHeaderAndSendOKResponse(pRequest);
        return;
    }

    auto effect = g_ptrSystem->GetEffectManager().CopyEffect(index);
    if (!effect)
    {
        AddCORSHeaderAndSendOKResponse(pRequest);
        return;
    }

    ApplyEffectSettings(pRequest, effect);

    if (g_ptrSystem->GetEffectManager().AppendEffect(effect))
        SendEffectSettingsResponse(pRequest, effect);
    else
        AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::DeleteEffect(AsyncWebServerRequest * pRequest)
{
    debugV("DeleteEffect");

    auto index = GetEffectIndexFromParam(pRequest, true);
    if (index == -1)
    {
        AddCORSHeaderAndSendOKResponse(pRequest);
        return;
    }

    if (index < g_ptrSystem->GetEffectManager().EffectCount() && g_ptrSystem->GetEffectManager().EffectsList()[index]->IsCoreEffect())
    {
        AddCORSHeaderAndSendBadRequest(pRequest, "Can't delete core effect");
        return;
    }

    g_ptrSystem->GetEffectManager().DeleteEffect(index);
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::NextEffect(AsyncWebServerRequest * pRequest)
{
    debugV("NextEffect");
    g_ptrSystem->GetEffectManager().NextEffect();
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::PreviousEffect(AsyncWebServerRequest * pRequest)
{
    debugV("PreviousEffect");
    g_ptrSystem->GetEffectManager().PreviousEffect();
    AddCORSHeaderAndSendOKResponse(pRequest);
}

void CWebServer::SendSettingSpecsResponse(AsyncWebServerRequest * pRequest, const std::vector<std::reference_wrapper<SettingSpec>> & settingSpecs)
{
    auto response = std::make_unique<AsyncJsonResponse>();
    auto jsonArray = response->getRoot().to<JsonArray>();

    for (const auto& specWrapper : settingSpecs)
    {
        const auto& spec = specWrapper.get();
        auto specObject = jsonArray.add<JsonObject>();

        auto jsonDoc = CreateJsonDocument();

        jsonDoc["name"] = spec.Name;
        jsonDoc["friendlyName"] = spec.FriendlyName;
        if (spec.Description)
            jsonDoc["description"] = spec.Description;
        jsonDoc["type"] = to_value(spec.Type);
        jsonDoc["typeName"] = spec.TypeName();
        if (spec.HasValidation)
            jsonDoc["hasValidation"] = true;
        if (spec.MinimumValue.has_value())
            jsonDoc["minimumValue"] = spec.MinimumValue.value();
        if (spec.MaximumValue.has_value())
            jsonDoc["maximumValue"] = spec.MaximumValue.value();
        if (spec.EmptyAllowed.has_value())
            jsonDoc["emptyAllowed"] = spec.EmptyAllowed.value();
        switch (spec.Access)
        {
            case SettingSpec::SettingAccess::ReadOnly:
                jsonDoc["readOnly"] = true;
                break;

            case SettingSpec::SettingAccess::WriteOnly:
                jsonDoc["writeOnly"] = true;
                break;

            default:
                // Default is read/write, so we don't need to specify that
                break;
        }

        if (jsonDoc.overflowed() || !specObject.set(jsonDoc.as<JsonObjectConst>()))
        {
            debugV("JSON response buffer overflow!");
            SendBufferOverflowResponse(pRequest);
            return;
        }
    }

    AddCORSHeaderAndSendResponse(pRequest, response.release());
}

const std::vector<std::reference_wrapper<SettingSpec>> & CWebServer::LoadDeviceSettingSpecs()
{
    if (deviceSettingSpecs.empty())
    {
        mySettingSpecs.emplace_back(
            "effectInterval",
            "Effect interval",
            "The duration in milliseconds that an individual effect runs, before the next effect is activated.",
            SettingSpec::SettingType::PositiveBigInteger
        );
        deviceSettingSpecs.insert(deviceSettingSpecs.end(), mySettingSpecs.begin(), mySettingSpecs.end());

        auto deviceConfigSpecs = g_ptrSystem->GetDeviceConfig().GetSettingSpecs();
        deviceSettingSpecs.insert(deviceSettingSpecs.end(), deviceConfigSpecs.begin(), deviceConfigSpecs.end());
    }

    return deviceSettingSpecs;
}

void CWebServer::GetSettingSpecs(AsyncWebServerRequest * pRequest)
{
    SendSettingSpecsResponse(pRequest, LoadDeviceSettingSpecs());
}

// Responds with current config, excluding any sensitive values
void CWebServer::GetSettings(AsyncWebServerRequest * pRequest)
{
    auto response = std::make_unique<AsyncJsonResponse>();
    response->addHeader("Server", "NightDriverStrip");
    auto root = response->getRoot();
    JsonObject jsonObject = root.to<JsonObject>();

    // We get the serialized JSON for the device config, without any sensitive values
    g_ptrSystem->GetDeviceConfig().SerializeToJSON(jsonObject, false);
    jsonObject["effectInterval"] = g_ptrSystem->GetEffectManager().GetInterval();

    AddCORSHeaderAndSendResponse(pRequest, response.release());
}

// Support function that silently sets whatever settings are included in the request passed.
//   Composing a response is left to the invoker!
void CWebServer::SetSettingsIfPresent(AsyncWebServerRequest * pRequest)
{
    auto& deviceConfig = g_ptrSystem->GetDeviceConfig();
    auto& effectManager = g_ptrSystem->GetEffectManager();

    PushPostParamIfPresent<size_t>(pRequest,"effectInterval", SET_VALUE(effectManager.SetInterval(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::HostnameTag, SET_VALUE(deviceConfig.SetHostname(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::LocationTag, SET_VALUE(deviceConfig.SetLocation(value)));
    PushPostParamIfPresent<bool>(pRequest, DeviceConfig::LocationIsZipTag, SET_VALUE(deviceConfig.SetLocationIsZip(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::CountryCodeTag, SET_VALUE(deviceConfig.SetCountryCode(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::OpenWeatherApiKeyTag, SET_VALUE(deviceConfig.SetOpenWeatherAPIKey(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::TimeZoneTag, SET_VALUE(deviceConfig.SetTimeZone(value)));
    PushPostParamIfPresent<bool>(pRequest, DeviceConfig::Use24HourClockTag, SET_VALUE(deviceConfig.Set24HourClock(value)));
    PushPostParamIfPresent<bool>(pRequest, DeviceConfig::UseCelsiusTag, SET_VALUE(deviceConfig.SetUseCelsius(value)));
    PushPostParamIfPresent<String>(pRequest, DeviceConfig::NTPServerTag, SET_VALUE(deviceConfig.SetNTPServer(value)));
    PushPostParamIfPresent<bool>(pRequest, DeviceConfig::RememberCurrentEffectTag, SET_VALUE(deviceConfig.SetRememberCurrentEffect(value)));
    PushPostParamIfPresent<int>(pRequest, DeviceConfig::PowerLimitTag, SET_VALUE(deviceConfig.SetPowerLimit(value)));
    PushPostParamIfPresent<int>(pRequest, DeviceConfig::BrightnessTag, SET_VALUE(deviceConfig.SetBrightness(value)));
    #if ENABLE_WIFI
    PushPostParamIfPresent<size_t>(pRequest, DeviceConfig::PortalTimeoutSecondsTag, SET_VALUE(deviceConfig.SetPortalTimeoutSeconds(value)));
    #endif


    #if SHOW_VU_METER
    PushPostParamIfPresent<bool>(pRequest, DeviceConfig::ShowVUMeterTag, SET_VALUE(effectManager.ShowVU(value)));
    #endif

    std::optional<CRGB> globalColor = {};
    std::optional<CRGB> secondColor = {};

    PushPostParamIfPresent<CRGB>(pRequest, DeviceConfig::GlobalColorTag, SET_VALUE(globalColor = value));
    PushPostParamIfPresent<CRGB>(pRequest, DeviceConfig::SecondColorTag, SET_VALUE(secondColor = value));

    deviceConfig.ApplyColorSettings(globalColor, secondColor,
                                    IsPostParamTrue(pRequest, DeviceConfig::ClearGlobalColorTag),
                                    IsPostParamTrue(pRequest, DeviceConfig::ApplyGlobalColorsTag));
}

// Set settings and return resulting config
void CWebServer::SetSettings(AsyncWebServerRequest * pRequest)
{
    debugV("SetSettings");

    SetSettingsIfPresent(pRequest);

    // We return the current config in response
    GetSettings(pRequest);
}

bool CWebServer::CheckAndGetSettingsEffect(AsyncWebServerRequest * pRequest, std::shared_ptr<LEDStripEffect> & effect, bool post)
{
    auto effectsList = g_ptrSystem->GetEffectManager().EffectsList();
    auto effectIndex = GetEffectIndexFromParam(pRequest, post);

    if (effectIndex < 0 || effectIndex >= effectsList.size())
    {
        AddCORSHeaderAndSendOKResponse(pRequest);

        return false;
    }

    effect = effectsList[effectIndex];

    return true;
}

void CWebServer::GetEffectSettingSpecs(AsyncWebServerRequest * pRequest)
{
    std::shared_ptr<LEDStripEffect> effect;

    if (!CheckAndGetSettingsEffect(pRequest, effect))
        return;

    auto settingSpecs = effect->GetSettingSpecs();

    SendSettingSpecsResponse(pRequest, settingSpecs);
}

void CWebServer::SendEffectSettingsResponse(AsyncWebServerRequest * pRequest, std::shared_ptr<LEDStripEffect> & effect)
{
    auto response = std::make_unique<AsyncJsonResponse>();
    auto jsonObject = response->getRoot().to<JsonObject>();

    if (effect->SerializeSettingsToJSON(jsonObject))
    {
        AddCORSHeaderAndSendResponse(pRequest, response.release());
        return;
    }

    debugV("JSON response buffer overflow!");
    SendBufferOverflowResponse(pRequest);
}

void CWebServer::GetEffectSettings(AsyncWebServerRequest * pRequest)
{
    debugV("GetEffectSettings");

    std::shared_ptr<LEDStripEffect> effect;

    if (!CheckAndGetSettingsEffect(pRequest, effect))
        return;

    SendEffectSettingsResponse(pRequest, effect);
}

bool CWebServer::ApplyEffectSettings(AsyncWebServerRequest * pRequest, std::shared_ptr<LEDStripEffect> & effect)
{
    bool settingChanged = false;

    for (auto& settingSpecWrapper : effect->GetSettingSpecs())
    {
        const String& settingName = settingSpecWrapper.get().Name;
        settingChanged = PushPostParamIfPresent<String>(pRequest, settingName, [&](auto value) { return effect->SetSetting(settingName, value); })
            || settingChanged;
    }

    return settingChanged;
}

void CWebServer::SetEffectSettings(AsyncWebServerRequest * pRequest)
{
    debugV("SetEffectSettings");

    std::shared_ptr<LEDStripEffect> effect;

    if (!CheckAndGetSettingsEffect(pRequest, effect, true))
        return;

    if (ApplyEffectSettings(pRequest, effect))
        SaveEffectManagerConfig();

    SendEffectSettingsResponse(pRequest, effect);
}

// Validate and set one setting. If no validator is available in settingValidators for the setting, validation is skipped.
//   Requests containing more than one known setting are malformed and rejected.
void CWebServer::ValidateAndSetSetting(AsyncWebServerRequest * pRequest)
{
    String paramName;

    for (auto& settingSpecWrapper : LoadDeviceSettingSpecs())
    {
        auto& settingSpec = settingSpecWrapper.get();

        if (pRequest->hasParam(settingSpec.Name, true))
        {
            if (paramName.isEmpty())
                paramName = settingSpec.Name;
            else
            // We found multiple known settings in the request, which we don't allow
            {
                AddCORSHeaderAndSendBadRequest(pRequest, "Malformed request");
                return;
            }
        }
    }

    // No known setting in the request, so we can stop processing and go on with our business
    if (paramName.isEmpty())
    {
        AddCORSHeaderAndSendOKResponse(pRequest);
        return;
    }

    auto validator = settingValidators.find(paramName);
    if (validator != settingValidators.end())
    {
        const String &paramValue = pRequest->getParam(paramName, true)->value();
        bool isValid;
        String validationMessage;

        std::tie(isValid, validationMessage) = validator->second(paramValue);

        if (!isValid)
        {
            AddCORSHeaderAndSendBadRequest(pRequest, validationMessage);
            return;
        }
    }

    // Process the setting as per usual
    SetSettingsIfPresent(pRequest);
    AddCORSHeaderAndSendOKResponse(pRequest);
}

// Reset effect config, device config and/or the board itself
void CWebServer::Reset(AsyncWebServerRequest * pRequest)
{
    bool boardResetRequested = IsPostParamTrue(pRequest, "board");
    bool deviceConfigResetRequested = IsPostParamTrue(pRequest, "deviceConfig");
    bool effectsConfigResetRequested = IsPostParamTrue(pRequest, "effectsConfig");

    // We can now let the requester know we're taking care of things without making them wait longer
    AddCORSHeaderAndSendOKResponse(pRequest);

    if (boardResetRequested)
    {
        // Flush any pending writes and make sure nothing is written after. We do this to make sure
        //   that what needs saving is written, but no further writes take place after any requested
        //   config resets have happened.
        g_ptrSystem->GetJSONWriter().FlushWrites(true);

        // Give the device a few seconds to finish the requested writes - this also gives AsyncWebServer
        //   time to push out the response to the request before the device resets
        delay(3000);
    }

    if (deviceConfigResetRequested)
    {
        debugI("Removing DeviceConfig");
        g_ptrSystem->GetDeviceConfig().RemovePersisted();
    }

    if (effectsConfigResetRequested)
    {
        debugI("Removing EffectManager config");
        RemoveEffectManagerConfig();
    }

    if (boardResetRequested)
    {
        debugW("Resetting device at API request!");
        throw std::runtime_error("Resetting device at API request");
    }
}

#endif  // ENABLE_WEBSERVER
