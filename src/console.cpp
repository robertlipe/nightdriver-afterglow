//+--------------------------------------------------------------------------
//
// File:        console.cpp
//
// NightDriverStrip - (c) 2026 Plummer's Software LLC.  All Rights Reserved.
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
//    Implementation of ConsoleSession and ConsoleManager.
//
//---------------------------------------------------------------------------

#include "globals.h"

#include "console.h"
#include "debug_cli.h"
#include "logger.h"

//
// ConsoleSession
//

ConsoleSession::ConsoleSession(IConsoleSink* sink) : _sink(sink) {}

void ConsoleSession::WriteRaw(std::string_view data)
{
    if (_sink)
        _sink->Write(data.data(), data.size());
}

void ConsoleSession::WriteText(std::string_view text)
{
    if (!_sink) return;

    if (_sink->LinePolicy() == LineEndingPolicy::CRLF)
    {
        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\n')
            {
                if (i > start)
                    _sink->Write(text.data() + start, i - start);
                _sink->Write("\r\n", 2);
                start = i + 1;
            }
        }
        if (start < text.size())
            _sink->Write(text.data() + start, text.size() - start);
    }
    else
    {
        _sink->Write(text.data(), text.size());
    }
}

void ConsoleSession::WriteLine(std::string_view text)
{
    WriteText(text);
    WriteText("\n");
    Flush();
}

void ConsoleSession::Flush()
{
    if (_sink)
        _sink->Flush();
}

void ConsoleSession::Write(LogLevel level, const char* tag, const char* message)
{
    if (!_sink) return;

    char lv;
    const char* color;
    const char* reset = "\x1B[0m";

    switch (level)
    {
        case LogLevel::Fatal:   lv = 'F'; color = "\x1B[1;31m"; break; // Bold Red
        case LogLevel::Error:   lv = 'E'; color = "\x1B[31m";   break; // Red
        case LogLevel::Warn:    lv = 'W'; color = "\x1B[33m";   break; // Yellow
        case LogLevel::Info:    lv = 'I'; color = "\x1B[32m";   break; // Green
        case LogLevel::Debug:   lv = 'D'; color = "\x1B[36m";   break; // Cyan
        case LogLevel::Verbose: lv = 'V'; color = "\x1B[39m";   break; // Default
        case LogLevel::Trace:   lv = 'T'; color = "\x1B[90m";   break; // Gray
        default:                lv = '?'; color = "";            break;
    }

    std::string line;
    if (_showColors) line += color;
    line += '[';
    line += lv;
    line += "][";
    line += tag;
    line += "] ";
    line += message;

    // Coalesce dual newlines: if the message already has a newline, don't add another.
    if (line.empty() || line.back() != '\n')
    {
        line += '\n';
    }

    if (_showColors) line += reset;

    WriteText(line);
    Flush();
}

//
// ConsoleManager
//

static SerialConsoleSink g_SerialSink;

ConsoleManager::ConsoleManager()
{
    _serialSession = std::make_shared<ConsoleSession>(&g_SerialSink);
}

void ConsoleManager::FeedSerialByte(char c)
{
    std::shared_ptr<ConsoleSession> session;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        session = _serialSession;
    }
    if (_byteHandler && session)
        _byteHandler(c, session);
}

void ConsoleManager::FeedTelnetByte(char c)
{
    std::shared_ptr<ConsoleSession> session;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        session = _telnetSession;
    }
    if (_byteHandler && session)
        _byteHandler(c, session);
}

void ConsoleManager::Broadcast(std::string_view data)
{
    AddDmesgLine(data);

    std::shared_ptr<ConsoleSession> serial, telnet;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        serial = _serialSession;
        telnet = _telnetSession;
    }
    if (serial) {
        serial->WriteText(data);
        serial->Flush();
    }
    if (telnet) {
        telnet->WriteText(data);
        telnet->Flush();
    }
}

void ConsoleManager::Broadcast(LogLevel level, const char* tag, const char* message)
{
    char lv;
    switch (level)
    {
        case LogLevel::Fatal:   lv = 'F'; break;
        case LogLevel::Error:   lv = 'E'; break;
        case LogLevel::Warn:    lv = 'W'; break;
        case LogLevel::Info:    lv = 'I'; break;
        case LogLevel::Debug:   lv = 'D'; break;
        case LogLevel::Verbose: lv = 'V'; break;
        case LogLevel::Trace:   lv = 'T'; break;
        default:                lv = '?'; break;
    }

    char stack_buf[256];
    int len = snprintf(stack_buf, sizeof(stack_buf), "[%c][%s] %s", lv, tag, message);
    if (len >= 0)
    {
        if ((size_t)len < sizeof(stack_buf))
        {
            AddDmesgLine(std::string_view(stack_buf, len));
        }
        else
        {
            char* heap_buf = (char*)malloc(len + 1);
            if (heap_buf)
            {
                snprintf(heap_buf, len + 1, "[%c][%s] %s", lv, tag, message);
                AddDmesgLine(std::string_view(heap_buf, len));
                free(heap_buf);
            }
        }
    }

    std::shared_ptr<ConsoleSession> serial, telnet;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        serial = _serialSession;
        telnet = _telnetSession;
    }
    if (serial) {
        serial->Write(level, tag, message);
    }
    if (telnet) {
        telnet->Write(level, tag, message);
    }
}

void ConsoleManager::AddDmesgLine(std::string_view line)
{
    size_t start = 0;
    while (start < line.size())
    {
        size_t end = line.find('\n', start);
        std::string_view sub;
        if (end == std::string_view::npos)
        {
            sub = line.substr(start);
            start = line.size();
        }
        else
        {
            sub = line.substr(start, end - start);
            start = end + 1;
        }

        while (!sub.empty() && (sub.back() == '\n' || sub.back() == '\r'))
        {
            sub.remove_suffix(1);
        }
        while (!sub.empty() && (sub.front() == '\n' || sub.front() == '\r'))
        {
            sub.remove_prefix(1);
        }

        if (sub.empty())
        {
            continue;
        }

        // Filter out the periodic 5-second status message to prevent dmesg flooding
        if (sub.contains("Mem: ") && sub.contains("LED FPS: "))
        {
            continue;
        }

        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (_dmesgBuffer.size() < DMESG_BUFFER_SIZE)
        {
            _dmesgBuffer.push_back(std::string(sub));
        }
        else
        {
            _dmesgBuffer[_dmesgHead].assign(sub.data(), sub.size());
            _dmesgHead = (_dmesgHead + 1) % DMESG_BUFFER_SIZE;
        }
    }
}

void ConsoleManager::PrintDmesg(std::function<void(const std::string&)> printFunc)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_dmesgBuffer.empty())
    {
        return;
    }

    if (_dmesgBuffer.size() < DMESG_BUFFER_SIZE)
    {
        for (const auto& line : _dmesgBuffer)
        {
            printFunc(line);
        }
    }
    else
    {
        for (size_t i = 0; i < DMESG_BUFFER_SIZE; ++i)
        {
            size_t idx = (_dmesgHead + i) % DMESG_BUFFER_SIZE;
            printFunc(_dmesgBuffer[idx]);
        }
    }
}

void ConsoleManager::SetTelnetSink(IConsoleSink* sink)
{
    std::shared_ptr<ConsoleSession> session;
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _telnetSession = std::make_shared<ConsoleSession>(sink);
        session = _telnetSession;
    }
    if (session) {
        session->SetEcho(true);
        session->SetShowColors(false);      // Default off for clean 'nc', real Telnet can call 'color on'
        DebugCLI::RunCommand("", session); // Force an initial prompt
    }
}

void ConsoleManager::ClearTelnetSink()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_telnetSession)
    {
        _telnetSession.reset();
    }
}
