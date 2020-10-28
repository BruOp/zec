#include "pch.h"
#include "resource_managers.h"
#include "dx_utils.h"
#include "globals.h"

namespace zec::dx12
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

    namespace TextureUtils
    {
        TextureHandle push_back(TextureList& list, Texture& texture)
        {
            ASSERT(list.resources.size < UINT32_MAX);
            TextureHandle handle = { u32(list.resources.push_back(texture.resource)) };
            list.allocations.push_back(texture.allocation);
            list.srvs.push_back(texture.srv);
            list.uavs.push_back(texture.uav);
            list.rtvs.push_back(texture.rtv);
            list.infos.push_back(texture.info);
            list.render_target_infos.push_back(texture.render_target_info);

            if (DescriptorUtils::is_valid(texture.dsv)) {
                list.dsv_infos.push_back({ handle, texture.dsv });
            };

            return handle;
        }

        DescriptorRangeHandle get_dsv(TextureList& texture_list, TextureHandle handle)
        {
            for (size_t i = 0; i < texture_list.dsv_infos.size; i++) {
                const auto& dsv_info = texture_list.dsv_infos[i];
                if (dsv_info.handle == handle) { return dsv_info.dsv; }
            }
            throw std::runtime_error("This texture was not created as a depth stencil buffer");
        }
    }
}
