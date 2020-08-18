#pragma once
#include "pch.h"

namespace zec
{
    // TODO: Support triple buffering?
    constexpr u64 render_latency = 2;

    // Used to setup validation layers, create device
    struct GfxApiContext
    {

    };

    struct Device
    {

    };

    void init_api_context(GfxApiContext& api_context);
    void destroy(GfxApiContext& api_context);

    void init_device(Device& device, const GfxApiContext& api_context, );
    void destroy(Device& device);

    struct SwapChain
    {
        IDXGISwapChain1* swapChain = nullptr;
    };

    struct Renderer
    {
        u64 current_cpu_frame = 0;
        u64 current_gpu_frame = 0;
        u64 current_frame_idx = 0;

        IDXGIFactory4* factory = nullptr;
        IDXGIAdapter1* adapter = nullptr;
        ID3D12Device* device = nullptr;
        D3D_FEATURE_LEVEL supported_feature_level;

        ID3D12GraphicsCommandList1* cmd_list = nullptr;
        ID3D12CommandQueue* gfx_queue = nullptr;
        ID3D12CommandAllocator* cmd_allocators[render_latency];
    };

    void init_renderer(Renderer& renderer, D3D_FEATURE_LEVEL min_feature_level);
    void destroy(Renderer& renderer);
}