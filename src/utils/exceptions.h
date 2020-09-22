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
#include "assert.h"

namespace zec
{

    // Error string functions
    inline std::wstring GetWin32ErrorString(DWORD errorCode)
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

    inline std::string GetWin32ErrorStringAnsi(DWORD errorCode)
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

    // Generic exception, used as base class for other types
    class Exception
    {

    public:
        Exception()
        { }

        // Specify an actual error message
        Exception(const std::wstring& exceptionMessage) : message(exceptionMessage)
        { }

        Exception(const std::string& exceptionMessage)
        {
            wchar buffer[512];
            MultiByteToWideChar(CP_ACP, 0, exceptionMessage.c_str(), -1, buffer, 512);
            message = std::wstring(buffer);
        }

        // Retrieve that error message
        const std::wstring& GetMessage() const throw()
        {
            return message;
        }

        void ShowErrorMessage() const throw()
        {
            MessageBox(NULL, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
        }

    protected:
        std::wstring message; // The error message
    };

    // Exception thrown when a Win32 function fails.
    class Win32Exception : public Exception
    {

    public:
        // Obtains a string for the specified Win32 error code
        Win32Exception(DWORD code, const wchar* msgPrefix = nullptr) : errorCode(code)
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

        // Retrieve the error code
        DWORD GetErrorCode() const throw()
        {
            return errorCode;
        }

    protected:
        DWORD errorCode; // The Win32 error code
    };


    // Error-handling functions

#if USE_ASSERTS

#define Win32Call(x)                                                        \
  do                                                                        \
  {                                                                         \
    BOOL res_ = x;                                                          \
    ASSERT_MSG(res_ != 0, GetWin32ErrorStringAnsi(GetLastError()).c_str()); \
  } while (0)

#else

    // Throws a Win32Exception on failing return value
    inline void Win32Call(BOOL retVal)
    {
        if (retVal == 0)
            throw Win32Exception(GetLastError());
    }

#endif

} // namespace zec
