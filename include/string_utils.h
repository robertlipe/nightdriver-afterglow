#pragma once

//+--------------------------------------------------------------------------
//
// File:        string_utils.h
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
//    String-related helpers, including case-insensitive comparisons.
//

#include "globals.h"

#include <cctype>
#include <cstring>
#include <string_view>
#include <strings.h>

// Fast, constexpr ASCII lowercase conversion for compile-time token evaluation.
constexpr char ToLowerAscii(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

// Case-insensitive string equality comparison for two string_views.
// Evaluates at compile time in constexpr contexts, or utilizes strncasecmp at runtime.
constexpr bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;

    if consteval
    {
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
                return false;
        }
        return true;
    }
    else
    {
        return strncasecmp(a.data(), b.data(), a.size()) == 0;
    }
}

// Case-insensitive string equality with null-safe const char*.
inline bool EqualsIgnoreCase(std::string_view a, const char* b) noexcept
{
    if (!b)
        return false;
    return EqualsIgnoreCase(a, std::string_view(b));
}

inline bool EqualsIgnoreCase(const char* a, std::string_view b) noexcept
{
    if (!a)
        return false;
    return EqualsIgnoreCase(std::string_view(a), b);
}

// Case-insensitive prefix match (starts_with).
constexpr bool StartsWithIgnoreCase(std::string_view haystack, std::string_view needle) noexcept
{
    if (needle.size() > haystack.size())
        return false;

    if consteval
    {
        for (size_t i = 0; i < needle.size(); ++i)
        {
            if (ToLowerAscii(haystack[i]) != ToLowerAscii(needle[i]))
                return false;
        }
        return true;
    }
    else
    {
        return strncasecmp(haystack.data(), needle.data(), needle.size()) == 0;
    }
}

inline bool StartsWithIgnoreCase(std::string_view haystack, const char* needle) noexcept
{
    if (!needle)
        return false;
    return StartsWithIgnoreCase(haystack, std::string_view(needle));
}

// Case-insensitive substring search (contains).
constexpr bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle) noexcept
{
    if (needle.empty())
        return true;
    if (needle.size() > haystack.size())
        return false;

    for (size_t i = 0; i <= haystack.size() - needle.size(); ++i)
    {
        if (EqualsIgnoreCase(haystack.substr(i, needle.size()), needle))
            return true;
    }
    return false;
}

inline bool ContainsIgnoreCase(std::string_view haystack, const char* needle) noexcept
{
    if (!needle)
        return false;
    return ContainsIgnoreCase(haystack, std::string_view(needle));
}

// Backward-compatible aliases for existing callers.
inline bool StringCompareInsensitive(std::string_view a, const char* b) noexcept
{
    return EqualsIgnoreCase(a, b);
}

inline bool StringCompareInsensitive(std::string_view a, std::string_view b) noexcept
{
    return EqualsIgnoreCase(a, b);
}

inline bool StringStartsWithInsensitive(std::string_view haystack, std::string_view needle) noexcept
{
    return StartsWithIgnoreCase(haystack, needle);
}

inline bool StringStartsWithInsensitive(std::string_view haystack, const char* needle) noexcept
{
    return StartsWithIgnoreCase(haystack, needle);
}

inline bool ContainsInsensitive(std::string_view haystack, std::string_view needle) noexcept
{
    return ContainsIgnoreCase(haystack, needle);
}
