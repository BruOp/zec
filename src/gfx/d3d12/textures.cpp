#include "textures.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec::gfx::dx12
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

    // TODO: Rename this function so it's not confusing
    void TextureList::destroy(void(*resource_destruction_callback)(ID3D12Resource*, D3D12MA::Allocation*), void(*descriptor_destruction_callback)(DescriptorRangeHandle))
    {
        for (size_t i = 0; i < count; i++) {
            resource_destruction_callback(resources[i], allocations[i]);
            resources[i] = nullptr;
            allocations[i] = nullptr;
        }
        resources.empty();
        allocations.empty();

        for (size_t i = 0; i < count; i++) {
            if (is_valid(srvs[i])) {
                descriptor_destruction_callback(srvs[i]);
                srvs[i] = INVALID_HANDLE;
            }

            if (is_valid(uavs[i])) {
                descriptor_destruction_callback(uavs[i]);
                uavs[i] = INVALID_HANDLE;
            }

            if (is_valid(rtvs[i])) {
                descriptor_destruction_callback(rtvs[i]);
                rtvs[i] = INVALID_HANDLE;
            }
        }
        srvs.empty();
        uavs.empty();
        rtvs.empty();

        for (auto [ handle, dsv] : dsvs) {
            descriptor_destruction_callback(dsv);
        }
        dsvs.empty();
        count = 0;
    }
}
