#include "pch.h"
#include "resource_managers.h"
#include "dx_utils.h"
#include "globals.h"

namespace zec::gfx::dx12
{
    Fence create_fence(Array<Fence>& fence_list, const u64 initial_value)
    {
        size_t idx = fence_list.create_back();
        Fence& fence = fence_list[idx];
        DXCall(g_device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3d_fence)));
        fence.fence_event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        ASSERT(fence.fence_event != 0);
        return fence;
    }

}
