#pragma once

#include "globals.h"
#include <FS.h>
#if !defined(USE_LITTLEFS)
#define USE_LITTLEFS 1
#endif

#if USE_LITTLEFS
#include <LittleFS.h>
#else
#include <SPIFFS.h>
#endif

class UserFilesystem {
public:
    // Concrete FS accessor
    auto& fs() {
#if USE_LITTLEFS
        return LittleFS;
#else
        return SPIFFS;
#endif
    }

    // Lifecycle
    bool begin(bool formatOnFail = false) {
#if USE_LITTLEFS
        return fs().begin(formatOnFail, "/littlefs", 10, "storage");
#else
        return fs().begin(formatOnFail, "/spiffs", 10, "storage");
#endif
    }
    void end() { fs().end(); }

    // ---- open() ----
    fs::File open(const char* path) { return fs().open(path); }
    fs::File open(const String& path) { return fs().open(path); }

    fs::File open(const char* path, const char* mode) {
        return fs().open(path, mode);
    }

    fs::File open(const String& path, const char* mode) {
        return fs().open(path, mode);
    }

    // ---- file ops ----
    bool remove(const char* path) { return fs().remove(path); }
    bool remove(const String& path) { return fs().remove(path); }

    bool exists(const char* path) { return fs().exists(path); }
    bool exists(const String& path) { return fs().exists(path); }

    bool rename(const char* from, const char* to) {
        return fs().rename(from, to);
    }

    bool rename(const String& from, const String& to) {
        return fs().rename(from, to);
    }

    // Interop escape hatch
    operator fs::FS&() { return fs(); }
};

inline UserFilesystem UserFS;
