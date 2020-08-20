#pragma once
#include "pch.h"
#include "constants.h"

namespace zec
{
    struct Device
    {
        IDXGIFactory4* factory = nullptr;
        IDXGIAdapter1* adapter = nullptr;
        ID3D12Device* device = nullptr;
        D3D_FEATURE_LEVEL supported_feature_level;

        ID3D12GraphicsCommandList1* cmd_list = nullptr;
        ID3D12CommandQueue* gfx_queue = nullptr;
        ID3D12CommandAllocator* cmd_allocators[RENDER_LATENCY];
    };

    void init(Device& device, D3D_FEATURE_LEVEL min_feature_level);
    void destroy(Device& device);
}