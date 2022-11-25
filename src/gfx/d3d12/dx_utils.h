#pragma once
#include "utils/exceptions.h"
#include "DXErr.h"

struct ID3D10Blob;
struct IDxcBlobUtf8;
struct IDxcBlobUtf16;

typedef long HRESULT;
typedef ID3D10Blob ID3DBlob;


namespace zec::gfx::dx12
{
    std::string get_string(IDxcBlobUtf8* blob);
    std::wstring get_wstring(IDxcBlobUtf8* blob);
    std::wstring get_wstring(IDxcBlobUtf16* blob);

    void print_blob(ID3DBlob* blob);
    void print_blob(IDxcBlobUtf8* blob);

    std::wstring GetDXErrorString(HRESULT hr);

    std::string GetDXErrorStringUTF8(HRESULT hr);

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
        ASSERT_MSG(SUCCEEDED(hr_), zec::gfx::dx12::GetDXErrorStringUTF8(hr_).c_str());   \
        } while (0);
#endif // DXCall
#else // USE_ASSERTS

// Throws a DXException on failing HRESULT
void DXCall(HRESULT hr);

#endif // USE_ASSERTS
