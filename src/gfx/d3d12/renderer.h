#pragma once
#include "pch.h"

namespace zec
{
    namespace dx12
    {
        struct DeviceContext
        {
            IDXGIFactory4* factory;
            IDXGIAdapter1* adapter;
            ID3D12Device* device;
            D3D_FEATURE_LEVEL supported_feature_level;
        };

        class Renderer
        {
            u64 current_frame_idx;
            u64 current_cpu_frame;  // Total number of GPU frames completed (completed means that the GPU signals the fence)
            u64 current_gpu_frame;  // Total number of GPU frames completed (completed means that the GPU signals the fence)

            DeviceContext device_context;

            ID3D12GraphicsCommandList1* cmd_list;
            ID3D12CommandQueue* gfx_queue;

            SwapChain swap_chain;

            DescriptorHeap rtv_descriptor_heap;

            Array<IUnknown*> deferred_releases;
        };
    }
}