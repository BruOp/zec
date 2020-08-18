#include "pch.h"
#include "gfx.h"
#include "utils/utils.h"
#include "utils/exceptions.h"

#if _DEBUG
#define USE_DEBUG_DEVICE 1
#define BREAK_ON_DX_ERROR (USE_DEBUG_DEVICE && 1)
#define USE_GPU_VALIDATION 1
#else
#define USE_DEBUG_DEVICE 0
#define BREAK_ON_DX_ERROR 0
#define USE_GPU_VALIDATION 0
#endif

namespace zec
{
    void init_api_context(GfxApiContext& api_context)
    {
        ASSERT(api_context.adapter == nullptr);
        ASSERT(api_context.factory == nullptr);
        constexpr int ADAPTER_NUMBER = 0;

        DXCall(CreateDXGIFactory1(IID_PPV_ARGS(&api_context.factory)));

        api_context.factory->EnumAdapters1(ADAPTER_NUMBER, &api_context.adapter);

        if (api_context.adapter == nullptr) {
            throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
        }

        DXGI_ADAPTER_DESC1 desc{  };
        api_context.adapter->GetDesc1(&desc);
        write_log("Creating DX12 device on adapter '%ls'", desc.Description);

    #ifdef _DEBUG
        ID3D12Debug* debug_ptr;
        DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_ptr)));
        debug_ptr->EnableDebugLayer();

    #ifdef USE_GPU_VALIDATION
        ID3D12Debug1* debug1;
        debug_ptr->QueryInterface(IID_PPV_ARGS(&debug1));
        debug1->SetEnableGPUBasedValidation(true);
        debug1->Release();
    #endif // USE_GPU_VALIDATION
        debug_ptr->Release();
    #endif // DEBUG
    }

    void zec::destroy(GfxApiContext& api_context)
    {
        api_context.factory->Release();
        api_context.adapter->Release();
    }

    void init_device(Device& device, const GfxApiContext& api_context, D3D_FEATURE_LEVEL min_feature_level)
    {
        DXCall(D3D12CreateDevice(api_context.adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device.api_device)));

        D3D_FEATURE_LEVEL feature_levels_arr[4] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_12_1,
        };
        D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
        feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
        feature_levels.pFeatureLevelsRequested = feature_levels_arr;
        DXCall(device.api_device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS,
            &feature_levels,
            sizeof(feature_levels)
        ));
        device.supported_feature_level = feature_levels.MaxSupportedFeatureLevel;

        if (device.supported_feature_level < min_feature_level) {
            std::wstring majorLevel = to_string<int>(min_feature_level >> 12);
            std::wstring minorLevel = to_string<int>((min_feature_level >> 8) & 0xF);
            throw Exception(L"The device doesn't support the minimum feature level required to run this sample (DX" + majorLevel + L"." + minorLevel + L")");
        }

    #ifdef USE_DEBUG_DEVICE
        ID3D12InfoQueue* infoQueue;
        DXCall(device.api_device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

        D3D12_MESSAGE_ID disabledMessages[] =
        {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,

            // These happen when capturing with VS diagnostics
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        D3D12_INFO_QUEUE_FILTER filter = { };
        filter.DenyList.NumIDs = ARRAY_SIZE(disabledMessages);
        filter.DenyList.pIDList = disabledMessages;
        infoQueue->AddStorageFilterEntries(&filter);
        infoQueue->Release();

    #endif // USE_DEBUG_DEVICE
    }

    void destroy(Device& device)
    {
        device.api_device->Release();
    }

    void init_renderer(Renderer& renderer)
    {
        init_api_context(renderer.gfx_api_context);
        init_device(renderer.device, renderer.gfx_api_context, D3D_FEATURE_LEVEL_12_0);
    }

    void destroy(Renderer& renderer)
    {
        write_log("Destroying Renderer");
        destroy(renderer.device);
        destroy(renderer.gfx_api_context);
    }
}
