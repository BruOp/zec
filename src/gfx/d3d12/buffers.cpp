#include "pch.h"
#include "gfx/gfx.h"
#include "buffers.h"
#include "dx_utils.h"

namespace zec::gfx::dx12
{
    void BufferList::destroy(void(*resource_destruction_callback)(ID3D12Resource*, D3D12MA::Allocation*), void(*descriptor_destruction_callback)(D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorRangeHandle))
    {
        for (size_t i = 0; i < resources.size; i++) {
            resource_destruction_callback(resources[i], allocations[i]);
            resources[i] = nullptr;
            allocations[i] = nullptr;
        }
        resources.empty();
        allocations.empty();

        ASSERT(srvs.size == uavs.size);
        for (size_t i = 0; i < srvs.size; i++) {
            if (is_valid(srvs[i])) {
                descriptor_destruction_callback(HeapTypes::SRV, srvs[i]);
                srvs[i] = INVALID_HANDLE;
            }

            if (is_valid(uavs[i])) {
                descriptor_destruction_callback(HeapTypes::UAV, uavs[i]);
                uavs[i] = INVALID_HANDLE;
            }
        }
        srvs.empty();
        uavs.empty();

        num_buffers = 0;
    }

    BufferHandle zec::gfx::dx12::BufferList::store_buffer(const Buffer& buffer)
    {
        ASSERT(resources.size < UINT32_MAX);
        u32 idx = UINT32_MAX;
        if (free_handles.size > 0) {
            idx = free_handles.pop_back();
            resources[idx] = buffer.resource;
            allocations[idx] = buffer.allocation;
            infos[idx] = buffer.info;
            srvs[idx] = buffer.srv;
            uavs[idx] = buffer.uav;
        }
        else {
            idx = u32(resources.push_back(buffer.resource));
            allocations.push_back(buffer.allocation);
            infos.push_back(buffer.info);
            srvs.push_back(buffer.srv);
            uavs.push_back(buffer.uav);
        }

        ++num_buffers;

        return { idx };
    }

    void BufferList::destroy_buffer(BufferHandle buffer_handle)
    {
        ASSERT(buffer_handle < UINT32_MAX);

        free_handles.push_back(buffer_handle.idx);

        resources[buffer_handle] = nullptr;
        allocations[buffer_handle] = nullptr;
        srvs[buffer_handle] = INVALID_HANDLE;
        uavs[buffer_handle] = INVALID_HANDLE;
        --num_buffers;
    }
}
