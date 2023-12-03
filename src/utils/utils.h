//=================================================================================================
//
//  Modified version of MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once
#include <string>
#include <vector>
#include <sstream>

#include "exceptions.h"
#include "assert.h"

namespace zec
{
    bool is_success(const ZecResult res);

    // Converts an ANSI string to a std::wstring
    std::wstring ansi_to_wstring(const char* ansiString);

    std::string wstring_to_ansi(const wchar* wideString);
    std::string wstring_to_utf8(const std::wstring& wideString);

    // Splits up a string using a delimiter
    void split(const std::wstring& str, std::vector<std::wstring>& parts, const std::wstring& delimiters = L" ");

    // Splits up a string using a delimiter
    std::vector<std::wstring> split(const std::wstring& str, const std::wstring& delimiters = L" ");

    // Splits up a string using a delimiter
    void split(const std::string& str, std::vector<std::string>& parts, const std::string& delimiters = " ");

    // Splits up a string using a delimiter
    std::vector<std::string> split(const std::string& str, const std::string& delimiters = " ");

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
    std::wstring to_string(const T& val)
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
    void debug_print(const std::wstring& str);

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

    /// <summary>
    /// Computes the lowest set bit
    /// If the mask is zero, is_not_zero is set to false
    /// </summary>
    /// <param name="mask">The mask we want to compute the lowest set bit for.</param>
    /// <param name="set_bit_index">
    /// Output reference. If the mask is zero, it is set to UINT32_MAX.
    /// Otherwise it is set to the lowest set bit in mask
    /// </param>
    /// <param name="is_not_zero">Output reference. If the mask is zero, set to false</param>
    /// <returns>A copy of the mask, but with the previously lowest set bit now zeroed</returns>
    u32 get_lowest_set_bit(u32 mask, u32& set_bit_index, bool& is_not_zero);

#define VAR_NAME_HELPER(name) #name
#define VAR_NAME(x) VAR_NAME_HELPER(x)

#define ARRAY_SIZE(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

    inline u32 align_to(u32 num, u32 alignment)
    {
        ASSERT(alignment > 0);
        return ((num + alignment - 1) / alignment) * alignment;
    }
} // namespace zec
