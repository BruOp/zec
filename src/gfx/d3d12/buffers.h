#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "gfx/resource_array.h"
#include "dx_helpers.h"
//TODO: Remove
#include "descriptor_heap.h"

namespace D3D12MA
{
    class Allocation;
}

namespace zec::gfx::dx12
{
    constexpr size_t CONSTANT_BUFFER_ALIGNMENT = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    constexpr size_t VERTEX_BUFFER_ALIGNMENT = 4;
    constexpr size_t INDEX_BUFFER_ALIGNMENT = 4;

    struct BufferInfo
    {
        void* cpu_address = 0;
        u64 gpu_address = 0;
        // Total represents the total amount of memory allocated for this resource
        // If the buffer is CPU accessible, then it will be RENDER_LATENCY * per_frame_size
        u32 total_size = 0;
        // If the buffer is CPU accessible this will equal total_size / RENDER_LATENCY
        // Else, it's just the total size
        u32 per_frame_size = 0;
        u32 stride = 0;
        u32 cpu_accessible = 0;
    };

    struct Buffer
    {
        ID3D12Resource* resource = nullptr;
        D3D12MA::Allocation* allocation = nullptr;
        BufferInfo info;
        DescriptorRangeHandle srv = {};
        DescriptorRangeHandle uav = {};
    };

    struct BufferList
    {
        BufferList() = default;
        ~BufferList()
        {
            ASSERT(num_buffers == 0);
            ASSERT(resources.size == 0);
            ASSERT(allocations.size == 0);
        }

        BufferHandle push_back(const Buffer& buffer);

        // Getters
        size_t size()
        {
            return num_buffers;
        }

        size_t num_buffers;
        ResourceArray<ID3D12Resource*, BufferHandle> resources = {};
        ResourceArray<D3D12MA::Allocation*, BufferHandle> allocations = {};
        ResourceArray<BufferInfo, BufferHandle> infos = {};
        ResourceArray<DescriptorRangeHandle, BufferHandle> srvs = {};
        ResourceArray<DescriptorRangeHandle, BufferHandle> uavs = {};
    };

    namespace buffer_utils
    {
        void* get_cpu_address(const BufferInfo& buffer_info, const u64 current_frame_idx)
        {
            ASSERT(current_frame_idx < RENDER_LATENCY);
            return static_cast<u8*>(buffer_info.cpu_address) + (buffer_info.per_frame_size * current_frame_idx);
        };

        u64 get_gpu_address(const BufferInfo& buffer_info, const u64 current_frame_idx)
        {
            ASSERT(current_frame_idx < RENDER_LATENCY);
            return buffer_info.gpu_address + (buffer_info.per_frame_size * current_frame_idx);
        };
    }
}
