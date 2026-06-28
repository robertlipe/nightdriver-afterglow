//+--------------------------------------------------------------------------
//
// File:        socketserver.cpp
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
//    Hosts a socket server on port 49152 to receive LED data from the master
//
// History:     Oct-26-2018     Davepl      Created
//---------------------------------------------------------------------------

#include "globals.h"
#include "byte_utils.h"
#include "ledbuffer.h"
#include "nd_network.h"
#include "socketserver.h"
#include "soundanalyzer.h"
#include "systemcontainer.h"
#include "values.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

extern "C"
{
    #include "uzlib/src/uzlib.h"
}

#if INCOMING_WIFI_ENABLED

// SocketResponse
//
// Response data sent back to server every time we receive a packet

SocketServer::SocketServer(int port, int numLeds) :
    _port(port),
    _numLeds(numLeds),
    _server_fd(-1),
    _cbReceived(0)
{
    _abOutputBuffer.resize(MAXIMUM_PACKET_SIZE+1);        // +1 for uzlib one byte overreach bug
    _pBuffer.resize(MAXIMUM_PACKET_SIZE);
    memset(&_address, 0, sizeof(_address));
}

void SocketServer::release()
{
    if (_server_fd >= 0)
    {
        close(_server_fd);
        _server_fd = -1;
    }
}

bool SocketServer::begin()
{
    _cbReceived = 0;

    // Creating socket file descriptor
    if ((_server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        debugE("socket error\n");
        release();
        return false;
    }

    nd_network::SetSocketBlockingEnabled(_server_fd, false);

    // When an error occurs, and we close and reopen the port, we need to specify reuse flags
    // or it might be too soon to use the port again, since close doesn't actually close it
    // until the socket is no longer in use.

    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        release();
        return false;
    }

    memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons( _port );

    if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)       // Bind socket to port
    {
        perror("bind failed\n");
        release();
        return false;
    }
    if (listen(_server_fd, 6) < 0)                                                  // Start listening for connections
    {
        perror("listen failed\n");
        release();
        return false;
    }
    return true;
}

void SocketServer::ResetReadBuffer()
{
    _cbReceived = 0;
    std::fill(_pBuffer.begin(), _pBuffer.end(), 0);
}

// ReadUntilNBytesReceived
//
// Read from the socket until the buffer contains at least cbNeeded bytes

bool SocketServer::ReadUntilNBytesReceived(size_t socket, size_t cbNeeded)
{
    if (cbNeeded <= _cbReceived)                            // If we already have that many bytes, we're already done
    {
        debugV("Already had enough data to satisfy read: requested %d, had %d", cbNeeded, _cbReceived);
        return true;
    }

    // This test caps maximum packet size as a full buffer read of LED data.  If other packets wind up being longer,
    // the buffer itself and this test might need to change

    if (cbNeeded > MAXIMUM_PACKET_SIZE)
    {
        debugE("Unexpected request for %d bytes in ReadUntilNBytesReceived\n", cbNeeded);
        return false;
    }

    do
    {
        // If we're reading at a point in the buffer more than just the header, we're actually transferring data, so light up the LED

        // Read data from the socket until we have _bcNeeded bytes in the buffer

        ssize_t cbRead = 0;
        do
        {
            cbRead = recv(socket, _pBuffer.data() + _cbReceived, cbNeeded - _cbReceived, 0);
        } while (cbRead < 0 && errno == EINTR);

        // Restore the old state

        if (cbRead > 0)
        {
            _cbReceived += cbRead;
        }
        else
        {
            debugE("ERROR: %d bytes read in ReadUntilNBytesReceived trying to read %d\n", cbRead, cbNeeded-_cbReceived);
            return false;
        }
    } while (_cbReceived < cbNeeded);
    return true;
}

// DecompressBuffer
//
// Use unzlib to decompress a memory buffer

bool SocketServer::DecompressBuffer(std::span<const uint8_t> compressed, std::span<uint8_t> output)
{
    // The uzlib library has a bug where it can read one byte past the end of the input buffer, so we need to copy
    // the input to a temporary buffer that is one byte larger than the actual size, and pad the end with a zero.
    // It's a small price to pay for a tiny embedded decompressor.

    std::unique_ptr<uint8_t[]> pTemp = std::make_unique<uint8_t[]>(compressed.size() + 1);
    memcpy(pTemp.get(), compressed.data(), compressed.size());
    pTemp[compressed.size()] = 0;

    struct uzlib_uncomp d = { 0 };
    uzlib_uncompress_init(&d, nullptr, 0);

    d.source         = pTemp.get();
    d.source_limit   = pTemp.get() + compressed.size() + 1;
    d.source_read_cb = nullptr;
    d.dest           = output.data();
    d.dest_start     = output.data();
    d.dest_limit     = output.data() + output.size() + 1;

    int res = uzlib_zlib_parse_header(&d);
    if (res < 0)
    {
        debugE("ERROR: Cannot parse zlib data header\n");
        return false;
    }

    res = uzlib_uncompress_chksum(&d);                                          // Expand the data

    if (res != TINF_DONE) {
        debugE("Error during decompression after producing %zd bytes: %d\n", d.dest - output.data(), res);
        return false;
    }

    if (size_t(d.dest - output.data()) != output.size())
    {
        debugE("Expected it to to decompress to %zd but got %zd instead\n", output.size(), d.dest - output.data());
        return false;
    }

    return true;
}

// ProcessIncomingConnectionsLoop
//
// Socket server main ProcessIncomingConnectionsLoop - accepts new connections and reads from them, dispatching
// data packets into our buffer and closing the socket if anything goes weird.

bool SocketServer::ProcessIncomingConnectionsLoop()
{
    if (0 >= _server_fd)
    {
        debugE("No _server_fd, returning.");
        return false;
    }

    int new_socket = -1;

    // Accept loop: wait for an incoming connection, sleeping between polls to avoid busy-spinning
    int addrlen = sizeof(_address);
    while (new_socket < 0)
    {
        new_socket = accept(_server_fd, (struct sockaddr *)&_address, (socklen_t*)&addrlen);
        if (new_socket < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                delay(100); // No connection yet, yield and retry
                continue;
            }
            debugE("Error accepting data: %s", strerror(errno));
            return false;
        }
    }

    // Report where this connection is coming from

    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    if (0 != getpeername(new_socket, (struct sockaddr *)&addr, &addr_size))
    {
        close(new_socket);
        ResetReadBuffer();
        return false;
    }

    debugV("Incoming connection from: %s", inet_ntoa(addr.sin_addr));

    // Set a timeout of 3 seconds on the socket so we don't permanently hang on a corrupt or partial packet

    struct timeval to;
    to.tv_sec = 3;
    to.tv_usec = 0;
    if (setsockopt(new_socket,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to)) < 0)
    {
        debugE("Unable to set read timeout on socket!");
        close(new_socket);
        ResetReadBuffer();
        return false;
    }

    if (_pBuffer.empty())
    {
        debugE("Buffer not allocated!");
        close(new_socket);
        ResetReadBuffer();
        return false;
    }

    // Ensure the new_socket is valid
    if (new_socket < 0) {
        debugE("Invalid socket!");
        ResetReadBuffer();
        return false;
    }

    do
    {
        bool bSendResponsePacket = false;

            // Read until we have at least enough for the data header

        if (false == ReadUntilNBytesReceived(new_socket, STANDARD_DATA_HEADER_SIZE))
        {
            debugE("Read error in getting header.\n");
            break;
        }

        // Now that we have the header we can see how much more data is expected to follow

        const uint32_t header  = _pBuffer[3] << 24  | _pBuffer[2] << 16  | _pBuffer[1] << 8  | _pBuffer[0];
        if (header == COMPRESSED_HEADER)
        {
            uint32_t compressedSize = _pBuffer[7] << 24  | _pBuffer[6] << 16  | _pBuffer[5] << 8  | _pBuffer[4];
            uint32_t expandedSize   = _pBuffer[11] << 24 | _pBuffer[10] << 16 | _pBuffer[9] << 8  | _pBuffer[8];
            uint32_t reserved       = _pBuffer[15] << 24 | _pBuffer[14] << 16 | _pBuffer[13] << 8 | _pBuffer[12];
            debugV("Compressed Header: compressedSize: %lu, expandedSize: %lu, reserved: %lu", (unsigned long)compressedSize, (unsigned long)expandedSize, (unsigned long)reserved);

            if (expandedSize > MAXIMUM_PACKET_SIZE)
            {
                debugE("Expanded packet would be %lu but buffer is only %lu !!!!\n", (unsigned long)expandedSize, (unsigned long)MAXIMUM_PACKET_SIZE);
                break;
            }

            if (false == ReadUntilNBytesReceived(new_socket, COMPRESSED_HEADER_SIZE + compressedSize))
            {
                debugE("Could not read compressed data from stream\n");
                break;
            }
            debugV("Successfully read %zu bytes", (size_t)(COMPRESSED_HEADER_SIZE + compressedSize));

            // If our buffer is in PSRAM it would be expensive to decompress in place, as the SPIRAM doesn't like
            // non-linear access from what I can tell.  I bet it must send addr+len to request each unique read, so
            // one big read one time would work best, and we use that to copy it to a regular RAM buffer.

            #if USE_PSRAM
                std::unique_ptr<uint8_t []> _abTempBuffer = std::make_unique<uint8_t []>(MAXIMUM_PACKET_SIZE+1);    // Plus one for uzlib buffer overreach bug
                memcpy(_abTempBuffer.get(), _pBuffer.data(), MAXIMUM_PACKET_SIZE);
                auto pSourceBuffer = &_abTempBuffer[COMPRESSED_HEADER_SIZE];
            #else
                auto pSourceBuffer = _pBuffer.data() + COMPRESSED_HEADER_SIZE;
            #endif

            if (!DecompressBuffer(std::span<const uint8_t>(pSourceBuffer, compressedSize), std::span<uint8_t>(_abOutputBuffer).first(expandedSize)))
            {
                debugE("Error decompressing data\n");
                break;
            }

            if (false == ProcessIncomingData(std::span<const uint8_t>(_abOutputBuffer).first(expandedSize)))
            {
                debugE("Error processing data\n");
                break;

            }
            ResetReadBuffer();
            bSendResponsePacket = true;
        }
        else
        {
            // Read the rest of the data
            uint16_t command16   = WORDFromMemory(&_pBuffer[0]);

            if (command16 == WIFI_COMMAND_PEAKDATA)
            {
                #if ENABLE_AUDIO
                {
                    uint16_t numbands  = WORDFromMemory(&_pBuffer[2]);
                    uint32_t length32  = DWORDFromMemory(&_pBuffer[4]);
                    uint64_t seconds   = ULONGFromMemory(&_pBuffer[8]);
                    uint64_t micros    = ULONGFromMemory(&_pBuffer[16]);

                    size_t totalExpected = STANDARD_DATA_HEADER_SIZE + length32;

                    debugV("PeakData Header: numbands=%u, length=%lu, seconds=%llu, micro=%llu", numbands, (unsigned long)length32, seconds, micros);

                    if (numbands != NUM_BANDS)
                    {
                        debugE("Expecting %d bands but received %d", NUM_BANDS, numbands);
                        break;
                    }

                    if (length32 != numbands * sizeof(float))
                    {
                        debugE("Expecting %zu bytes for %d audio bands, but received %zu.  Ensure float size and endianness matches between sender and receiver systems.", (size_t)totalExpected, (int)NUM_BANDS, (size_t)_cbReceived);
                        break;
                    }

                    if (false == ReadUntilNBytesReceived(new_socket, totalExpected))
                    {
                        debugE("Error in getting peak data from wifi, could not read the %zu bytes", (size_t)totalExpected);
                        break;
                    }

                    if (false == ProcessIncomingData(std::span<const uint8_t>(_pBuffer).first(totalExpected)))
                        break;

                    // Consume the data by resetting the buffer
                    debugV("Consuming the data as WIFI_COMMAND_PEAKDATA by setting _cbReceived to from %zu down 0.", (size_t)_cbReceived);
                }
                #else
                    // Audio disabled: consume any declared payload to keep stream in sync, then ignore it
                    uint32_t length32  = DWORDFromMemory(&_pBuffer[4]);
                    size_t totalExpected = STANDARD_DATA_HEADER_SIZE + length32;
                    if (!ReadUntilNBytesReceived(new_socket, totalExpected))
                    {
                        debugE("Audio disabled, failed to skip PEAKDATA payload of %zu bytes", (size_t)totalExpected);
                        break;
                    }
                    debugV("Audio disabled; skipped PEAKDATA payload (%zu bytes)", (size_t)totalExpected);
                #endif
                ResetReadBuffer();

            }
            else if (command16 == WIFI_COMMAND_PIXELDATA64)
            {
                // We know it's pixel data, so we do some validation before calling Process.

                uint16_t channel16 = WORDFromMemory(&_pBuffer[2]);
                uint32_t length32  = DWORDFromMemory(&_pBuffer[4]);
                uint64_t seconds   = ULONGFromMemory(&_pBuffer[8]);
                uint64_t micros    = ULONGFromMemory(&_pBuffer[16]);

                debugV("Uncompressed Header: channel16=%u, length=%lu, seconds=%llu, micro=%llu", channel16, (unsigned long)length32, seconds, micros);

                size_t totalExpected = STANDARD_DATA_HEADER_SIZE + length32 * LED_DATA_SIZE;
                if (totalExpected > MAXIMUM_PACKET_SIZE)
                {
                    debugE("Too many bytes promised (%zu) - more than we can use for our LEDs at max packet (%lu)\n", (size_t)totalExpected, (unsigned long)MAXIMUM_PACKET_SIZE);
                    break;
                }

                debugV("Expecting %zu total bytes", (size_t)totalExpected);
                if (false == ReadUntilNBytesReceived(new_socket, totalExpected))
                {
                    debugE("Error in getting pixel data from wifi\n");
                    break;
                }

                // Add it to the buffer ring

                if (false == ProcessIncomingData(std::span<const uint8_t>(_pBuffer).first(totalExpected)))
                {
                    debugE("Error in processing pixel data from wifi\n");
                    break;
                }

                // Consume the data by resetting the buffer
                debugV("Consuming the data as WIFI_COMMAND_PIXELDATA64 by setting _cbReceived to from %zu down 0.", (size_t)_cbReceived);
                ResetReadBuffer();

                bSendResponsePacket = true;
            }
            else
            {
                debugE("Unknown command in packet received: %u\n", command16);
                break;
            }
        }

        // If we make it to this point, it should be success, so we consume

        ResetReadBuffer();

        if (bSendResponsePacket)
        {
            static uint64_t sequence = 0;

            debugV("Sending Response Packet from Socket Server");
            auto& bufferManager = g_ptrSystem->GetBufferManagers()[0];

            SocketResponse response = {
                                        .size = sizeof(SocketResponse),
                                        .sequence     = sequence++,
                                        .flashVersion = FLASH_VERSION,
                                        .currentClock = g_Values.AppTime.CurrentTime(),
                                        .oldestPacket = bufferManager.AgeOfOldestBuffer(),
                                        .newestPacket = bufferManager.AgeOfNewestBuffer(),
                                        .brightness   = g_Values.Brite,
                                        .wifiSignal   = (float) nd_network::GetWiFiRSSI(),
                                        .bufferSize   = bufferManager.BufferCount(),
                                        .bufferPos    = bufferManager.Depth(),
                                        .fpsDrawing   = g_Values.FPS,
                                        .watts        = g_Values.Watts
                                    };

            // I dont think this is fatal, and doesn't affect the read buffer, so content to ignore for now if it happens
            if (sizeof(response) != write(new_socket, &response, sizeof(response)))
                debugE("Unable to send response back to server.");
        }

        delay(1);

    } while (true);

    close(new_socket);
    ResetReadBuffer();
    return false;
}

#endif
