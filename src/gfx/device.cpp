#include "pch.h"
#include "device.h"
#include "utils/utils.h"

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
    void init(Device& device, D3D_FEATURE_LEVEL min_feature_level)
    {
        ASSERT(device.adapter == nullptr);
        ASSERT(device.factory == nullptr);
        constexpr int ADAPTER_NUMBER = 0;

        DXCall(CreateDXGIFactory1(IID_PPV_ARGS(&device.factory)));

        device.factory->EnumAdapters1(ADAPTER_NUMBER, &device.adapter);

        if (device.adapter == nullptr) {
            throw Exception(L"Unabled to located DXGI 1.4 adapter that supports D3D12.");
        }

        DXGI_ADAPTER_DESC1 desc{  };
        device.adapter->GetDesc1(&desc);
        write_log("Creating DX12 device on adapter '%ls'", desc.Description);

    #ifdef USE_DEBUG_DEVICE
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
        DXCall(D3D12CreateDevice(device.adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device.device)));

        D3D_FEATURE_LEVEL feature_levels_arr[4] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_12_1,
        };
        D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = { };
        feature_levels.NumFeatureLevels = ARRAY_SIZE(feature_levels_arr);
        feature_levels.pFeatureLevelsRequested = feature_levels_arr;
        DXCall(device.device->CheckFeatureSupport(
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
        DXCall(device.device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

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

        // Initialize command allocators and command list
        // We need N allocators for N frames, since we cannot reset them and reclaim the associated memory
        // until the GPU is finished executing against them. So we have to use fences
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            DXCall(device.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&device.cmd_allocators[i])
            ));
        }

        // Command lists, however, can be reset as soon as we've submitted them.
        DXCall(device.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, device.cmd_allocators[0], nullptr, IID_PPV_ARGS(&device.cmd_list)
        ));
        DXCall(device.cmd_list->Close());
        device.cmd_list->SetName(L"Graphics command list");

        // Initialize graphics queue
        D3D12_COMMAND_QUEUE_DESC queue_desc{ };
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        DXCall(device.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&device.gfx_queue)));
        device.gfx_queue->SetName(L"Graphics queue");

        // Set current frame index and reset command allocator and list appropriately
        ID3D12CommandAllocator* current_cmd_allocator = device.cmd_allocators[0];
        DXCall(current_cmd_allocator->Reset());
        DXCall(device.cmd_list->Reset(current_cmd_allocator, nullptr));
    }

    void destroy(Device& device)
    {
        write_log("Destroying Cmd Lists, Allocators, Device, etc");

        device.cmd_list->Release();
        for (u64 i = 0; i < ARRAY_SIZE(device.cmd_allocators); i++) {
            device.cmd_allocators[i]->Release();
        }

        device.device->Release();
        device.adapter->Release();
        device.factory->Release();
    }
}