#include "gfx/gfx.h"
#include "buffers.h"
#include "dx_utils.h"

namespace zec::gfx::dx12
{
    void BufferList::destroy()
    {
        resources.empty();
        allocations.empty();

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
