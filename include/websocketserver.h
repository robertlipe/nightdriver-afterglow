#pragma once

//+--------------------------------------------------------------------------
//
// File:        websocketserver.h
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
//   Server class that optionally registers one or more web sockets with the
//   on-board webserver to allow interactions with web clients that include
//   messages being pushed from server to client.
//
// History:     Dec-21-2023         Rbergen     Created
//---------------------------------------------------------------------------

#include "globals.h"

#if WEB_SOCKETS_ANY_ENABLED

#include "effectmanager.h"
#include "webserver.h"

class WebSocketServer : public IEffectEventListener
{
    AsyncWebSocket _colorDataSocket;
    AsyncWebSocket _effectChangeSocket;
    static constexpr size_t _maxColorNumberLen = sizeof(NAME_OF(16777215)) - 1; // (uint32_t)CRGB(255,255,255) == 16777215
    static constexpr size_t _maxCountLen = sizeof(NAME_OF(4294967295)) - 1;     // SIZE_MAX == 4294967295

public:

    WebSocketServer(CWebServer& webServer) :
        _colorDataSocket("/ws/frames"),
        _effectChangeSocket("/ws/effects")
    {
        auto eventHandler = [](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
            if (type == WS_EVT_CONNECT) {
                debugI("WS Client #%u connected to %s from %s", client->id(), server->url(), client->remoteIP().toString().c_str());
            } else if (type == WS_EVT_DISCONNECT) {
                debugI("WS Client #%u disconnected from %s", client->id(), server->url());
            } else if (type == WS_EVT_ERROR) {
                debugE("WS Client #%u error on %s: %s", client->id(), server->url(), arg ? (const char*)arg : "Unknown");
            }
        };

        _colorDataSocket.onEvent(eventHandler);
        _effectChangeSocket.onEvent(eventHandler);

        #if COLORDATA_WEB_SOCKET_ENABLED
        webServer.AddWebSocket(_colorDataSocket);
        #endif

        #if EFFECTS_WEB_SOCKET_ENABLED
        webServer.AddWebSocket(_effectChangeSocket);
        #endif
    }

    void CleanupClients()
    {
        _colorDataSocket.cleanupClients();
        _effectChangeSocket.cleanupClients();
    }

    bool HaveColorDataClients()
    {
        return _colorDataSocket.count() > 0;
    }

    size_t ColorDataClientCount() const
    {
        return _colorDataSocket.count();
    }

    size_t EffectChangeClientCount() const
    {
        return _effectChangeSocket.count();
    }

    // Send the color data for an array of leds of indicated length.
    void SendColorData(CRGB* leds, size_t count)
    {
        if (!HaveColorDataClients() || leds == nullptr || count == 0 || !_colorDataSocket.availableForWriteAll())
            return;

        _colorDataSocket.binaryAll((uint8_t *)leds, count * sizeof(CRGB));
    }

    void OnCurrentEffectChanged(size_t currentEffectIndex) override
    {
        if (_effectChangeSocket.availableForWriteAll())
            _effectChangeSocket.textAll(str_sprintf("{\"currentEffectIndex\":%zu}", currentEffectIndex));
    }

    void OnEffectListDirty() override
    {
        if (_effectChangeSocket.availableForWriteAll())
            _effectChangeSocket.textAll("{\"effectListDirty\":true}");
    }

    void OnEffectEnabledStateChanged(size_t effectIndex, bool newState) override
    {
        if (_effectChangeSocket.availableForWriteAll())
            _effectChangeSocket.textAll(str_sprintf("{\"effectsEnabledState\":[{\"index\":%zu,\"enabled\":%s}]}", effectIndex, newState ? "true" : "false"));
    }

    void OnIntervalChanged(uint interval) override
    {
        if (_effectChangeSocket.availableForWriteAll())
            _effectChangeSocket.textAll(str_sprintf("{\"interval\":%u}", interval));
    }
};
#endif // WEB_SOCKETS_ANY_ENABLED
