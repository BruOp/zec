#include "pch.h"
#include "utils/utils.h"

namespace zec
{
    void write_log(const wchar* format, ...)
    {
        wchar buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, ARRAY_SIZE(buffer), format, args);

        OutputDebugStringW(buffer);
        OutputDebugStringW(L"\n");
    }

    void write_log(const char* format, ...)
    {
        char buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, ARRAY_SIZE(buffer), format, args);

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");

    }

    std::wstring make_string(const wchar* format, ...)
    {
        wchar buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, ARRAY_SIZE(buffer), format, args);
        return std::wstring(buffer);
    }

    std::string make_string(const char* format, ...)
    {
        char buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, ARRAY_SIZE(buffer), format, args);
        return std::string(buffer);
    }
}