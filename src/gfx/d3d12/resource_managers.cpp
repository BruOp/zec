#include "pch.h"
#include "resource_managers.h"
#include "dx_utils.h"
#include "globals.h"

namespace zec
{
    namespace dx12
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

        TextureHandle push_back(TextureList& list, Texture& texture)
        {
            ASSERT(list.resources.size < UINT32_MAX);
            TextureHandle handle = { u32(list.resources.push_back(texture.resource)) };
            list.allocations.push_back(texture.allocation);
            list.srv_indices.push_back(texture.srv);
            list.uavs.push_back(texture.uav);
            list.rtvs.push_back(texture.rtv);
            list.infos.push_back(texture.info);
            list.render_target_infos.push_back(texture.render_target_info);
            return handle;
        }
    }
}
