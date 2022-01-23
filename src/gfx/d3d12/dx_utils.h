#pragma once
#include "utils/exceptions.h"
#include "DXErr.h"
#include <d3dcommon.h>

struct IDxcBlobUtf8;

namespace zec::gfx::dx12
{
    std::string get_string(IDxcBlobUtf8* blob);
    std::wstring get_wstring(IDxcBlobUtf8* blob);

    void print_blob(ID3DBlob* blob);
    void print_blob(IDxcBlobUtf8* blob);

    inline std::wstring GetDXErrorString(HRESULT hr)
    {
        const u32 errStringSize = 1024;
        wchar errorString[errStringSize];
        DXGetErrorDescriptionW(hr, errorString, errStringSize);

        std::wstring message = L"DirectX Error: ";
        message += errorString;
        return message;
    }

    inline std::string GetDXErrorStringAnsi(HRESULT hr)
    {
        std::wstring errorString = GetDXErrorString(hr);

        std::string message;
        message.reserve(errorString.length());
        for (u64 i = 0; i < errorString.length(); ++i)
            message.append(1, static_cast<char>(errorString[i]));
        return message;
    }

    // Exception thrown when a DirectX Function fails
    class DXException : public Exception
    {

    public:
        // Obtains a string for the specified HRESULT error code
        DXException(HRESULT hresult) : errorCode(hresult)
        {
            message = GetDXErrorString(hresult);
        }

        DXException(HRESULT hresult, LPCWSTR errorMsg) : errorCode(hresult)
        {
            message = L"DirectX Error: ";
            message += errorMsg;
        }

        // Retrieve the error code
        HRESULT GetErrorCode() const throw()
        {
            return errorCode;
        }

    protected:
        HRESULT errorCode; // The DX error code
    };

}

#if USE_ASSERTS
#ifndef DXCall
#define DXCall(x)   \
        do {                                                                        \
        HRESULT hr_ = x;                                                            \
        ASSERT_MSG(SUCCEEDED(hr_), zec::gfx::dx12::GetDXErrorStringAnsi(hr_).c_str());   \
        } while (0);
#endif // DXCall
#else // USE_ASSERTS

// Throws a DXException on failing HRESULT
void DXCall(HRESULT hr);

#endif // USE_ASSERTS
