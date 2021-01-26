#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
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
        u64 size = 0;
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
            ASSERT(resources.size == 0);
            ASSERT(allocations.size == 0);
        }

        Array<ID3D12Resource*> resources = {};
        Array<D3D12MA::Allocation*> allocations = {};
        Array<BufferInfo> infos = {};
        Array<DescriptorRangeHandle> srvs = {};
        Array<DescriptorRangeHandle> uavs = {};
    };

    namespace buffer_utils
    {
        BufferHandle push_back(BufferList& buffer_list, const Buffer& buffer);
        void update(BufferList& buffer_list, const BufferHandle handle, const void* data, const u64 data_size, const u64 frame_idx);

        inline ID3D12Resource* get_resource(const BufferList& buffer_list, const BufferHandle handle)
        {
            return buffer_list.resources[handle.idx];
        }

        inline void* get_cpu_address(const BufferList& buffer_list, const BufferHandle handle, const u64 current_frame_idx)
        {
            const BufferInfo& buffer_info = buffer_list.infos[handle.idx];
            if (buffer_info.cpu_accessible) {
                return reinterpret_cast<u8*>(buffer_info.cpu_address) + (buffer_info.size * current_frame_idx);
            }
            else {
                return buffer_info.cpu_address;
            }
        }

        inline u64 get_gpu_address(const BufferList& buffer_list, const BufferHandle handle, const u64 current_frame_idx)
        {
            const BufferInfo& buffer_info = buffer_list.infos[handle.idx];
            if (buffer_info.cpu_accessible) {
                return buffer_info.gpu_address + (buffer_info.size * current_frame_idx);
            }
            else {
                return buffer_info.gpu_address;
            }
        }

        inline u32 get_srv_index(const BufferList& buffer_list, const BufferHandle handle)
        {
            return descriptor_utils::get_offset(buffer_list.srvs[handle.idx]);
        }

        inline u32 get_uav_index(const BufferList& buffer_list, const BufferHandle handle)
        {
            return descriptor_utils::get_offset(buffer_list.uavs[handle.idx]);
        }

        inline const BufferInfo& get_buffer_info(const BufferList& buffer_list, BufferHandle buffer_handle)
        {
            return buffer_list.infos[buffer_handle.idx];
        }
    }
}
