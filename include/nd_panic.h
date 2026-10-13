#pragma once

//+--------------------------------------------------------------------------
//
// File:        nd_panic.h
//
// NightDriverStrip - (c) 2026 Robert Lipe.  All Rights Reserved.
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
//    Fatal error reporting.
//
//    NightDriverStrip is built without C++ exceptions (-fno-exceptions).  The
//    exception machinery - .eh_frame, .gcc_except_table, the unwind tables,
//    typeinfo, and the __cxa_* / _Unwind_* entry points - cost ~153KB of the
//    app image (firmware.bin) on kitch, and on a chip with no console it buys
//    us nothing: no unwinder is shipped in the image and no caller could catch.
//    Panic() therefore replaces throw for unrecoverable errors.  It reports the
//    message together with the file, line, and function of the caller, records
//    it in the RTC flight recorder (so a post-mortem can still read it after the
//    reboot), and restarts the chip.
//
//---------------------------------------------------------------------------

#include "globals.h"

#include <source_location>

// PanicAt
//
// The real function behind the Panic() macro.  Formats the message, reports it
// tagged with the location of the caller, records it for post-mortem, and then
// restarts the chip.  Always call Panic() rather than this so that the location
// is captured automatically.

[[noreturn]] void PanicAt(const std::source_location& location, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

// Panic
//
// Fatal, unrecoverable error: formats a printf-style message and reboots.
//
// This has to be a macro.  A defaulted std::source_location parameter is what we
// want to write, but a defaulted parameter must come last and the ellipsis of a
// printf-style variadic function must come last as well, so the two cannot be
// combined in a real function without dragging in std::format.  The macro is
// purely an argument-injection device; it adds no code and preserves the
// compiler's format-string checking at the call site.

#define Panic(...) PanicAt(std::source_location::current(), __VA_ARGS__)
