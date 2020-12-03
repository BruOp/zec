#include "pch.h"
#include "dx_utils.h"
#include "globals.h"

#if USE_ASSERTS
#else // USE_ASSERTS

// Throws a DXException on failing HRESULT
void DXCall(HRESULT hr)
{
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED) {
            throw zec::gfx::dx12::DXException(zec::gfx::dx12::g_device->GetDeviceRemovedReason());
        }
        else {
            throw zec::gfx::dx12::DXException(hr);
        }
    }
}

#endif // USE_ASSERTS
