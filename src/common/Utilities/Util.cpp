/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "Util.h"
#include "Containers.h"
#include <boost/core/demangle.hpp>
#include <algorithm>
#include <utf8.h>

bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize)
{
    try
    {
        Warhead::CheckedBufferOutputIterator<wchar_t> out(wstr, wsize);
        out = utf8::utf8to16(utf8str, utf8str + csize, out);
        wsize -= out.remaining(); // remaining unused space
        wstr[wsize] = L'\0';
    }
    catch (std::exception const&)
    {
        // Replace the converted string with an error message if there is enough space
        // Otherwise just return an empty string
        const wchar_t* errorMessage = L"An error occurred converting string from UTF-8 to WStr";
        std::size_t errorMessageLength = std::char_traits<wchar_t>::length(errorMessage);
        if (wsize >= errorMessageLength)
        {
            std::wcscpy(wstr, errorMessage);
            wsize = std::char_traits<wchar_t>::length(wstr);
        }
        else if (wsize > 0)
        {
            wstr[0] = L'\0';
            wsize = 0;
        }
        else
        {
            wsize = 0;
        }

        return false;
    }

    return true;
}

bool Utf8toWStr(std::string_view utf8str, std::wstring& wstr)
{
    wstr.clear();
    try
    {
        utf8::utf8to16(utf8str.begin(), utf8str.end(), std::back_inserter(wstr));
    }
    catch (std::exception const&)
    {
        wstr.clear();
        return false;
    }

    return true;
}

bool StringEqualI(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); });
}

std::string GetTypeName(std::type_info const& info)
{
    return boost::core::demangle(info.name());
}

std::string Warhead::Impl::ByteArrayToHexStr(uint8 const* bytes, size_t arrayLen, bool reverse /* = false */)
{
    int32 init = 0;
    int32 end = arrayLen;
    int8 op = 1;

    if (reverse)
    {
        init = arrayLen - 1;
        end = -1;
        op = -1;
    }

    std::ostringstream ss;
    for (int32 i = init; i != end; i += op)
    {
        char buffer[4];
        sprintf(buffer, "%02X", bytes[i]);
        ss << buffer;
    }

    return ss.str();
}

void Warhead::Impl::HexStrToByteArray(std::string_view str, uint8* out, size_t outlen, bool reverse /*= false*/)
{
//    ASSERT(str.size() == (2 * outlen));

    int32 init = 0;
    int32 end = int32(str.length());
    int8 op = 1;

    if (reverse)
    {
        init = int32(str.length() - 2);
        end = -2;
        op = -1;
    }

    uint32 j = 0;
    for (int32 i = init; i != end; i += 2 * op)
    {
        char buffer[3] = { str[i], str[i + 1], '\0' };
        out[j++] = uint8(strtoul(buffer, nullptr, 16));
    }
}

bool WStrToUtf8(wchar_t const* wstr, size_t size, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(size * 4);                            // allocate for most long case

        if (size)
        {
            char* oend = utf8::utf16to8(wstr, wstr + size, &utf8str2[0]);
            utf8str2.resize(oend - (&utf8str2[0]));               // remove unused tail
        }

        utf8str = utf8str2;
    }
    catch (std::exception const&)
    {
        utf8str.clear();
        return false;
    }

    return true;
}

bool WStrToUtf8(std::wstring_view wstr, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(wstr.size() * 4);                     // allocate for most long case

        if (!wstr.empty())
        {
            char* oend = utf8::utf16to8(wstr.begin(), wstr.end(), &utf8str2[0]);
            utf8str2.resize(oend - (&utf8str2[0]));                // remove unused tail
        }

        utf8str = utf8str2;
    }
    catch (std::exception const&)
    {
        utf8str.clear();
        return false;
    }

    return true;
}

void wstrToUpper(std::wstring& str) { std::transform(std::begin(str), std::end(str), std::begin(str), wcharToUpper); }
void wstrToLower(std::wstring& str) { std::transform(std::begin(str), std::end(str), std::begin(str), wcharToLower); }
