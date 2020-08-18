//=================================================================================================
//
//  Modified version of MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "pch.h"

#include "exceptions.h"
// #include "interface_pointers.h"
// #include "zec_math.h"
#include "assert.h"

namespace zec
{

    // Converts an ANSI string to a std::wstring
    inline std::wstring ansi_to_wstring(const char* ansiString)
    {
        wchar buffer[512];
        Win32Call(MultiByteToWideChar(CP_ACP, 0, ansiString, -1, buffer, 512));
        return std::wstring(buffer);
    }

    inline std::string wstring_to_ansi(const wchar* wideString)
    {
        char buffer[512];
        Win32Call(WideCharToMultiByte(CP_ACP, 0, wideString, -1, buffer, 612, NULL, NULL));
        return std::string(buffer);
    }

    // Splits up a string using a delimiter
    inline void split(const std::wstring& str, std::vector<std::wstring>& parts, const std::wstring& delimiters = L" ")
    {
        // Skip delimiters at beginning
        std::wstring::size_type lastPos = str.find_first_not_of(delimiters, 0);

        // Find first "non-delimiter"
        std::wstring::size_type pos = str.find_first_of(delimiters, lastPos);

        while (std::wstring::npos != pos || std::wstring::npos != lastPos) {
            // Found a token, add it to the vector
            parts.push_back(str.substr(lastPos, pos - lastPos));

            // Skip delimiters.  Note the "not_of"
            lastPos = str.find_first_not_of(delimiters, pos);

            // Find next "non-delimiter"
            pos = str.find_first_of(delimiters, lastPos);
        }
    }

    // Splits up a string using a delimiter
    inline std::vector<std::wstring> split(const std::wstring& str, const std::wstring& delimiters = L" ")
    {
        std::vector<std::wstring> parts;
        split(str, parts, delimiters);
        return parts;
    }

    // Splits up a string using a delimiter
    inline void split(const std::string& str, std::vector<std::string>& parts, const std::string& delimiters = " ")
    {
        // Skip delimiters at beginning
        std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

        // Find first "non-delimiter"
        std::string::size_type pos = str.find_first_of(delimiters, lastPos);

        while (std::string::npos != pos || std::string::npos != lastPos) {
            // Found a token, add it to the vector
            parts.push_back(str.substr(lastPos, pos - lastPos));

            // Skip delimiters.  Note the "not_of"
            lastPos = str.find_first_not_of(delimiters, pos);

            // Find next "non-delimiter"
            pos = str.find_first_of(delimiters, lastPos);
        }
    }

    // Splits up a string using a delimiter
    inline std::vector<std::string> split(const std::string& str, const std::string& delimiters = " ")
    {
        std::vector<std::string> parts;
        split(str, parts, delimiters);
        return parts;
    }

    // Parses a string into a number
    template <typename T>
    inline T parse(const std::wstring& str)
    {
        std::wistringstream stream(str);
        wchar_t c;
        T x;
        if (!(str >> x) || stream.get(c))
            throw Exception(L"Can't parse string \"" + str + L"\"");
        return x;
    }

    // Converts a number to a string
    template <typename T>
    inline std::wstring to_string(const T& val)
    {
        std::wostringstream stream;
        if (!(stream << val))
            throw Exception(L"Error converting value to string");
        return stream.str();
    }

    // Converts a number to an ansi string
    template <typename T>
    inline std::string to_ansi_string(const T& val)
    {
        std::ostringstream stream;
        if (!(stream << val))
            throw Exception(L"Error converting value to string");
        return stream.str();
    }

    void write_log(const wchar* format, ...);
    void write_log(const char* format, ...);

    std::wstring make_string(const wchar* format, ...);
    std::string make_string(const char* format, ...);

    // Outputs a string to the debugger output and stdout
    inline void debug_print(const std::wstring& str)
    {
        std::wstring output = str + L"\n";
        OutputDebugStringW(output.c_str());
        std::printf("%ls", output.c_str());
    }

    // Gets an index from an index buffer
    inline u32 get_index(const void* indices, u32 idx, u32 indexSize)
    {
        if (indexSize == 2)
            return reinterpret_cast<const u16*>(indices)[idx];
        else
            return reinterpret_cast<const u32*>(indices)[idx];
    }

    template <typename T, u64 N>
    u64 array_size(T(&)[N])
    {
        return N;
    }

#define ARRAY_SIZE(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

    inline u64 align_to(u64 num, u64 alignment)
    {
        ASSERT(alignment > 0);
        return ((num + alignment - 1) / alignment) * alignment;
    }

} // namespace zec
