#include "utils.h"

namespace zec
{
    std::wstring ansi_to_wstring(const char* ansiString)
    {
        wchar buffer[512];
        Win32Call(MultiByteToWideChar(CP_ACP, 0, ansiString, -1, buffer, 512));
        return std::wstring(buffer);
    }

    std::string wstring_to_ansi(const wchar* wideString)
    {
        char buffer[512];
        Win32Call(WideCharToMultiByte(CP_ACP, 0, wideString, -1, buffer, 512, NULL, NULL));
        return std::string(buffer);
    }

    void split(const std::wstring& str, std::vector<std::wstring>& parts, const std::wstring& delimiters)
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

    std::vector<std::wstring> split(const std::wstring& str, const std::wstring& delimiters)
    {
        std::vector<std::wstring> parts;
        split(str, parts, delimiters);
        return parts;
    }

    void split(const std::string& str, std::vector<std::string>& parts, const std::string& delimiters)
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

    std::vector<std::string> split(const std::string& str, const std::string& delimiters)
    {
        std::vector<std::string> parts;
        split(str, parts, delimiters);
        return parts;
    }
    
    void write_log(const wchar* format, ...)
    {
        wchar buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, array_size(buffer), format, args);

        OutputDebugStringW(buffer);
        OutputDebugStringW(L"\n");
    }

    void write_log(const char* format, ...)
    {
        char buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, array_size(buffer), format, args);

        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");

    }

    std::wstring make_string(const wchar* format, ...)
    {
        wchar buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vswprintf_s(buffer, array_size(buffer), format, args);
        return std::wstring(buffer);
    }

    std::string make_string(const char* format, ...)
    {
        char buffer[1024] = { 0 };
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, array_size(buffer), format, args);
        return std::string(buffer);
    }

    void debug_print(const std::wstring& str)
    {
        std::wstring output = str + L"\n";
        OutputDebugStringW(output.c_str());
        std::printf("%ls", output.c_str());
    }
}