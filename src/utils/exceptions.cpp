#include "exceptions.h"
#include <Windows.h>

namespace zec
{
    std::wstring GetWin32ErrorString(DWORD errorCode)
    {
        wchar errorString[MAX_PATH];
        ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorString,
            MAX_PATH,
            NULL);

        std::wstring message = L"Win32 Error: ";
        message += errorString;
        return message;
    }

    std::string GetWin32ErrorStringAnsi(DWORD errorCode)
    {
        char errorString[MAX_PATH];
        ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorString,
            MAX_PATH,
            NULL);

        std::string message = "Win32 Error: ";
        message += errorString;
        return message;
    }

    Exception::Exception(const std::string& exceptionMessage)
    {
        wchar buffer[512];
        MultiByteToWideChar(CP_ACP, 0, exceptionMessage.c_str(), -1, buffer, 512);
        message = std::wstring(buffer);
    }

    void Exception::ShowErrorMessage() const throw()
    {
        MessageBox(NULL, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
    }

    Win32Exception::Win32Exception(DWORD code, const wchar* msgPrefix) : errorCode(code)
    {
        wchar errorString[MAX_PATH];
        ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
            0,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorString,
            MAX_PATH,
            NULL);

        message = L"Win32 Error: ";
        if (msgPrefix)
            message += msgPrefix;
        message += errorString;
    }

#if !USE_ASSERTS
    void Win32Call(BOOL retVal)
    {
        if (retVal == 0)
            throw Win32Exception(GetLastError());
    }
#endif // USE_ASSERTS
}