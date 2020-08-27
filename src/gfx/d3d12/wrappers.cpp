#include "pch.h"
#include "wrappers.h"
#include "utils/utils.h"


namespace zec
{
    namespace dx12
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

        void after_reset(SwapChain& swap_chain)
        {
            // Re-create an RTV for each back buffer
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                // RTV Descriptor heap only has one heap -> only one handle is issued
                ASSERT(rtv_descriptor_heap.num_heaps == 1);
                swap_chain.back_buffers[i].rtv = allocate_persistent_descriptor(rtv_descriptor_heap).handles[0];
                DXCall(swap_chain.swap_chain->GetBuffer(UINT(i), IID_PPV_ARGS(&swap_chain.back_buffers[i].texture.resource)));

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = swap_chain.format;
                rtvDesc.Texture2D.MipSlice = 0;
                rtvDesc.Texture2D.PlaneSlice = 0;
                device->CreateRenderTargetView(swap_chain.back_buffers[i].texture.resource, &rtvDesc, swap_chain.back_buffers[i].rtv);

                swap_chain.back_buffers[i].texture.resource->SetName(make_string(L"Back Buffer %llu", i).c_str());

                // Copy properties from swap chain to backbuffer textures, in case we need em
                swap_chain.back_buffers[i].texture.width = swap_chain.width;
                swap_chain.back_buffers[i].texture.height = swap_chain.height;
                swap_chain.back_buffers[i].texture.depth = 1;
                swap_chain.back_buffers[i].texture.array_size = 1;
                swap_chain.back_buffers[i].texture.format = swap_chain.format;
                swap_chain.back_buffers[i].texture.num_mips = 1;

            }
            swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
        }

        void prepare_full_screen_settings(SwapChain& swap_chain)
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
            DXCall(swap_chain.output->FindClosestMatchingMode(&desired_mode, &closest_match, dx12::device));

            swap_chain.width = closest_match.Width;
            swap_chain.height = closest_match.Height;
            swap_chain.refresh_rate = closest_match.RefreshRate;
        }

        void init(SwapChain& swap_chain, const SwapChainDesc& desc)
        {
            ASSERT(swap_chain.swap_chain == nullptr);

            adapter->EnumOutputs(0, &swap_chain.output);

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
            d3d_swap_chain_desc.SampleDesc.Count = 1;
            d3d_swap_chain_desc.OutputWindow = desc.output_window;
            d3d_swap_chain_desc.Windowed = TRUE;
            d3d_swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            IDXGISwapChain* d3d_swap_chain = nullptr;
            DXCall(factory->CreateSwapChain(gfx_queue, &d3d_swap_chain_desc, &d3d_swap_chain));
            DXCall(d3d_swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain.swap_chain)));
            d3d_swap_chain->Release();

            swap_chain.back_buffer_idx = swap_chain.swap_chain->GetCurrentBackBufferIndex();
            swap_chain.waitable_object = swap_chain.swap_chain->GetFrameLatencyWaitableObject();

            after_reset(swap_chain);
        }

        void destroy(SwapChain& swap_chain)
        {
            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                // Do not need to destroy the resource ourselves, since it's managed by the swap chain (??)
                destroy(swap_chain.back_buffers[i]);
                /*free_persistent_alloc(rtv_descriptor_heap, swap_chain.back_buffers[i].rtv);*/
            }

            swap_chain.swap_chain->Release();
            swap_chain.output->Release();
        }

        void reset(SwapChain& swap_chain)
        {
            ASSERT(swap_chain.swap_chain != nullptr);

            if (swap_chain.output == nullptr) {
                swap_chain.fullscreen = false;
            }

            for (u64 i = 0; i < NUM_BACK_BUFFERS; i++) {
                destroy(swap_chain.back_buffers[i]);
                free_persistent_alloc(rtv_descriptor_heap, swap_chain.back_buffers[i].rtv);
            }

            set_formats(swap_chain, swap_chain.format);

            if (swap_chain.fullscreen) {
                prepare_full_screen_settings(swap_chain);
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

            after_reset(swap_chain);
        }


        inline ID3D12DescriptorHeap* get_active_heap(const DescriptorHeap& heap)
        {
            return heap.heaps[heap.heap_idx];
        }

        u32 get_idx_from_handle(const DescriptorHeap& heap, const D3D12_CPU_DESCRIPTOR_HANDLE handle)
        {
            u64 descriptor_size = u64(heap.descriptor_size);
            ASSERT(heap.heaps[heap.heap_idx] != nullptr);
            // Check that our handle is after the start of the heap's allocated memory range
            ASSERT(handle.ptr >= heap.cpu_start[heap.heap_idx].ptr);
            // Check that our handle is before the end of the heap's allocated memory range
            ASSERT(handle.ptr < heap.cpu_start[heap.heap_idx].ptr + descriptor_size * (u64(heap.num_persistent) + u64(heap.num_temporary)));
            // Check that our handle is aligned with the individual heap allocations
            ASSERT((handle.ptr - heap.cpu_start[heap.heap_idx].ptr) % descriptor_size == 0);
            return static_cast<u32>((handle.ptr - heap.cpu_start[heap.heap_idx].ptr) / descriptor_size);
        }

        u32 get_idx_from_handle(const DescriptorHeap& heap, const D3D12_GPU_DESCRIPTOR_HANDLE handle)
        {
            u64 descriptor_size = u64(heap.descriptor_size);
            ASSERT(heap.heaps[heap.heap_idx] != nullptr);
            // Check that our handle is after the start of the heap's allocated memory range
            ASSERT(handle.ptr >= heap.gpu_start[heap.heap_idx].ptr);
            // Check that our handle is before the end of the heap's allocated memory range
            ASSERT(handle.ptr < heap.gpu_start[heap.heap_idx].ptr + descriptor_size * (u64(heap.num_persistent) + u64(heap.num_temporary)));
            // Check that our handle is aligned with the individual heap allocations
            ASSERT((handle.ptr - heap.gpu_start[heap.heap_idx].ptr) % descriptor_size == 0);
            return static_cast<u32>((handle.ptr - heap.gpu_start[heap.heap_idx].ptr) / descriptor_size);
        }

        void init(DescriptorHeap& descriptor_heap, const DescriptorHeapDesc& desc)
        {
            u32 total_num_descriptors = desc.num_persistent + desc.num_temporary;
            ASSERT(total_num_descriptors > 0);

            descriptor_heap.num_persistent = desc.num_persistent;
            descriptor_heap.num_temporary = desc.num_temporary;
            descriptor_heap.heap_type = desc.heap_type;
            descriptor_heap.is_shader_visible = desc.is_shader_visible;
            // We ignore this settings when using RTV or DSV types
            if (desc.heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || desc.heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
                if (descriptor_heap.is_shader_visible) {
                    write_log("Cannot create shader visible descriptor heap for RTV or DSV heaps");
                }
                descriptor_heap.is_shader_visible = false;
            }

            descriptor_heap.num_heaps = descriptor_heap.is_shader_visible ? RENDER_LATENCY : 1;
            descriptor_heap.dead_list.grow(descriptor_heap.num_persistent);
            for (u32 i = 0; i < descriptor_heap.num_persistent; i++) {
                descriptor_heap.dead_list[i] = i;
            }

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
            heap_desc.NumDescriptors = total_num_descriptors;
            heap_desc.Type = descriptor_heap.heap_type;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (descriptor_heap.is_shader_visible) {
                heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            }

            for (size_t i = 0; i < descriptor_heap.num_heaps; i++) {
                DXCall(dx12::device->CreateDescriptorHeap(
                    &heap_desc, IID_PPV_ARGS(&descriptor_heap.heaps[i])
                ));
                descriptor_heap.cpu_start[i] = descriptor_heap.heaps[i]->GetCPUDescriptorHandleForHeapStart();
                if (descriptor_heap.is_shader_visible) {
                    descriptor_heap.gpu_start[i] = descriptor_heap.heaps[i]->GetGPUDescriptorHandleForHeapStart();
                }
            }

            descriptor_heap.descriptor_size = dx12::device->GetDescriptorHandleIncrementSize(descriptor_heap.heap_type);
        }

        void destroy(DescriptorHeap& descriptor_heap)
        {
            ASSERT(descriptor_heap.num_allocated_persistent == 0);
            for (size_t i = 0; i < descriptor_heap.num_heaps; i++) {
                descriptor_heap.heaps[i]->Release();
            }
        }

        PersistentDescriptorAlloc allocate_persistent_descriptor(DescriptorHeap& heap)
        {
            ASSERT(heap.heaps[0] != nullptr);

            // Aquire lock so we can't accidentally allocate from several threads at once
            ASSERT(heap.num_allocated_persistent < heap.num_persistent);
            u32 idx = heap.dead_list[heap.num_allocated_persistent];
            heap.num_allocated_persistent++;

            // Release Lock

            PersistentDescriptorAlloc alloc{ };
            alloc.idx = idx;
            for (size_t i = 0; i < heap.num_heaps; i++) {
                // Assign and shift the allocation's ptr to the correct place in memory
                alloc.handles[i] = heap.cpu_start[i];
                alloc.handles[i].ptr += u64(idx) * u64(heap.descriptor_size);
            }

            return alloc;
        };

        void free_persistent_alloc(DescriptorHeap& heap, const u32 alloc_idx)
        {
            if (alloc_idx == UINT32_MAX) {
                return;
            }

            ASSERT(alloc_idx < heap.num_persistent);
            ASSERT(heap.heaps[0] != nullptr);

            // TODO: Acquire lock

            ASSERT(heap.num_allocated_persistent > 0);

            heap.dead_list[heap.num_allocated_persistent--] = alloc_idx;

            // TODO: Release lock
        }

        void free_persistent_alloc(DescriptorHeap& heap, const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
        {
            if (handle.ptr != 0) {
                u32 idx = get_idx_from_handle(heap, handle);
                free_persistent_alloc(heap, idx);
            }
        }

        void free_persistent_alloc(DescriptorHeap& heap, const D3D12_GPU_DESCRIPTOR_HANDLE& handle)
        {
            if (handle.ptr != 0) {
                u32 idx = get_idx_from_handle(heap, handle);
                free_persistent_alloc(heap, idx);
            }
        }

        void init(Fence& fence, u64 initial_value)
        {
            DXCall(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3d_fence)));
            fence.fence_event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
            Win32Call(fence.fence_event != 0);
        }

        void destroy(Fence& fence)
        {
            queue_resource_destruction(fence.d3d_fence);
            fence.d3d_fence = nullptr;
        }

        void signal(Fence& fence, ID3D12CommandQueue* queue, u64 value)
        {
            ASSERT(fence.d3d_fence != nullptr);
            DXCall(queue->Signal(fence.d3d_fence, value));
        }

        void wait(Fence& fence, u64 fence_value)
        {
            ASSERT(fence.d3d_fence != nullptr);
            if (fence.d3d_fence->GetCompletedValue() < fence_value) {
                fence.d3d_fence->SetEventOnCompletion(fence_value, fence.fence_event);
                WaitForSingleObjectEx(fence.fence_event, 5000, FALSE);
            }
        }

        bool is_signaled(Fence& fence, u64 fence_value)
        {
            ASSERT(fence.d3d_fence != nullptr);
            return fence.d3d_fence->GetCompletedValue() >= fence_value;
        }

        void clear(Fence& fence, u64 fence_value)
        {
            ASSERT(fence.d3d_fence != nullptr);
            fence.d3d_fence->Signal(fence_value);
        }


    }
}