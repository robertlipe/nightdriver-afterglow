#pragma once

#include "globals.h"

#include <esp_memory_utils.h>
#include <FS.h>
#include <source_location>

#if !defined(USE_LITTLEFS)
#define USE_LITTLEFS 1
#endif

#if USE_LITTLEFS
#include <LittleFS.h>
#else
#include <SPIFFS.h>
#endif

#include "nd_panic.h"

inline bool IsPointerInPSRAM(const void* p)
{
#if SOC_SPIRAM_SUPPORTED || CONFIG_SPIRAM || defined(BOARD_HAS_PSRAM)
    return esp_ptr_external_ram(p);
#else
    return false;
#endif
}

inline bool IsStackInPSRAM()
{
    const void* sp = __builtin_frame_address(0);
#if CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
    return esp_stack_ptr_in_extram((uint32_t)sp);
#else
    return IsPointerInPSRAM(sp);
#endif
}

inline void AssertValidFlashContext(const std::source_location& loc = std::source_location::current())
{
    if (IsStackInPSRAM())
    {
        PanicAt(loc, "Flash/UserFS operation called with stack in PSRAM (SP=%p)", __builtin_frame_address(0));
    }
}

inline void AssertValidFlashBuffer(const void* buf, const std::source_location& loc = std::source_location::current())
{
    if (buf && IsPointerInPSRAM(buf))
    {
        PanicAt(loc, "Flash/UserFS buffer resides in PSRAM (buf=%p)", buf);
    }
}

class UserFile : public fs::File {
public:
    using fs::File::File;
    UserFile() : fs::File() {}
    UserFile(fs::File f) : fs::File(std::move(f)) {}

    // ---- write operations ----
    size_t write(uint8_t c) override {
        AssertValidFlashContext();
        return fs::File::write(c);
    }

    size_t write(const uint8_t *buf, size_t size) override {
        AssertValidFlashContext();
        AssertValidFlashBuffer(buf);
        return fs::File::write(buf, size);
    }

    // ---- read operations ----
    int read() override {
        AssertValidFlashContext();
        return fs::File::read();
    }

    size_t read(uint8_t *buf, size_t size) {
        AssertValidFlashContext();
        AssertValidFlashBuffer(buf);
        return fs::File::read(buf, size);
    }

    size_t readBytes(char *buffer, size_t length) {
        AssertValidFlashContext();
        AssertValidFlashBuffer(buffer);
        return fs::File::readBytes(buffer, length);
    }

    String readString() {
        AssertValidFlashContext();
        return fs::File::readString();
    }

    // ---- lifecycle / control ----
    void flush() override {
        AssertValidFlashContext();
        fs::File::flush();
    }

    void close() {
        AssertValidFlashContext();
        fs::File::close();
    }

    bool seek(uint32_t pos, SeekMode mode) {
        AssertValidFlashContext();
        return fs::File::seek(pos, mode);
    }

    bool seek(uint32_t pos) {
        AssertValidFlashContext();
        return fs::File::seek(pos);
    }
};

class UserFilesystem {
public:
    using File = UserFile;

    // Concrete FS accessor
    auto& fs() {
#if USE_LITTLEFS
        return LittleFS;
#else
        return SPIFFS;
#endif
    }

    // Lifecycle
    bool begin(bool formatOnFail = false, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
#if USE_LITTLEFS
        return fs().begin(formatOnFail, "/littlefs", 10, "storage");
#else
        return fs().begin(formatOnFail, "/spiffs", 10, "storage");
#endif
    }
    void end(const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        fs().end();
    }

    // ---- open() ----
    UserFile open(const char* path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return UserFile(fs().open(path));
    }
    UserFile open(const String& path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return UserFile(fs().open(path));
    }

    UserFile open(const char* path, const char* mode, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return UserFile(fs().open(path, mode));
    }

    UserFile open(const String& path, const char* mode, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return UserFile(fs().open(path, mode));
    }

    // ---- file ops ----
    bool remove(const char* path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().remove(path);
    }
    bool remove(const String& path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().remove(path);
    }

    bool exists(const char* path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().exists(path);
    }
    bool exists(const String& path, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().exists(path);
    }

    bool rename(const char* from, const char* to, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().rename(from, to);
    }

    bool rename(const String& from, const String& to, const std::source_location& loc = std::source_location::current()) {
        AssertValidFlashContext(loc);
        return fs().rename(from, to);
    }

    // Interop escape hatch
    operator fs::FS&() {
        AssertValidFlashContext();
        return fs();
    }
};

inline UserFilesystem UserFS;
