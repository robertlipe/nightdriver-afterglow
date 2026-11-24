//+--------------------------------------------------------------------------
//
// File:        debug_cli.cpp
//
// NightDriverStrip - (c) 2025 Plummer's Software LLC.  All Rights Reserved.
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
//    Implementation of debugging command line interface to NightDriver.
//

#include "globals.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_vfs.h>
#if ENABLE_WIFI
#include <WiFi.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <optional>
#include <soc/soc_caps.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <vector>

#include "console.h"
#include "debug_cli.h"
#include "deviceconfig.h"
#include "effectmanager.h"
#include "gfxbase.h"
#include "ledstripeffect.h"
#include "nd_network.h"
#include "sensors.h"
#include "soundanalyzer.h"
#include "systemcontainer.h"
#include "systemhardwarehub.h"
#include "userfs.h"
#include "values.h"

namespace DebugCLI
{

//
// Private Globals & Persistence
//
static std::vector<const command *> g_CommandTable;

// persistent record in RTC RAM
static RTC_NOINIT_ATTR RTC_PanicRecord l_rtcPanicRecord;

void RecordPanicMessage(const char* message)
{
    if (message == nullptr)
    {
        return;
    }
    l_rtcPanicRecord.Magic = RTC_PANIC_MAGIC;
    strncpy(l_rtcPanicRecord.Message, message, sizeof(l_rtcPanicRecord.Message) - 1);
    l_rtcPanicRecord.Message[sizeof(l_rtcPanicRecord.Message) - 1] = '\0';
}

//
// Tokenizer (Internal)
// Uses NVRO to allocate in caller's frame to make array of pointer
// pairs of starts/ends. No copies. No allocs.
// Simple quoting like 'setname "My Cool Blinkies"' is supported.
//
static std::vector<std::string_view> Tokenize(std::string_view input)
{
    std::vector<std::string_view> output;
    constexpr std::string_view kWhitespace = " \t\r\n\f\v";

    while (true)
    {
        size_t start = input.find_first_not_of(kWhitespace);
        if (start == std::string_view::npos)
            break;

        input.remove_prefix(start);

        if (input.front() == '"')
        {
            input.remove_prefix(1);
            size_t end = input.find('"');
            output.push_back(input.substr(0, end));
            if (end == std::string_view::npos)
                break;
            input.remove_prefix(end + 1);
        }
        else
        {
            size_t end = input.find_first_of(" \t\r\n\f\v\"");
            output.push_back(input.substr(0, end));
            if (end == std::string_view::npos)
                break;
            input.remove_prefix(end);
        }
    }

    return output;
}

//
// String Comparison
//
bool StringCompareInsensitive(std::string_view sv, const char *in_s)
{
    if (!in_s)
        return false;
    const unsigned char *s = (const unsigned char *)in_s;
    for (unsigned char c : sv)
    {
        if (!*s || tolower(c) != tolower(*s))
            return false;
        s++;
    }
    return *s == '\0';
}

bool StringStartsWithInsensitive(std::string_view haystack, std::string_view needle)
{
    if (needle.length() > haystack.length())
        return false;
    return std::equal(needle.begin(), needle.end(), haystack.begin(),
                      [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
}

bool ContainsInsensitive(std::string_view haystack, std::string_view needle)
{
    if (needle.empty())
        return true;
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
    return it != haystack.end();
}


//
// Number Parsing Helpers
// Safe for non-null-terminated string_views, zero allocations.
//
template <typename T = int>
static std::optional<T> ParseInt(std::string_view str, int base = 10)
{
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())))
        str.remove_prefix(1);

    if (str.empty())
        return std::nullopt;

    T val{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val, base);
    if (ec != std::errc() || ptr != str.data() + str.size())
        return std::nullopt;

    return val;
}

//
// Registration
//
void RegisterCommand(const command *cmd)
{
    if (cmd)
        g_CommandTable.push_back(cmd);
}

void RegisterCommands(const command *cmds, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        g_CommandTable.push_back(&cmds[i]);
    }
}

//
// Core Command Helpers
//
static void PrintHelp(const cli_argv &argv)
{
    if (argv.size() > 1)
    {
        std::string_view target = argv[1];

        auto exact_it = std::find_if(g_CommandTable.begin(), g_CommandTable.end(),
                                     [target](const command *cmd) { return StringCompareInsensitive(target, cmd->command); });

        if (exact_it != g_CommandTable.end())
        {
            cli_printf("%-20s %s\n", (*exact_it)->command, (*exact_it)->help);
        }
        else
        {
            auto is_prefix_match = [target](const command *cmd) { return StringStartsWithInsensitive(cmd->command, target); };
            int matches = std::count_if(g_CommandTable.begin(), g_CommandTable.end(), is_prefix_match);

            if (matches == 1)
            {
                auto prefix_it = std::find_if(g_CommandTable.begin(), g_CommandTable.end(), is_prefix_match);
                cli_printf("%-20s %s\n", (*prefix_it)->command, (*prefix_it)->help);
            }
            else
            {
                cli_printf("Command '%.*s' not found or ambiguous.\n", (int)target.length(), target.data());
            }
        }
    }
    else
    {
        for (const auto *cmd : g_CommandTable)
        {
            cli_printf("%-20s %s\n", cmd->command, cmd->help);
        }
    }
}

//
// _activeSession - thread-local pointer to the session currently dispatching a command.
// Set by RunCommand so that cli_printf knows where to direct output.
// Falls back to Broadcast if called from outside a RunCommand context.
//
static thread_local std::shared_ptr<ConsoleSession> _activeSession;

void cli_write(std::string_view text)
{
    if (_activeSession)
        _activeSession->WriteText(text);
    else
        ConsoleManager::Instance().Broadcast(text);
}

//
// Parse and run cmd_line. Zero copies or allocations.
//
void RunCommand(std::string_view cmd_line, std::shared_ptr<ConsoleSession> session)
{
    const auto argv = Tokenize(cmd_line);

    // Pin output to this session for the duration of the command.
    std::shared_ptr<ConsoleSession> previous = _activeSession;
    _activeSession = session;

    // If command is valid
    if (!argv.empty())
    {
        auto it = std::find_if(g_CommandTable.begin(), g_CommandTable.end(),
                               [&](const command *c) { return StringCompareInsensitive(argv[0], c->command); });

        if (it != g_CommandTable.end())
        {
            if ((*it)->announcement)
            {
                session->WriteText((*it)->announcement);
                session->WriteText("\n");
            }
            (*it)->helper(argv);
        }
        else
        {
            cli_printf("Unknown command: %.*s\n", (int)argv[0].length(), argv[0].data());
        }
    }

    _activeSession = previous;

    // Always emit prompt after command execution (or empty command)
    if (session) {
        session->WriteRaw("> ");
        session->Flush();
    }
}

// If you have command 'reboot', reb<TAB> will complete "reboot". Caller will
// print a space that's not in the command until newline is found. It will
// not complete arguments, e.g. not 'reboot n<tab>' would not complete the
// 'now' if that happened to be an option.
std::string_view TabComplete(std::string_view partial, std::string_view full_line)
{
    if (partial.empty())
        return "";

    // If we're completing the first token (the command)
    if (partial.data() == full_line.data())
    {
        std::string_view match = "";
        int matches = 0;
        size_t common_len = 0;

        for (const auto *cmd : g_CommandTable)
        {
            std::string_view cmd_view{cmd->command};
            if (StringStartsWithInsensitive(cmd_view, partial))
            {
                if (matches == 0)
                {
                    match = cmd_view;
                    common_len = match.length();
                }
                else
                {
                    size_t j = partial.length();
                    size_t max_len = std::min(common_len, cmd_view.length());
                    while (j < max_len && tolower(match[j]) == tolower(cmd_view[j]))
                    {
                        j++;
                    }
                    common_len = j;
                }
                matches++;
            }
        }

        if (matches > 0 && common_len > partial.length())
            return match.substr(partial.length(), common_len - partial.length());
    }
    // If we're completing an argument for 'effect'
    else if (full_line.starts_with("effect"))
    {
        auto& effectManager = g_ptrSystem->GetEffectManager();
        std::string_view firstMatch = "";
        int matches = 0;
        size_t common_len = 0;
        const size_t effectCount = effectManager.EffectCount();

        for (size_t i = 0; i < effectCount; ++i)
        {
            const String& name = effectManager.EffectsList()[i]->FriendlyName();
            if (StringStartsWithInsensitive(name.c_str(), partial))
            {
                if (matches == 0)
                {
                    firstMatch = name.c_str();
                    common_len = firstMatch.length();
                }
                else
                {
                    size_t j = partial.length();
                    while (j < common_len && j < (size_t)name.length() &&
                           tolower(firstMatch[j]) == tolower(name[j]))
                        j++;
                    common_len = j;
                }
                matches++;
            }
        }

        if (matches > 0 && common_len > partial.length())
            return firstMatch.substr(partial.length(), common_len - partial.length());
    }

    return "";
}


//
// Helper: Resolve Effect by index or name
//
static std::optional<size_t> ResolveEffect(std::string_view arg)
{
    auto& effectManager = g_ptrSystem->GetEffectManager();

    // Try as index first
    if (auto val = ParseInt<size_t>(arg))
    {
        if (*val < effectManager.EffectCount())
        {
            return *val;
        }
        else
        {
            cli_printf("Error: Effect index %zu out of range (0-%u)\n", *val, effectManager.EffectCount() - 1);
            return std::nullopt;
        }
    }

    // Try exact name match first
    const size_t effectCount = effectManager.EffectCount();
    for (size_t i = 0; i < effectCount; ++i)
    {
        const String& name = effectManager.EffectsList()[i]->FriendlyName();
        if (StringCompareInsensitive(arg, name.c_str()))
        {
            return i;
        }
    }

    // Try fuzzy substring match
    int match_index = -1;
    int matches = 0;
    std::vector<std::string> candidates;

    for (size_t i = 0; i < effectCount; ++i)
    {
        const String& name = effectManager.EffectsList()[i]->FriendlyName();
        if (ContainsInsensitive(name.c_str(), arg))
        {
            match_index = i;
            matches++;
            candidates.push_back(name.c_str());
        }
    }

    if (matches == 1)
    {
        return (size_t)match_index;
    }
    else if (matches > 1)
    {
        cli_printf("Error: Ambiguous match for '%.*s'. Candidates:\n", (int)arg.length(), arg.data());
        for (const auto& c : candidates)
            cli_printf("  %s\n", c.c_str());
        return std::nullopt;
    }
    else
    {
        cli_printf("Error: No effect matching '%.*s' found.\n", (int)arg.length(), arg.data());
        return std::nullopt;
    }
}

//
// Log Verification Command
//
static void DoEmitMix(const cli_argv &)
{
    debugF("This is a FATAL message");
    debugE("This is an ERROR message");
    debugW("This is a WARNING message");
    debugI("This is an INFO message");
    debugD("This is a DEBUG message");
    debugV("This is a VERBOSE message");
    debugT("This is a TRACE message");
}

//
// Core Commands Table
//
static void DoEffectCommand(const cli_argv &argv)
{
    auto& effectManager = g_ptrSystem->GetEffectManager();
    cli_printf("Current Effect: %s\n", effectManager.GetCurrentEffectName().c_str());

    if (argv.size() > 1)
    {
        std::string_view arg = argv[1];

        if (arg == "next")
        {
            effectManager.NextEffect();
        }
        else if (arg == "prev")
        {
            effectManager.PreviousEffect();
        }
        else
        {
            auto idx = ResolveEffect(arg);
            if (idx)
            {
                effectManager.SetCurrentEffectIndex(*idx);
            }
            else
            {
                return;
            }
        }
        cli_printf("New Effect: %s\n", effectManager.GetCurrentEffectName().c_str());
    }
}

static void DoDirectoryListing(const cli_argv &argv)
{
    fs::FS *fileSystem = &UserFS.fs();

    fs::File root = fileSystem->open("/");

    fs::File file = root.openNextFile();
    while (file)
    {
        time_t t = file.getLastWrite();
        struct tm tm = {0};
        localtime_r(&t, &tm);

        if (file.isDirectory())
        {
            cli_printf("%-32s DIR      %d-%02d-%02d %02d:%02d:%02d\n", file.name(), (int)(tm.tm_year + 1900), (int)(tm.tm_mon + 1),
                       (int)tm.tm_mday, (int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec);
        }
        else
        {
            cli_printf("%-32s %8zu %d-%02d-%02d %02d:%02d:%02d\n", file.name(), (size_t)file.size(), (int)(tm.tm_year + 1900),
                       (int)(tm.tm_mon + 1), (int)tm.tm_mday, (int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec);
        }
        file = root.openNextFile();
    }
}

// Display a file. Hope it's text!
static void DoCat(const cli_argv &argv)
{
    if (argv.size() < 2)
    {
        cli_printf("Usage: cat <filename>\n");
        return;
    }

    std::string fname(argv[1]);
    if (fname[0] != '/')
        fname = "/" + fname;

    fs::FS *fileSystem = &UserFS.fs();
    if (!fileSystem->exists(fname.c_str()))
    {
        cli_printf("File not found: %s\n", fname.c_str());
        return;
    }

    fs::File file = fileSystem->open(fname.c_str(), "r");
    if (!file || file.isDirectory())
    {
        cli_printf("Error opening file: %s\n", fname.c_str());
        if (file)
            file.close();
        return;
    }

    char buf[256];
    while (file.available())
    {
        size_t len = file.readBytes(buf, sizeof(buf));
        if (len > 0)
        {
            cli_write(std::string_view(buf, len));
        }
    }
    // cli_printf("\n"); // Optional: Ensure newline? Left raw for now.

    file.close();
}

void DoUptime(const cli_argv &)
{
    // Microseconds since boot. File wrap bugreport in 292 million years.
    auto uptime_us = esp_timer_get_time();
    uint64_t total_seconds = uptime_us / 1000000;

    uint32_t days    = (uint32_t)(total_seconds / 86400);
    uint32_t hours   = (uint32_t)((total_seconds % 86400) / 3600);
    uint32_t minutes = (uint32_t)((total_seconds % 3600) / 60);
    uint32_t seconds = (uint32_t)(total_seconds % 60);

    cli_printf("Uptime: %u days, %02u:%02u:%02u\n", (unsigned int)days, (unsigned int)hours, (unsigned int)minutes, (unsigned int)seconds);

    if (l_rtcPanicRecord.Magic == RTC_PANIC_MAGIC)
    {
        cli_printf("Persistent Panic Message: %s\n", l_rtcPanicRecord.Message);
        l_rtcPanicRecord.Magic = 0; // Clear it so we don't report it every time uptime is called
    }

    const char* reason_text = "Unknown";
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason)
    {
        case ESP_RST_POWERON: reason_text = "Power On"; break;
        case ESP_RST_EXT: reason_text = "External Pin"; break;
        case ESP_RST_SW: reason_text = "Software Restart"; break;
        case ESP_RST_PANIC: reason_text = "Panic"; break;
        case ESP_RST_INT_WDT: reason_text = "Watchdog barked"; break;
        case ESP_RST_TASK_WDT: reason_text = "Task Watchdog barked"; break;
        case ESP_RST_WDT: reason_text = "Other Watchdog barked"; break;
        case ESP_RST_DEEPSLEEP: reason_text = "Reset in deep sleep"; break;
        case ESP_RST_BROWNOUT: reason_text = "Brownout"; break;
        case ESP_RST_SDIO: reason_text = "Reset over SDIO"; break;
        // Documented,  but not defined in ESP-IDF esp_system.h (v5.1.0) yet.
        // case ESP_RST_USB: reason_text = "Reset by USB peripheral"; break;
        default: reason_text = "Unknown"; break;
    }
    cli_printf("Last boot reason: (%d): %s\n", (int)reason, reason_text);
}

static void DoClearCoredump(const cli_argv &)
{
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (part != NULL)
    {
        cli_printf("Erasing coredump partition (%lu bytes)...\n", (unsigned long)part->size);
        esp_err_t err = esp_partition_erase_range(part, 0, part->size);
        if (err == ESP_OK)
        {
            cli_printf("Coredump cleared.\n");
        }
        else
        {
            cli_printf("Error erasing partition: %d\n", (int)err);
        }
    }
    else
    {
        cli_printf("Coredump partition not found.\n");
    }
}

static void DoPartitions(const cli_argv &)
{
    cli_printf("%-16s %-10s %-10s %-10s %-10s\n", "Name", "Type", "Sub", "Offset", "Size");
    cli_printf("----------------------------------------------------------------------\n");

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL)
    {
        const esp_partition_t* p = esp_partition_get(it);
        cli_printf("%-16s 0x%02x       0x%02x       0x%08lx 0x%08lx (%lu KB)\n",
                   p->label, (unsigned int)p->type, (unsigned int)p->subtype, (unsigned long)p->address, (unsigned long)p->size, (unsigned long)p->size / 1024);
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
}

static void DoCrashMe(const cli_argv& argv)
{
    std::string_view mode = (argv.size() > 1) ? argv[1] : "null";

    if (mode == "null")
    {
        cli_printf("Writing 0xDEADBEEF to NULL...\n");
        delay(100);
        *(volatile int*)0 = 0xDEADBEEF;
    }
    else if (mode == "abort")
    {
        cli_printf("Calling abort()...\n");
        delay(100);
        abort();
    }
    else if (mode == "wdt")
    {
        cli_printf("Stalling in tight loop (Task WDT should trip)...\n");
        delay(100);
        while(true);
    }
    else
    {
        cli_printf("Unknown mode: %.*s (try null, abort, wdt)\n", (int)mode.length(), mode.data());
    }
}

static void DoSetPanic(const cli_argv& argv)
{
    if (argv.size() < 2)
    {
        cli_printf("Usage: setpanic <message>\n");
        return;
    }

    RecordPanicMessage(std::string(argv[1]).c_str());
    cli_printf("Persistent panic message set to: %s\n", l_rtcPanicRecord.Message);
}

static void DoDmesg(const cli_argv&)
{
    ConsoleManager::Instance().PrintDmesg([](const std::string& line) {
        cli_write(line);
        cli_write("\n");
    });
}

static void DoStatusLog(const cli_argv& argv)
{
    if (argv.size() > 1)
    {
        std::string_view arg = argv[1];
        if (arg == "on")
        {
            g_Values.ShowStatusLog = true;
            cli_printf("Periodic status logs enabled.\n");
        }
        else if (arg == "off")
        {
            g_Values.ShowStatusLog = false;
            cli_printf("Periodic status logs disabled.\n");
        }
        else
        {
            cli_printf("Usage: statuslog [on|off]\n");
        }
    }
    else
    {
        cli_printf("Periodic status logs: %s\n", g_Values.ShowStatusLog ? "ON" : "OFF");
    }
}

template <typename F>
static void StreamToCli(F &&fn)
{
    FILE *f = funopen(
        nullptr,
        nullptr,
        [](void *, const char *buf, int size) -> int {
            if (size > 0)
            {
                cli_write(std::string_view(buf, static_cast<size_t>(size)));
            }
            return size;
        },
        nullptr,
        nullptr);

    if (!f)
    {
        cli_printf("Error: Insufficient memory to open output stream.\n");
        fn(stdout);
        return;
    }

    FILE *old_stdout = stdout;
    stdout = f;
    fn(f);
    fclose(f);
    stdout = old_stdout;
}

static void DoHeap(const cli_argv & = {})
{
    StreamToCli([](FILE *f) {
        fprintf(f, "--- Default Heap (MALLOC_CAP_DEFAULT) ---\n");
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
#if USE_PSRAM
        if (psramFound())
        {
            fprintf(f, "\n--- PSRAM (External SPIRAM) ---\n");
            heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
        }
#endif
    });
}

static bool TryParseGpioPin(std::string_view token, int &outPin)
{
    auto pin = ParseInt<int>(token);
    if (!pin || *pin < 0 || *pin >= SOC_GPIO_PIN_COUNT || !GPIO_IS_VALID_GPIO(*pin))
        return false;

    outPin = *pin;
    return true;
}

static void DoGpio(const cli_argv &argv)
{
    uint64_t mask = SOC_GPIO_VALID_GPIO_MASK;
    if (argv.size() > 1)
    {
        int pin = -1;
        if (!TryParseGpioPin(argv[1], pin))
        {
            cli_printf("Invalid GPIO pin: '%.*s'\n", (int)argv[1].size(), argv[1].data());
            return;
        }
        mask = (1ULL << pin);
    }

    StreamToCli([mask](FILE *f) {
        gpio_dump_io_configuration(f, mask);
    });
}

static void DoDump(const cli_argv &argv)
{
    if (argv.size() < 2)
    {
        cli_printf("Usage: dump [fds|gpio [pin]|heap|intr|tasks|timers]\n");
        return;
    }

    std::string_view sub = argv[1];
    if (StringCompareInsensitive(sub, "gpio"))
    {
        cli_argv gpio_argv(argv.begin() + 1, argv.end());
        DoGpio(gpio_argv);
    }
    else if (StringCompareInsensitive(sub, "heap"))
    {
        DoHeap();
    }
    else if (StringCompareInsensitive(sub, "intr") || StringCompareInsensitive(sub, "interrupts"))
    {
        StreamToCli([](FILE *f) {
            esp_intr_dump(f);
        });
    }
    else if (StringCompareInsensitive(sub, "timers"))
    {
        StreamToCli([](FILE *f) {
            esp_timer_dump(f);
        });
    }
    else if (StringCompareInsensitive(sub, "fds") || StringCompareInsensitive(sub, "vfs"))
    {
        StreamToCli([](FILE *f) {
            fprintf(f, "--- Open File Descriptors ---\n");
            esp_vfs_dump_fds(f);
            fprintf(f, "\n--- Registered VFS Mounts ---\n");
            esp_vfs_dump_registered_paths(f);
        });
    }
#if configUSE_TRACE_FACILITY && configUSE_STATS_FORMATTING_FUNCTIONS
    else if (StringCompareInsensitive(sub, "tasks"))
    {
        char buf[1024];
        vTaskList(buf);
        cli_write("\nName          State      Prio  Stack  Num Core\n");
        cli_write(buf);
    }
#endif
    else
    {
        cli_printf("Unknown dump target: '%.*s'\n", (int)sub.size(), sub.data());
        cli_printf("Usage: dump [fds|gpio [pin]|heap|intr|tasks|timers]\n");
    }
}

static const char* GetPinConflictDescription(int pin)
{
#if defined(IR_REMOTE_PIN) && IR_REMOTE_PIN >= 0
    if (pin == IR_REMOTE_PIN) return "IR Remote (IR_REMOTE_PIN)";
#endif
#if defined(I2S_BCLK_PIN) && I2S_BCLK_PIN >= 0
    if (pin == I2S_BCLK_PIN) return "I2S Bit Clock (I2S_BCLK_PIN)";
#endif
#if defined(I2S_WS_PIN) && I2S_WS_PIN >= 0
    if (pin == I2S_WS_PIN) return "I2S Word Select (I2S_WS_PIN)";
#endif
#if defined(I2S_DATA_IN_PIN) && I2S_DATA_IN_PIN >= 0
    if (pin == I2S_DATA_IN_PIN) return "I2S Data In (I2S_DATA_IN_PIN)";
#endif
#if defined(DHT11_PIN) && DHT11_PIN >= 0
    if (pin == DHT11_PIN) return "DHT11 Sensor (DHT11_PIN)";
#endif
    return nullptr;
}

static void DoStrip(const cli_argv &argv)
{
    auto &config = g_ptrSystem->GetDeviceConfig();
    const auto &strips = config.GetStrips();
    static constexpr std::string_view kApplyNotice = " Run 'strip apply' to commit and restart.\n";

    auto printUsage = []() {
        cli_write("Usage:\n"
                  "  strip [list]                              - List configured channels\n"
                  "  strip set <ch> pin <pin>                  - Set data GPIO pin\n"
                  "  strip set <ch> length <n>                 - Set pixel count\n"
                  "  strip set <ch> order <RGB|GRB|...>        - Set color order\n"
                  "  strip set <ch> chipset <WS2812B|WS2811|..>- Set chipset\n"
                  "  strip set <ch> enable <on|off>            - Enable or disable channel\n"
                  "  strip add <pin> <len> [order] [chipset]   - Add a new channel\n"
                  "  strip remove <ch>                         - Remove a channel\n"
                  "  strip apply                               - Save to /device.cfg and restart\n");
    };

    auto printStripsTable = [&strips]() {
        if (strips.empty())
        {
            cli_write("No LED strips configured.\n");
            return;
        }
        cli_write("Ch   Pin Clock Length Order   Chipset  Status\n"
                  "--   --- ----- ------ -----   -------  ------\n");
        for (size_t i = 0; i < strips.size(); ++i)
        {
            const auto &s = strips[i];
            char clockBuf[16] = "--";
            if (s.clockPin >= 0)
                snprintf(clockBuf, sizeof(clockBuf), "%d", s.clockPin);

            cli_printf("%2zu  %4d  %4s %6u %-7s %-8s %s\n",
                       i, s.dataPin, clockBuf, s.length,
                       ColorOrderToString(s.colorOrder),
                       ChipsetToString(s.chipset),
                       s.enabled ? "ENABLED" : "DISABLED");
        }
    };

    if (argv.size() < 2 || StringCompareInsensitive(argv[1], "list"))
    {
        printStripsTable();
        return;
    }

    std::string_view sub = argv[1];

    if (StringCompareInsensitive(sub, "apply"))
    {
        cli_write("Saving configuration to /device.cfg...\n");
        config.FlushToDisk();
        cli_write("Restarting system to reinitialize LED channels...\n");
        esp_restart();
        return;
    }

    if (StringCompareInsensitive(sub, "set"))
    {
        if (argv.size() < 5)
        {
            printUsage();
            return;
        }

        auto chOpt = ParseInt<size_t>(argv[2]);
        if (!chOpt || *chOpt >= strips.size())
        {
            cli_printf("Invalid channel index: '%.*s' (valid: 0..%zu)\n",
                       (int)argv[2].size(), argv[2].data(), strips.empty() ? 0 : strips.size() - 1);
            return;
        }

        size_t ch = *chOpt;
        StripConfig sc = strips[ch];
        std::string_view prop = argv[3];

        if (StringCompareInsensitive(prop, "pin"))
        {
            int pin = -1;
            if (!TryParseGpioPin(argv[4], pin))
            {
                cli_printf("Invalid GPIO pin: '%.*s'\n", (int)argv[4].size(), argv[4].data());
                return;
            }
            if (const char *conflict = GetPinConflictDescription(pin))
            {
                cli_printf("Warning: GPIO %d is already assigned to %s!\n", pin, conflict);
            }
            sc.dataPin = pin;
            config.SetStrip(ch, sc);
            cli_printf("Channel %zu pin set to %d.", ch, pin);
            cli_write(kApplyNotice);
        }
        else if (StringCompareInsensitive(prop, "length") || StringCompareInsensitive(prop, "count"))
        {
            auto lenOpt = ParseInt<uint16_t>(argv[4]);
            if (!lenOpt || *lenOpt == 0)
            {
                cli_printf("Invalid length: '%.*s' (must be > 0)\n", (int)argv[4].size(), argv[4].data());
                return;
            }
            sc.length = *lenOpt;
            config.SetStrip(ch, sc);
            cli_printf("Channel %zu length set to %u.", ch, sc.length);
            cli_write(kApplyNotice);
        }
        else if (StringCompareInsensitive(prop, "order"))
        {
            auto orderOpt = ParseColorOrder(argv[4]);
            if (!orderOpt)
            {
                cli_printf("Invalid color order: '%.*s' (valid: RGB, RBG, GRB, GBR, BRG, BGR)\n",
                           (int)argv[4].size(), argv[4].data());
                return;
            }
            sc.colorOrder = *orderOpt;
            config.SetStrip(ch, sc);
            cli_printf("Channel %zu color order set to %s.", ch, ColorOrderToString(sc.colorOrder));
            cli_write(kApplyNotice);
        }
        else if (StringCompareInsensitive(prop, "chipset"))
        {
            auto csOpt = ParseChipsetType(argv[4]);
            if (!csOpt)
            {
                cli_printf("Invalid chipset: '%.*s' (valid: WS2812B, WS2811, WS2815, APA102, SK9822)\n",
                           (int)argv[4].size(), argv[4].data());
                return;
            }
            sc.chipset = *csOpt;
            config.SetStrip(ch, sc);
            cli_printf("Channel %zu chipset set to %s.", ch, ChipsetToString(sc.chipset));
            cli_write(kApplyNotice);
        }
        else if (StringCompareInsensitive(prop, "enable"))
        {
            std::string_view val = argv[4];
            bool enable = (StringCompareInsensitive(val, "on") ||
                           StringCompareInsensitive(val, "true") ||
                           StringCompareInsensitive(val, "1") ||
                           StringCompareInsensitive(val, "yes"));
            sc.enabled = enable;
            config.SetStrip(ch, sc);
            cli_printf("Channel %zu %s.", ch, enable ? "enabled" : "disabled");
            cli_write(kApplyNotice);
        }
        else
        {
            cli_printf("Unknown property: '%.*s'. Valid: pin, length, order, chipset, enable\n",
                       (int)prop.size(), prop.data());
        }
        return;
    }

    if (StringCompareInsensitive(sub, "add"))
    {
        if (argv.size() < 4)
        {
            cli_write("Usage: strip add <pin> <length> [order] [chipset]\n");
            return;
        }

        int pin = -1;
        if (!TryParseGpioPin(argv[2], pin))
        {
            cli_printf("Invalid GPIO pin: '%.*s'\n", (int)argv[2].size(), argv[2].data());
            return;
        }

        auto lenOpt = ParseInt<uint16_t>(argv[3]);
        if (!lenOpt || *lenOpt == 0)
        {
            cli_printf("Invalid length: '%.*s' (must be > 0)\n", (int)argv[3].size(), argv[3].data());
            return;
        }

        StripConfig sc;
        sc.dataPin = pin;
        sc.length = *lenOpt;
        sc.colorOrder = EOrder::GRB;
        sc.chipset = ChipsetType::WS2812B;
        sc.enabled = true;

        if (argv.size() > 4)
        {
            if (auto orderOpt = ParseColorOrder(argv[4]))
                sc.colorOrder = *orderOpt;
            else
            {
                cli_printf("Invalid color order: '%.*s'\n", (int)argv[4].size(), argv[4].data());
                return;
            }
        }

        if (argv.size() > 5)
        {
            if (auto csOpt = ParseChipsetType(argv[5]))
                sc.chipset = *csOpt;
            else
            {
                cli_printf("Invalid chipset: '%.*s'\n", (int)argv[5].size(), argv[5].data());
                return;
            }
        }

        if (const char *conflict = GetPinConflictDescription(pin))
        {
            cli_printf("Warning: GPIO %d is already assigned to %s!\n", pin, conflict);
        }

        config.AddStrip(sc);
        cli_printf("Added channel %zu (pin %d, len %u, %s, %s).",
                   config.GetStrips().size() - 1, sc.dataPin, sc.length,
                   ColorOrderToString(sc.colorOrder), ChipsetToString(sc.chipset));
        cli_write(kApplyNotice);
        return;
    }

    if (StringCompareInsensitive(sub, "remove"))
    {
        if (argv.size() < 3)
        {
            cli_write("Usage: strip remove <ch>\n");
            return;
        }

        auto chOpt = ParseInt<size_t>(argv[2]);
        if (!chOpt || *chOpt >= strips.size())
        {
            cli_printf("Invalid channel index: '%.*s'\n", (int)argv[2].size(), argv[2].data());
            return;
        }

        size_t ch = *chOpt;
        config.RemoveStrip(ch);
        cli_printf("Removed channel %zu.", ch);
        cli_write(kApplyNotice);
        return;
    }

    printUsage();
}

static const command core_commands[] = {
    {"bright", "[level] Display/Set brightness 0-255", "Brightness:",
     [](const cli_argv &argv) {
         if (argv.size() > 1)
         {
             if (auto val = ParseInt<int>(argv[1]))
                 g_ptrSystem->GetDeviceConfig().SetBrightness(std::clamp(*val, 0, 255));
             else
                 cli_printf("Invalid brightness: '%.*s'\n", (int)argv[1].size(), argv[1].data());
         }
         cli_printf("Brightness: %lu\n", (unsigned long)g_ptrSystem->GetDeviceConfig().GetBrightness());
     }},
    {"cat", "Display file content", "Printing file...", DoCat},
    {"clearcoredump", "Erase the coredump partition", "Clearing...", DoClearCoredump},
    {"clearsettings", "Reset persisted user settings", "Removing persisted settings",
     [](const cli_argv &) {
         g_ptrSystem->GetDeviceConfig().RemovePersisted();
         RemoveEffectManagerConfig(); // Helper from effectmanager.h
         #if ENABLE_WIFI
         nd_network::ClearWiFiConfig(nd_network::WifiCredSource::CompileTimeCreds);
         nd_network::ClearWiFiConfig(nd_network::WifiCredSource::ImprovCreds);
         nd_network::ClearWiFiConfig(nd_network::WifiCredSource::CaptivePortal);
         nd_network::SetWiFiMode(nd_network::WiFiMode::Off);
         nd_network::RequestSystemReboot(1000);
         #endif
     }},

    {"color", "[on|off] | [r g b | hex] Set or show colors", "Global Color:",
     [](const cli_argv &argv) {
         if (argv.size() > 1)
         {
             std::string_view firstArg = argv[1];
             if (firstArg == "on" || firstArg == "off")
             {
                 if (_activeSession)
                 {
                     _activeSession->SetShowColors(firstArg == "on");
                     cli_printf("Log colorization is now %s for this session.\n", _activeSession->ShowColors() ? "enabled" : "disabled");
                 }
                 return;
             }

             CRGB newColor = g_ptrSystem->GetDeviceConfig().GlobalColor();
             if (argv.size() == 2) // Assume hex if one arg
             {
                 std::string_view hexStr = argv[1];
                 if (!hexStr.empty() && hexStr.front() == '#')
                     hexStr.remove_prefix(1);
                 if (auto val = ParseInt<uint32_t>(hexStr, 16))
                     newColor = CRGB(*val);
                 else
                     cli_printf("Invalid hex color: '%.*s'\n", (int)argv[1].size(), argv[1].data());
             }
             else if (argv.size() == 4) // Assume R G B
             {
                 auto r = ParseInt<uint8_t>(argv[1]);
                 auto g = ParseInt<uint8_t>(argv[2]);
                 auto b = ParseInt<uint8_t>(argv[3]);
                 if (r && g && b)
                     newColor = CRGB(*r, *g, *b);
                 else
                     cli_printf("Invalid RGB components.\n");
             }
             g_ptrSystem->GetDeviceConfig().SetGlobalColor(newColor);
         }
         CRGB c = g_ptrSystem->GetDeviceConfig().GlobalColor();
         cli_printf("Global Color: R:%d G:%d B:%d (0x%02X%02X%02X)\n", c.r, c.g, c.b, c.r, c.g, c.b);
         if (_activeSession)
         {
             cli_printf("Log colors: %s\n", _activeSession->ShowColors() ? "ON" : "OFF");
         }
     }},
    {"crashme", "[null|abort|wdt] Trigger a system crash", "Crashing...", DoCrashMe},
    {"debug", "emix | log ... Debugging utilities", nullptr,
     [](const cli_argv &argv) {
        if (argv.size() > 1 && StringCompareInsensitive(argv[1], "emix")) {
            DoEmitMix(argv);
        } else {
            cli_printf("Usage: debug emix\n");
        }
     }},
    {"dmesg", "Display the circular log buffer", "dmesg:", DoDmesg},
    {"dump", "[fds|gpio|heap|intr|tasks|timers] Dump diagnostic state", nullptr, DoDump},
    {"effect", "[next|prev|name|index] Show/change current effect", "Effects.", DoEffectCommand}, // Function pointer
    {"gpio", "[pin] Display GPIO configuration and muxing", "GPIO configuration:", DoGpio},
    {"heap", "Display heap memory info", "Heap usage:", DoHeap},
    {"help", "Display command line options", "Displaying system help",
     PrintHelp}, // Function pointer logic requires PrintHelp signature match. It does.
    {"log", "[tag] <level> Get/set log level", nullptr,
     [](const cli_argv &argv) {
         static const struct
         {
             const char *name;
             esp_log_level_t level;
         } levels[] = {{"none", ESP_LOG_NONE}, {"error", ESP_LOG_ERROR}, {"warn", ESP_LOG_WARN},
                       {"info", ESP_LOG_INFO}, {"debug", ESP_LOG_DEBUG}, {"verbose", ESP_LOG_VERBOSE}};

         if (argv.size() < 2)
         {
             // No args: show the current global log level
             esp_log_level_t current = esp_log_level_get("*");
             const char *name = "unknown";
             for (const auto &l : levels)
                 if (l.level == current) { name = l.name; break; }
             cli_printf("Log level: %s\n", name);
             return;
         }
         std::string_view levelStr = argv.back();
         std::string_view tag = "*";
         if (argv.size() > 2)
             tag = argv[1];

         for (const auto &l : levels)
         {
             if (StringCompareInsensitive(levelStr, l.name))
             {
                 esp_log_level_set(std::string(tag).c_str(), l.level);
                 cli_printf("Set level for tag '%s' to %d (%s)\n", std::string(tag).c_str(), l.level, l.name);
                 return;
             }
         }
         cli_printf("Invalid level '%s'. Valid: none error warn info debug verbose\n", std::string(levelStr).c_str());
     }},
    {"ls", "Show filesytem directory", "NAME", DoDirectoryListing},                    // Function pointer
    {"partitions", "Display partition table", "Partition Table:", DoPartitions},
    {"reboot", "Reboot system", "Rebooting. Please stand by...", [](const cli_argv &) { esp_restart(); }},
#if USE_MATRIX
    {"rotate", "[0|90|180|270] | [auto [on|off]] Get or set display orientation", "Orientation:",
     [](const cli_argv &argv) {
#if HAS_SYSTEM_HARDWARE_HUB
         if (argv.size() > 1)
         {
             std::string_view arg = argv[1];
             if (arg == "auto")
             {
                 bool enable = true;
                 if (argv.size() > 2)
                 {
                     std::string_view sub = argv[2];
                     if (sub == "off" || sub == "0")
                         enable = false;
                 }
                 SystemHardwareHub::Instance().SetAutoOrientationEnabled(enable);
                 cli_printf("Auto-orientation %s.\n", enable ? "enabled" : "disabled");
             }
             else if (arg == "0" || arg == "normal")
             {
                 SystemHardwareHub::Instance().SetManualRotation(0);
                 cli_printf("Display rotation set to normal (0 deg, auto-orientation paused).\n");
             }
             else if (arg == "90")
             {
                 SystemHardwareHub::Instance().SetManualRotation(1);
                 cli_printf("Display rotation set to 90 deg (auto-orientation paused).\n");
             }
             else if (arg == "180" || arg == "inverted")
             {
                 SystemHardwareHub::Instance().SetManualRotation(2);
                 cli_printf("Display rotation set to inverted (180 deg, auto-orientation paused).\n");
             }
             else if (arg == "270")
             {
                 SystemHardwareHub::Instance().SetManualRotation(3);
                 cli_printf("Display rotation set to 270 deg (auto-orientation paused).\n");
             }
             else
             {
                 cli_printf("Usage: rotate [0|90|180|270] | [auto [on|off]]\n");
             }
         }
         else
         {
             uint8_t rot = SystemHardwareHub::Instance().GetCurrentRotation();
             int deg = (rot == 1) ? 90 : (rot == 2) ? 180 : (rot == 3) ? 270 : 0;
             bool is_auto = SystemHardwareHub::Instance().IsAutoOrientationEnabled();
             cli_printf("Rotation: %d deg, Auto-orientation: %s\n",
                        deg, is_auto ? "enabled" : "disabled");
         }
#else
         if (g_ptrSystem && g_ptrSystem->HasDevices() && argv.size() > 1)
         {
             std::string_view arg = argv[1];
             uint8_t rot = 0;
             if (arg == "90" || arg == "1") rot = 1;
             else if (arg == "180" || arg == "2") rot = 2;
             else if (arg == "270" || arg == "3") rot = 3;
             for (auto &dev : g_ptrSystem->GetDevices())
                 dev->setRotation(rot);
             int deg = (rot == 1) ? 90 : (rot == 2) ? 180 : (rot == 3) ? 270 : 0;
             cli_printf("Display rotation set to %d deg.\n", deg);
         }
         else
         {
             cli_printf("Usage: rotate [0|90|180|270]\n");
         }
#endif
     }},
#endif
    {"setpanic", "<msg> Set persistent panic message for next boot", "Setting...", DoSetPanic},
    {"simbeat", "[on|off] [bpm] Simulate audio beat", "Simulate Beat:",
     [](const cli_argv &argv) {
        if (argv.size() > 1) {
            if (StringCompareInsensitive(argv[1], "on")) g_Analyzer.SetSimulateBeat(true);
            else if (StringCompareInsensitive(argv[1], "off")) g_Analyzer.SetSimulateBeat(false);
            else if (auto bpm = ParseInt<int>(argv[1])) g_Analyzer.SetSimulateBPM(*bpm);
        }
        if (argv.size() > 2) {
             if (auto bpm = ParseInt<int>(argv[2])) g_Analyzer.SetSimulateBPM(*bpm);
        }
        cli_printf("SimBeat: %s  BPM: %d\n", g_Analyzer.GetSimulateBeat() ? "ON" : "OFF", g_Analyzer.GetSimulateBPM());
     }},
    {"settings", "List current effect settings", "Effect Settings:", [](const cli_argv&) {
        auto& effect = g_ptrSystem->GetEffectManager().GetCurrentEffect();
        const auto& specs = effect.GetSettingSpecs();
        if (!specs.empty()) {
            for (const auto& spec_ref : specs) {
                const auto& spec = spec_ref.get();
                cli_printf("  %s (%s)\n", spec.Name, spec.FriendlyName ? spec.FriendlyName : "");
            }
        } else {
            cli_printf("  No settings for this effect.\n");
        }
    }},
    {"setsetting", "<key> <value> Set property of current effect", "Setting property:", [](const cli_argv& argv) {
        if (argv.size() < 3) {
            cli_printf("Usage: setsetting <key> <value>\n");
            return;
        }
        auto& effect = g_ptrSystem->GetEffectManager().GetCurrentEffect();
        if (effect.SetSetting(String(argv[1].data(), argv[1].length()), String(argv[2].data(), argv[2].length()))) {
            cli_printf("Successfully set %.*s to %.*s.\n", (int)argv[1].length(), argv[1].data(), (int)argv[2].length(), argv[2].data());
            SaveEffectManagerConfig();
        } else {
            cli_printf("Failed to set %.*s.\n", (int)argv[1].length(), argv[1].data());
        }
    }},
    {"statuslog", "[on|off] Show/change periodic status logs", "Status logs.", DoStatusLog},
    {"strip", "[list|set|add|remove|apply] Configure LED strip channels", "LED Strips:", DoStrip},
#if HAS_SYSTEM_HARDWARE_HUB
    {"rtc", "[sync|read] Read or synchronize PCF85063 real-time clock", "RTC status:",
     [](const cli_argv &argv) {
         if (argv.size() > 1 && StringCompareInsensitive(argv[1], "sync"))
         {
             if (SystemHardwareHub::Instance().SyncRTCTimeFromSystem())
             {
                 cli_printf("RTC synchronized from system time.\n");
             }
             else
             {
                 cli_printf("Failed to synchronize RTC from system time.\n");
             }
             return;
         }

         struct tm timeinfo = {};
         if (SystemHardwareHub::Instance().ReadRTC(timeinfo))
         {
             char buf[64];
             strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
             cli_printf("PCF85063 RTC time: %s\n", buf);
         }
         else
         {
             cli_printf("PCF85063 RTC: failed to read or oscillator stopped.\n");
         }

         time_t now = time(nullptr);
         struct tm sys_tm = {};
         localtime_r(&now, &sys_tm);
         char sys_buf[64];
         strftime(sys_buf, sizeof(sys_buf), "%Y-%m-%d %H:%M:%S %Z", &sys_tm);
         cli_printf("System local time: %s (epoch: %lld)\n", sys_buf, (long long)now);
     }},
#endif
#if HAS_SYSTEM_HARDWARE_HUB
    {"tap", "[on|off] Enable/disable bezel tap to advance effect", "Tap detection:",
     [](const cli_argv &argv) {
         if (argv.size() > 1)
         {
             std::string_view arg = argv[1];
             if (arg == "on" || arg == "1")
             {
                 SystemHardwareHub::Instance().SetTapDetectionEnabled(true);
                 cli_printf("Bezel tap detection enabled.\n");
             }
             else if (arg == "off" || arg == "0")
             {
                 SystemHardwareHub::Instance().SetTapDetectionEnabled(false);
                 cli_printf("Bezel tap detection disabled.\n");
             }
             else
             {
                 cli_printf("Usage: tap [on|off]\n");
             }
         }
         else
         {
             cli_printf("Tap detection: %s\n",
                        SystemHardwareHub::Instance().IsTapDetectionEnabled() ? "enabled" : "disabled");
         }
     }},
#endif
#if configUSE_TRACE_FACILITY && configUSE_STATS_FORMATTING_FUNCTIONS
    {"tasks", "Display FreeRTOS task list", "Task List:",
     [](const cli_argv &) {
         char buf[1024];
         vTaskList(buf);
         cli_write("\nName          State      Prio  Stack  Num Core\n");
         cli_write(buf);
     }},
#endif
    {"tempoffset", "[degrees] View or set ambient temperature calibration offset (°F)", "Temperature offset:",
     [](const cli_argv &argv) {
         if (argv.size() > 1)
         {
             char *endptr = nullptr;
             std::string argStr(argv[1]);
             float offset = std::strtof(argStr.c_str(), &endptr);
             if (endptr == argStr.c_str())
             {
                 cli_printf("Invalid temperature offset: %s\n", argStr.c_str());
                 return;
             }
             g_ptrSystem->GetDeviceConfig().SetTempOffset(offset);
             cli_printf("Ambient temperature offset set to %.1f °F\n", offset);
             auto rawOpt = SensorManager::GetRawAmbientTemp();
             if (rawOpt.has_value())
             {
                 g_Values.AmbientTemp = *rawOpt + offset;
                 cli_printf("Calibrated ambient temperature: %.1f °F (Raw: %.1f °F)\n",
                            *g_Values.AmbientTemp, *rawOpt);
             }
         }
         else
         {
             cli_printf("Ambient temperature offset: %.1f °F\n",
                        g_ptrSystem->GetDeviceConfig().GetTempOffset());
             if (g_Values.AmbientTemp.has_value())
             {
                 cli_printf("Current calibrated ambient temp: %.1f °F\n",
                            *g_Values.AmbientTemp);
             }
         }
     }},
    {"thermal", "Display thermal safety and throttling status", "Thermal status:",
     [](const cli_argv &) {
         cli_printf("Thermal State: %s (Throttle: %d/255, %3.0f%%)\n",
                    SensorManager::GetThermalStateName(),
                    g_Values.ThermalThrottle,
                    (g_Values.ThermalThrottle / 2.55));
         if (g_Values.InternalTemp.has_value())
         {
             cli_printf("  Die Temp:     %.1f °F (Rate: %+.1f °F/min)\n",
                        *g_Values.InternalTemp,
                        SensorManager::GetDieRiseRate());
         }
         if (g_Values.AmbientTemp.has_value())
         {
             auto raw = SensorManager::GetRawAmbientTemp().value_or(*g_Values.AmbientTemp);
             cli_printf("  Ambient Temp: %.1f °F (Raw: %.1f °F, Offset: %+.1f °F, Rate: %+.1f °F/min)\n",
                        *g_Values.AmbientTemp,
                        raw,
                        g_ptrSystem->GetDeviceConfig().GetTempOffset(),
                        SensorManager::GetAmbientRiseRate());
         }
     }},
    {"uptime", "Show system uptime", "Showing uptime...", DoUptime},
    {"+", "Nudge brightness up", "Brightness +10", [](const cli_argv &) {
        int val = std::min(255, (int)g_ptrSystem->GetDeviceConfig().GetBrightness() + 10);
        g_ptrSystem->GetDeviceConfig().SetBrightness(val);
        cli_printf("Brightness: %d\n", val);
    }},
    {"-", "Nudge brightness down", "Brightness -10", [](const cli_argv &) {
        int val = std::max(0, (int)g_ptrSystem->GetDeviceConfig().GetBrightness() - 10);
        g_ptrSystem->GetDeviceConfig().SetBrightness(val);
        cli_printf("Brightness: %d\n", val);
    }},
    {"?", "Display command line options", "Displaying system help",
     PrintHelp}
};


//
// Helper: cli_printf routes output to the session that originated the current
// command (_activeSession), falling back to Broadcast if called outside a
// RunCommand context.
//
void cli_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // Optimized buffer handling: use a 256-byte stack buffer for typical output.
    // Use heap only for large payloads like 'tasks' or 'network stats'.
    char stack_buf[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args_copy);
    va_end(args_copy);

    if (len < 0)
    {
        va_end(args);
        return;
    }

    char* buf = stack_buf;
    bool must_free = false;

    if (static_cast<size_t>(len) >= sizeof(stack_buf))
    {
        buf = static_cast<char*>(malloc(len + 1));
        if (!buf)
        {
            va_end(args);
            return; // OOM: drop output
        }
        must_free = true;
        vsnprintf(buf, len + 1, fmt, args);
    }
    va_end(args);

    // Direct output to the session that originated the current command, if any.
    // This prevents commands run on telnet from also spewing on serial and vice versa.
    cli_write(std::string_view(buf, static_cast<size_t>(len)));

    if (must_free)
        free(buf);
}

void ProcessCLIByte(char c, std::shared_ptr<ConsoleSession> session)
{
    if (!session) return;
    std::string& cmd = session->StringBuffer();

    switch (c)
    {
    case '\t':
    {
        std::string_view partial(cmd);
        if (size_t lastSpace = partial.rfind(' '); lastSpace != std::string_view::npos)
            partial.remove_prefix(lastSpace + 1);
        std::string_view suffix = TabComplete(partial, cmd);
        if (!suffix.empty())
        {
            cmd += suffix;
            cmd += " ";
            session->WriteRaw(suffix);
            session->WriteRaw(" ");
        }
        break;
    }
    case '\b':
    case 0x7f:
        if (!cmd.empty())
        {
            cmd.pop_back();
            session->WriteRaw("\b \b");
        }
        break;

    case '\r':
    {
        // CR triggers command dispatch. Echo the newline so the terminal
        // moves to the next line before command output appears.
        session->WriteRaw("\r\n");
        RunCommand(cmd, session);
        cmd.clear();
        break;
    }

    case '\n':
        // Swallow bare LF — it's the second byte of a \r\n pair from a
        // terminal, or a stray newline. The \r already dispatched the command.
        break;

    default:
        if (session->EchoEnabled())
            session->WriteRaw(c);
        cmd += c;
        break;
    }
}

void InitDebugCLI()
{
    // Registration
    RegisterCommands(core_commands);

    // Register the byte handler with ConsoleManager
    ConsoleManager::Instance().SetByteHandler(ProcessCLIByte);
}

} // namespace DebugCLI
