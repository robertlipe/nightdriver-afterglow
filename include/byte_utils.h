#pragma once

//+--------------------------------------------------------------------------
//
// File:        byte_utils.h
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
//
// Description:
//
//    Byte-level helpers for endian-safe reads.
//

#include "globals.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

template <typename T>
inline T ReadFromMemory(const uint8_t* payloadData)
{
    static_assert(std::is_integral_v<T>, "ReadFromMemory requires an integral type");
    T value;
    // std::memcpy is the only strictly conforming way to do unaligned reads in C++.
    // Modern compilers completely optimize this away into a direct (unaligned) load instruction.
    std::memcpy(&value, payloadData, sizeof(T));

    if constexpr (std::endian::native == std::endian::big) {
        return std::byteswap(value);
    } else {
        return value;
    }
}

inline uint64_t ULONGFromMemory(const uint8_t* payloadData) { return ReadFromMemory<uint64_t>(payloadData); }
inline uint32_t DWORDFromMemory(const uint8_t* payloadData) { return ReadFromMemory<uint32_t>(payloadData); }
inline uint16_t WORDFromMemory(const uint8_t* payloadData)  { return ReadFromMemory<uint16_t>(payloadData); }
