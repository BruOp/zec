#include "pch.h"
#include "wrappers.h"
#include "dx_utils.h"
#include "renderer.h"

namespace zec
{
    namespace dx12
    {
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

        void init(DescriptorHeap& descriptor_heap, const DescriptorHeapDesc& desc, ID3D12Device* device)
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
                DXCall(device->CreateDescriptorHeap(
                    &heap_desc, IID_PPV_ARGS(&descriptor_heap.heaps[i])
                ));
                descriptor_heap.cpu_start[i] = descriptor_heap.heaps[i]->GetCPUDescriptorHandleForHeapStart();
                if (descriptor_heap.is_shader_visible) {
                    descriptor_heap.gpu_start[i] = descriptor_heap.heaps[i]->GetGPUDescriptorHandleForHeapStart();
                }
            }

            descriptor_heap.descriptor_size = device->GetDescriptorHandleIncrementSize(descriptor_heap.heap_type);
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