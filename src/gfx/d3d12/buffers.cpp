#include "pch.h"
#include "gfx/gfx.h"
#include "buffers.h"
#include "dx_utils.h"

namespace zec::gfx::dx12
{
    void BufferList::destroy(void(*resource_destruction_callback)(ID3D12Resource*, D3D12MA::Allocation*), void(*descriptor_destruction_callback)(D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorRangeHandle))
    {
        for (size_t i = 0; i < num_buffers; i++) {
            resource_destruction_callback(resources[i], allocations[i]);
            resources[i] = nullptr;
            allocations[i] = nullptr;
        }
        resources.empty();
        allocations.empty();

        for (size_t i = 0; i < num_buffers; i++) {
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

    BufferHandle zec::gfx::dx12::BufferList::push_back(const Buffer& buffer)
    {
        ASSERT(resources.size < UINT32_MAX);
        ++num_buffers;
        u32 idx = u32(resources.push_back(buffer.resource));
        allocations.push_back(buffer.allocation);
        infos.push_back(buffer.info);
        srvs.push_back(buffer.srv);
        uavs.push_back(buffer.uav);
        return { idx };
    }
}
