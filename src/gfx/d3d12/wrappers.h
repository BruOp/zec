#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/constants.h"
#include "./resources.h"

namespace zec
{
    namespace dx12
    {
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

        // ---------- Descriptor Heap Wrapper ----------
        struct PersistentDescriptorAlloc
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handles[RENDER_LATENCY] = { };
            u32 idx = UINT32_MAX;
        };

        struct DescriptorHeapDesc
        {
            u32 num_persistent = 0;
            u32 num_temporary = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            boolean is_shader_visible = false;
        };

        struct DescriptorHeap
        {
            ID3D12DescriptorHeap* heaps[RENDER_LATENCY] = {  };
            u32 num_persistent = 0;
            u32 num_allocated_persistent = 0;
            Array<u32> dead_list = {};

            u32 num_temporary = 0;
            volatile i64 num_allocated_temporary = 0;

            u32 heap_idx = 0;
            u32 num_heaps = 0;
            u32 descriptor_size = 0;
            boolean is_shader_visible = false;
            D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_start[RENDER_LATENCY] = { };
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_start[RENDER_LATENCY] = { };
        };

        void init(DescriptorHeap& descriptor_heap, const DescriptorHeapDesc& desc);
        void destroy(DescriptorHeap& descriptor_heap);

        PersistentDescriptorAlloc allocate_persistent_descriptor(DescriptorHeap& descriptor_heap);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const u32 alloc_indx);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);

        // ---------- Fence ----------
        struct Fence
        {
            ID3D12Fence* d3d_fence = nullptr;
            HANDLE fence_event = INVALID_HANDLE_VALUE;
        };

        void init(Fence& fence, u64 initial_value = 0);
        void destroy(Fence& fence);

        void signal(Fence& fence, ID3D12CommandQueue* queue, u64 fence_value);
        void wait(Fence& fence, u64 fence_value);
        bool is_signaled(Fence& fence, u64 fence_value);
        void clear(Fence& fence, u64 fence_value);

    }
}