#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/constants.h"
#include "gfx/public_resources.h"

namespace zec::gfx::dx12
{
    constexpr u32 INVALID_DESCRIPTOR_INDEX = UINT32_MAX;
    constexpr D3D12_CPU_DESCRIPTOR_HANDLE INVALID_CPU_HANDLE = { 0 };
    constexpr D3D12_GPU_DESCRIPTOR_HANDLE INVALID_GPU_HANDLE = { 0 };

    inline bool operator==(D3D12_CPU_DESCRIPTOR_HANDLE a, D3D12_CPU_DESCRIPTOR_HANDLE b)
    {
        return a.ptr == b.ptr;
    }

    inline bool operator==(D3D12_GPU_DESCRIPTOR_HANDLE a, D3D12_GPU_DESCRIPTOR_HANDLE b)
    {
        return a.ptr == b.ptr;
    }

    // ---------- Swap Chain Wrapper ----------
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
        TextureHandle back_buffers[NUM_BACK_BUFFERS] = {};

        HANDLE waitable_object = INVALID_HANDLE_VALUE;

        // Used for enumerating output modes?
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

    inline const TextureHandle get_current_back_buffer_handle(const SwapChain& swap_chain, const u64 current_frame_idx)
    {
        return swap_chain.back_buffers[current_frame_idx];
    }


    // ---------- Fence ----------
    struct Fence
    {
        ID3D12Fence* d3d_fence = nullptr;
        HANDLE fence_event = INVALID_HANDLE_VALUE;
    };

    inline u64 get_completed_value(Fence& fence)
    {
        return fence.d3d_fence->GetCompletedValue();
    }
    void signal(Fence& fence, ID3D12CommandQueue* queue, u64 fence_value);
    void wait(Fence& fence, u64 fence_value);
    bool is_signaled(Fence& fence, u64 fence_value);
    void clear(Fence& fence, u64 fence_value);

}
