#pragma once
#include "pch.h"
#include "core/array.h"
#include "core/ring_buffer.h"
#include "gfx/public_resources.h"
#include "gfx/constants.h"

namespace zec::gfx::dx12
{
    constexpr u32 MAX_DESCRIPTOR_HEAP_SIZE = 1000000;
    constexpr u32 MAX_SAMPLER_HEAP_SIZE = 2048;

    namespace HeapTypes
    {
        constexpr D3D12_DESCRIPTOR_HEAP_TYPE SRV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        constexpr D3D12_DESCRIPTOR_HEAP_TYPE UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        constexpr D3D12_DESCRIPTOR_HEAP_TYPE DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        constexpr D3D12_DESCRIPTOR_HEAP_TYPE RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        constexpr D3D12_DESCRIPTOR_HEAP_TYPE SAMPLER = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        constexpr size_t NUM_HEAPS = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    };

    struct DescriptorRangeHandle
    {
        // Composite of 24-bit offset, 8-bit count
        u32 id = zec::k_invalid_handle;

        inline u32 get_offset() const
        {
            return id >> 8;
        }

        inline u32 get_count() const
        {
            return id & 0x000000FF;
        }

        static DescriptorRangeHandle encode(const u32 offset, const u32 count)
        {
            ASSERT(offset < 0xFFFFFF00 && count < UINT8_MAX);
            return { .id = (offset << 8) | count };
        }
    };

    inline bool is_valid(const DescriptorRangeHandle handle)
    {
        return handle.id != zec::k_invalid_handle;
    }

    struct DescriptorHeapDesc
    {
        static constexpr u64 MAX_SIZE = 16384; // arbitrary
        u64 size = 0;
        D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    };

    struct DescriptorDestructionElement
    {
        DescriptorRangeHandle handle;
        u64 frame_index;
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

        Array<DescriptorRangeHandle> dead_list;

        DescriptorRangeHandle allocate_descriptors(const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[]);

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            return { .ptr = cpu_start.ptr + (u64(descriptor.get_offset()) + local_offset) * descriptor_size };
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            return { .ptr = gpu_start.ptr + (u64(descriptor.get_offset()) + local_offset) * descriptor_size };
        }

        inline void free_descriptors(const DescriptorRangeHandle descriptor_range)
        {
            if (is_valid(descriptor_range)) {
                dead_list.push_back(descriptor_range);
                num_free += descriptor_range.get_count();
            }
        };

        inline void free_descriptors(const Array<DescriptorRangeHandle>& descriptors)
        {
            for (const DescriptorRangeHandle descriptor : descriptors) {
                free_descriptors(descriptor);
            }
        }
    };
}