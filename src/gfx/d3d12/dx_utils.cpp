#include "pch.h"
#include "dx_utils.h"
#include "render_context.h"
#include <dxcompiler/dxcapi.h>

#if USE_ASSERTS
#else // USE_ASSERTS

// Throws a DXException on failing HRESULT
void DXCall(HRESULT hr)
{
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            throw zec::gfx::dx12::DXException(zec::gfx::dx12::get_render_context().device->GetDeviceRemovedReason());
        }
        else {
            throw zec::gfx::dx12::DXException(hr);
        }
    }
}

#endif // USE_ASSERTS

namespace zec::gfx::dx12
{
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
        const char* blob_string = blob->GetStringPointer();
        size_t len = blob->GetStringLength();
        std::wstring wc(len, L'#');
        mbstowcs(&wc[0], blob_string, len);
        debug_print(wc);
    };
}