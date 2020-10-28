#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "gfx/constants.h"

namespace zec
{
    namespace dx12
    {
        constexpr u32 MAX_DESCRIPTOR_HEAP_SIZE = 1000000;
        constexpr u32 MAX_SAMPLER_HEAP_SIZE = 2048;

        struct DescriptorRangeHandle
        {
            // Composite of 24-bit offset, 8-bit count
            u32 id = zec::k_invalid_handle;
        };

        struct DescriptorHeapDesc
        {
            static constexpr u64 MAX_SIZE = 16384; // arbitrary
            u64 size = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        };

        struct DescriptorDestructionElement
        {
            DescriptorRangeHandle handle;
            u32 frame_index;
        };

        // NOTE: Descriptors are immutable, they can only be allocated and destroyed.
        // So don't try to write to the CPU descriptor handle of an existing descriptor
        struct DescriptorHeap
        {
            ID3D12DescriptorHeap* heap;
            size_t num_allocated = 0;
            size_t num_free = 0;
            size_t capacity = 0;
            size_t descriptor_size = 0;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_start = {};
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_start = {};
            bool is_shader_visible = false;

            Array<DescriptorDestructionElement> destruction_queue;
            Array<DescriptorRangeHandle> dead_list;
        };

        namespace DescriptorUtils
        {

            inline u32 get_offset(const DescriptorRangeHandle handle)
            {
                return handle.id >> 8;
            }

            inline u32 get_count(const DescriptorRangeHandle handle)
            {
                return handle.id & 0x000000FF;
            }

            inline DescriptorRangeHandle encode(const u32 offset, const u32 count)
            {
                ASSERT(offset < 0xFFFFFF00 && count < UINT8_MAX);
                return { .id = (offset << 8) | count };
            }

            inline bool is_valid(const DescriptorRangeHandle handle)
            {
                return handle.id != zec::k_invalid_handle;
            }

            void init(ID3D12Device* device, DescriptorHeap& heap, const DescriptorHeapDesc& heap_desc);
            void destroy(DescriptorHeap& heap);

            // This takes a heap and a null CPU_DESCRIPTOR_HANDLE and returns an index, but also sets the in_handles ptr value
            // to the appropriate one so it can be used to create views
            DescriptorRangeHandle allocate_descriptors(DescriptorHeap& heap, const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[]);
            DescriptorRangeHandle allocate_descriptors(const D3D12_DESCRIPTOR_HEAP_TYPE type, const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[]); // Use globals

            void free_descriptors(DescriptorHeap& heap, const DescriptorRangeHandle descriptor_handle, u64 current_frame_idx);
            void free_descriptors(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_range); // Use globals

            void process_destruction_queue(DescriptorHeap& heap, u64 current_frame_idx);

            void init_descriptor_heaps();
            void destroy_descriptor_heaps();

            // Helpers
            D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_handle, size_t local_offset = 0);
            D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_range, size_t local_offset = 0);
        }
    }
}