#include "pch.h"
#include "swap_chain.h"
#include "utils/exceptions.h"

namespace zec
{
    void set_formats(SwapChain& swap_chain, const DXGI_FORMAT format)
    {
        swap_chain.format = format;
        if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            swap_chain.non_sRGB_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            swap_chain.non_sRGB_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        }
        else {
            swap_chain.non_sRGB_format = swap_chain.format;
        }
    }

    void after_reset(SwapChain& swap_chain, Device& device)
    {
        // Re-create an RTV for each back buffer
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            // TODO
        }
        swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
    }

    void prepare_full_screen_settings(SwapChain& swap_chain, Device& device)
    {
        ASSERT(swap_chain.output != nullptr);

        DXGI_MODE_DESC desired_mode{};
        desired_mode.Format = swap_chain.non_sRGB_format;
        desired_mode.Width = swap_chain.width;
        desired_mode.Height = swap_chain.height;
        desired_mode.RefreshRate.Numerator = 0;
        desired_mode.RefreshRate.Denominator = 0;
        desired_mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        desired_mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

        DXGI_MODE_DESC closest_match{};
        DXCall(swap_chain.output->FindClosestMatchingMode(&desired_mode, &closest_match, device.device));

        swap_chain.width = closest_match.Width;
        swap_chain.height = closest_match.Height;
        swap_chain.refresh_rate = closest_match.RefreshRate;
    }

    void init_swap_chain(const SwapChainDesc& desc, SwapChain& swap_chain, Device& device)
    {
        ASSERT(swap_chain.swap_chain == nullptr);

        desc.device->adapter->EnumOutputs(0, &swap_chain.output);

        swap_chain.width = desc.width;
        swap_chain.height = desc.height;
        swap_chain.fullscreen = desc.fullscreen;
        swap_chain.vsync = desc.vsync;

        swap_chain.format = desc.format;
        if (desc.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            swap_chain.non_sRGB_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        else if (desc.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            swap_chain.non_sRGB_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        }
        else {
            swap_chain.non_sRGB_format = swap_chain.format;
        }

        DXGI_SWAP_CHAIN_DESC d3d_swap_chain_desc{  };
        d3d_swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
        d3d_swap_chain_desc.BufferDesc.Width = swap_chain.width;
        d3d_swap_chain_desc.BufferDesc.Height = swap_chain.height;
        d3d_swap_chain_desc.BufferDesc.Format = swap_chain.format;
        d3d_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d3d_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        d3d_swap_chain_desc.OutputWindow = desc.output_window;
        d3d_swap_chain_desc.Windowed = TRUE;
        d3d_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        IDXGISwapChain* d3d_swap_chain = nullptr;
        DXCall(desc.device->factory->CreateSwapChain(desc.device->gfx_queue, &d3d_swap_chain_desc, &d3d_swap_chain));
        DXCall(d3d_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain.swap_chain)));
        d3d_swap_chain->Release();

        swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
        swap_chain.waitable_object = swap_chain.swap_chain->GetFrameLatencyWaitableObject();

        after_reset(swap_chain, device);
    }

    void destroy(SwapChain& swap_chain)
    {
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            // Do not need to destroy the resource ourselves, since it's managed by the swap chain (??)
            reset(swap_chain.back_buffers[i]);
        }

        swap_chain.swap_chain->Release();
        swap_chain.output->Release();
    }

    void reset(SwapChain& swap_chain, Device& device)
    {
        ASSERT(swap_chain.swap_chain != nullptr);

        if (swap_chain.output == nullptr) {
            swap_chain.fullscreen = false;
        }

        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            reset(swap_chain.back_buffers[i]);
        }

        set_formats(swap_chain, swap_chain.format);

        if (swap_chain.fullscreen) {
            prepare_full_screen_settings(swap_chain, device);
        }
        else {
            swap_chain.refresh_rate.Numerator = 60;
            swap_chain.refresh_rate.Denominator = 1;
        }

        DXCall(swap_chain.swap_chain->SetFullscreenState(swap_chain.fullscreen, swap_chain.output));
        // TODO Backbuffers
        for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
            DXCall(swap_chain.swap_chain->ResizeBuffers(
                NUM_BACK_BUFFERS,
                swap_chain.width,
                swap_chain.height, swap_chain.non_sRGB_format,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
        }

        if (swap_chain.fullscreen) {
            DXGI_MODE_DESC mode;
            mode.Format = swap_chain.non_sRGB_format;
            mode.Width = swap_chain.width;
            mode.Height = swap_chain.height;
            mode.RefreshRate.Numerator = 0;
            mode.RefreshRate.Denominator = 0;
            mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            DXCall(swap_chain.swap_chain->ResizeTarget(&mode));
        }

        after_reset(swap_chain, device);
    }
}