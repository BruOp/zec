#pragma once
#include "pch.h"

namespace zec
{
    // Used to setup validation layers, create device
    struct GfxApiContext
    {
        IDXGIFactory4* factory = nullptr;
        IDXGIAdapter1* adapter = nullptr;
    };

    struct Device
    {
        ID3D12Device* api_device = nullptr;
        D3D_FEATURE_LEVEL supported_feature_level;
    };

    void init_api_context(GfxApiContext& api_context);
    void destroy(GfxApiContext& api_context);

    void init_device(Device& device, const GfxApiContext& api_context, D3D_FEATURE_LEVEL min_feature_level);
    void destroy(Device& device);

    struct SwapChain
    {
        IDXGISwapChain1* swapChain = nullptr;
    };

    struct Renderer
    {
        GfxApiContext gfx_api_context;
        Device device;
    };

    void init_renderer(Renderer& renderer);
    void destroy(Renderer& renderer);
}