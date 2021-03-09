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
#include <windows.h>

#include "core/zec_types.h"
#include "assert.h"

typedef int BOOL;
typedef unsigned long DWORD;

namespace zec
{
    // Error string functions
    std::wstring GetWin32ErrorString(DWORD errorCode);

    std::string GetWin32ErrorStringAnsi(DWORD errorCode);

    // Generic exception, used as base class for other types
    class Exception
    {

    public:
        Exception()
        { }

        // Specify an actual error message
        Exception(const std::wstring& exceptionMessage) : message(exceptionMessage)
        { }

        Exception(const std::string& exceptionMessage);

        // Retrieve that error message
        const std::wstring& GetMessage() const throw()
        {
            return message;
        }

        void ShowErrorMessage() const throw();

    protected:
        std::wstring message; // The error message
    };

    // Exception thrown when a Win32 function fails.
    class Win32Exception : public Exception
    {

    public:
        // Obtains a string for the specified Win32 error code
        Win32Exception(DWORD code, const wchar* msgPrefix = nullptr);

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

#endif // USE_ASSERTS

} // namespace zec
