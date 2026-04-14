#pragma once

//+--------------------------------------------------------------------------
//
// File:        improvserial.h
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
//    Support for the Improv serial WiFI credential provisioning standard
//
// History:     Jun-03-2023         Davepl      Created
//
//---------------------------------------------------------------------------

#include "globals.h"
#include "improv.h"
#include "nd_network.h"

#define ENABLE_IMPROV_LOGGING 0

// ImprovSerial
//
// Listens on a serial port for Improv provisioning commands.

template <typename T>
class ImprovSerial
{
  private:

    T * _pSerial;

    String firmware_name_;
    String firmware_version_;
    String hardware_variant_;
    String device_name_;

    std::vector<uint8_t> _buffer;

    #if ENABLE_IMPROV_LOGGING
        #define log_write(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
    #else
        #define log_write(fmt, ...)
    #endif

    std::function<void(uint8_t)> _on_unknown_byte = nullptr;

    void process_improv_data(uint8_t * data, size_t len)
    {
        improv::ImprovCommand command = improv::parse_command(data, len);

        switch (command.command)
        {
            case improv::WIFI_SETTINGS:
            {
                String ssid = command.ssid;
                String password = command.password;

                debugW("Improv received WiFi settings for SSID: %s", ssid.c_str());

                nd_network::WiFiConnectResult result = nd_network::ConnectToWiFi(ssid, password);

                if (result == nd_network::WiFiConnectResult::Connected)
                {
                    debugI("Improv provisioning success - writing to NVS");
                    nd_network::WriteWiFiConfig(nd_network::WifiCredSource::ImprovCreds, ssid, password);

                    std::vector<uint8_t> url = improv::build_rpc_response(improv::WIFI_SETTINGS, { "http://" + nd_network::GetWiFiLocalIP() }, false);
                    _pSerial->write(url.data(), url.size());
                    improv::set_state(improv::STATE_PROVISIONED);
                    std::vector<uint8_t> state = { 'I', 'M', 'P', 'R', 'O', 'V', 0x01, improv::STATE_PROVISIONED, 0x00 };
                    state[8] = improv::checksum(state);
                    _pSerial->write(state.data(), state.size());
                }
                else
                {
                    debugE("Improv provisioning failure");
                    improv::set_error(improv::ERROR_UNABLE_TO_CONNECT);
                    std::vector<uint8_t> error = { 'I', 'M', 'P', 'R', 'O', 'V', 0x01, improv::ERROR_UNABLE_TO_CONNECT, 0x00 };
                    error[8] = improv::checksum(error);
                    _pSerial->write(error.data(), error.size());
                }
                break;
            }

            case improv::GET_CURRENT_STATE:
            {
                log_write("GET_CURRENT_STATE\n");
                improv::State state = (WiFi.status() == WL_CONNECTED) ? improv::STATE_PROVISIONED : improv::STATE_AUTHORIZED;
                std::vector<uint8_t> response = { 'I', 'M', 'P', 'R', 'O', 'V', 0x01, state, 0x00 };
                response[8] = improv::checksum(response);
                _pSerial->write(response.data(), response.size());
                break;
            }

            case improv::GET_DEVICE_INFO:
            {
                log_write("GET_DEVICE_INFO\n");
                std::vector<uint8_t> info = improv::build_rpc_response(improv::GET_DEVICE_INFO, { firmware_name_, firmware_version_, hardware_variant_, device_name_ }, false);
                _pSerial->write(info.data(), info.size());
                break;
            }

            case improv::GET_WIFI_NETWORKS:
            {
                log_write("GET_WIFI_NETWORKS\n");
                int n = WiFi.scanNetworks();
                for (int i = 0; i < n; ++i)
                {
                    std::vector<uint8_t> network = improv::build_rpc_response(
                            improv::GET_WIFI_NETWORKS, {WiFi.SSID(i), str_sprintf("%ld", (long)WiFi.RSSI(i)), WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "YES" : "NO"}, false);
                    _pSerial->write(network.data(), network.size());
                }
                break;
            }

            default:
                log_write("Unknown Improv command: %d\n", command.command);
                break;
        }
    }

  public:

    ImprovSerial() : _pSerial(nullptr) {}

    void setup(const String& firmware_name, const String& firmware_version, const String& hardware_variant, const String& device_name, T * pSerial)
    {
        _pSerial = pSerial;
        firmware_name_ = firmware_name;
        firmware_version_ = firmware_version;
        hardware_variant_ = hardware_variant;
        device_name_ = device_name;
    }

    void set_on_unknown_byte(std::function<void(uint8_t)> callback)
    {
        _on_unknown_byte = callback;
    }

    void loop()
    {
        if (_pSerial == nullptr)
            return;

        while (_pSerial->available())
        {
            uint8_t b = _pSerial->read();

            if (improv::parse_improv_serial(_buffer, b, _on_unknown_byte))
            {
                process_improv_data(_buffer.data(), _buffer.size());
                _buffer.clear();
            }
        }
    }
};

extern std::unique_ptr<ImprovSerial<typeof(Serial)>> g_pImprovSerial;
