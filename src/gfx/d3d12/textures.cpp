#include "textures.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec::rhi::dx12
{
    TextureHandle TextureList::push_back(const Texture& texture)
    {
        ASSERT(count < UINT32_MAX);
        count++;
        TextureHandle handle = { u32(resources.push_back(texture.resource)) };
        allocations.push_back(texture.allocation);
        srvs.push_back(texture.srv);
        uavs.push_back(texture.uav);
        rtvs.push_back(texture.rtv);
        infos.push_back(texture.info);
        render_target_infos.push_back(texture.render_target_info);

        if (is_valid(texture.dsv)) {
            dsvs.set(handle, texture.dsv);
        };

        return handle;
    }

    // TODO: Add function for destroying individual textures
}
