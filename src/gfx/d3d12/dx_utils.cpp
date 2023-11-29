#include "dx_utils.h"

#include <d3d12.h>
#include <d3dcommon.h>

#include <dxcompiler/dxcapi.h>
#include "render_context.h"
#include "utils/utils.h"

#if USE_ASSERTS
#else // USE_ASSERTS

// Throws a DXException on failing HRESULT
// TODO: Can I move this rhi_d3d12.cpp?
void DXCall(HRESULT hr)
{
    /*if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            throw zec::rhi::dx12::DXException(zec::rhi::dx12::get_render_context().device->GetDeviceRemovedReason());
        }
        else {
            throw zec::rhi::dx12::DXException(hr);
        }
    }*/
}

#endif // USE_ASSERTS

namespace zec::rhi::dx12
{
    std::string get_string(IDxcBlobUtf8* blob)
    {
        const char* blob_string = blob->GetStringPointer();
        size_t len = blob->GetStringLength();
        return std::string( blob_string, len );
    };

    std::wstring get_wstring(IDxcBlobUtf8* blob)
    {
        const char* blob_string = blob->GetStringPointer();
        size_t len = blob->GetStringLength();
        std::wstring wc(len, L'#');
        mbstowcs(&wc[0], blob_string, len);
        return wc;
    };

    std::wstring get_wstring(IDxcBlobUtf16* blob)
    {
        const wchar_t* blob_string = blob->GetStringPointer();
        size_t len = blob->GetStringLength();
        return std::wstring{ blob_string, len };
    };

    void print_blob(ID3DBlob* blob)
    {
        const char* blob_string = reinterpret_cast<char*>(blob->GetBufferPointer());
        size_t len = std::strlen(blob_string);
        std::wstring wc(len, L'#');
        mbstowcs(&wc[0], blob_string, len);
        debug_print(wc);
    };

    void print_blob(IDxcBlobUtf8* blob)
    {
        std::wstring wc = get_wstring(blob);
        debug_print(wc);
    }

    std::wstring GetDXErrorString(HRESULT hr)
    {
        const u32 errStringSize = 1024;
        wchar errorString[errStringSize];
        DXGetErrorDescriptionW(hr, errorString, errStringSize);

        std::wstring message = L"DirectX Error: ";
        message += errorString;
        return message;
    }

    std::string GetDXErrorStringUTF8(HRESULT hr)
    {
        std::wstring errorString = GetDXErrorString(hr);
        return wstring_to_utf8(errorString);
    }
}