#include "pch.h"
#include "descriptor_heap.h"
#include "utils/utils.h"
#include "globals.h"

namespace zec
{
    namespace dx12
    {

        constexpr u32 NUM_HEAPS = RENDER_LATENCY;

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

            descriptor_heap.num_heaps = descriptor_heap.is_shader_visible ? 2 : 1;
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
            ASSERT(descriptor_heap.num_allocated_persistant == 0);
            for (size_t i = 0; i < NUM_HEAPS; i++) {
                descriptor_heap.heaps[i]->Release();
            }
        }

        PersistentDescriptorAlloc allocate_persistent_descriptor(DescriptorHeap& heap)
        {
            ASSERT(heap.heaps[0] != nullptr);

            // Aquire lock so we can't accidentally allocate from several threads at once
            ASSERT(heap.num_allocated_persistant < heap.num_persistent);
            u32 idx = heap.dead_list[heap.num_allocated_persistant];
            heap.num_allocated_persistant++;

            // Release Lock

            PersistentDescriptorAlloc alloc{ };
            alloc.idx = idx;
            for (size_t i = 0; i < NUM_HEAPS; i++) {
                // Assign and shift the allocation's ptr to the correct place in memory
                alloc.hadles[i] = heap.cpu_start[i];
                alloc.hadles[i].ptr += u64(idx) * u64(heap.descriptor_size);
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

            ASSERT(heap.num_allocated_persistant > 0);

            heap.dead_list[heap.num_allocated_persistant--] = alloc_idx;

            // TODO: Release lock
        }
    }
}