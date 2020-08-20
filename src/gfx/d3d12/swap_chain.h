#pragma once
#include "pch.h"
#include "gfx/constants.h"
#include "resources.h"

namespace zec
{
    namespace dx12
    {
        struct SwapChainDesc
        {
            HWND output_window;
            u32 width = 1280;
            u32 height = 720;
            bool fullscreen = false;
            bool vsync = true;
            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        };

        struct SwapChain
        {
            IDXGISwapChain4* swap_chain = nullptr;
            u32 back_buffer_idx = 0;
            RenderTexture back_buffers[NUM_BACK_BUFFERS] = { };

            HANDLE waitable_object = INVALID_HANDLE_VALUE;

            // TODO: What is this
            IDXGIOutput* output = nullptr;

            DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
            DXGI_FORMAT non_sRGB_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            u32 width = 1280;
            u32 height = 720;
            bool fullscreen = false;
            bool vsync = true;
            DXGI_RATIONAL refresh_rate = {
                60, // Numerator
                1   // Denominator
            };
            u32 num_vsync_intervals = 1;
        };

        void init(SwapChain& swap_chain, const SwapChainDesc& desc);
        void destroy(SwapChain& swap_chain);

        void reset(SwapChain& swap_chain);
    }
}
