#include "pch.h"
#include "dx_utils.h"
#include "render_context.h"

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
