#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "dx_helpers.h"
#include "gfx/resource_array.h"
// TODO: Remove this
#include "descriptor_heap.h"

namespace D3D12MA
{
    class Allocation;
}


namespace zec::gfx::dx12
{
    struct RenderTargetInfo
    {
        u8 mssa_samples = 0;
        u8 msaa_quality = 0;
    };

    struct Texture
    {
        ID3D12Resource* resource = nullptr;
        D3D12MA::Allocation* allocation = nullptr;
        DescriptorRangeHandle srv = INVALID_HANDLE;
        DescriptorRangeHandle uav = INVALID_HANDLE;
        DescriptorRangeHandle rtv = INVALID_HANDLE;
        DescriptorRangeHandle dsv = INVALID_HANDLE;
        TextureInfo info = {};
        RenderTargetInfo render_target_info = {};
    };

    struct DepthStencilInfo
    {
        TextureHandle handle;
        DescriptorRangeHandle dsv;
    };

    class DSVStore
    {
    public:
        size_t push_back(const DepthStencilInfo depth_stencil_info)
        {
            return data.push_back(depth_stencil_info);
        }

        DescriptorRangeHandle& operator[](const TextureHandle handle);
        const DescriptorRangeHandle& operator[](const TextureHandle handle) const;

        Array<DepthStencilInfo> data;
    };

    // This is really just a data repository
    // Most of the logic involve in updating and creating textures is not in this class!
    class TextureList
    {
    public:
        TextureList() = default;
        ~TextureList()
        {
            ASSERT(resources.size == 0);
            ASSERT(allocations.size == 0);
        }

        TextureHandle push_back(const Texture& texture);

        void destroy(void (*resource_destruction_callback)(ID3D12Resource*, D3D12MA::Allocation*), void(*descriptor_destruction_callback)(DescriptorRangeHandle));

        // Getters
        size_t size() const
        {
            return count;
        }

        size_t count = 0;
        ResourceArray<ID3D12Resource*, TextureHandle> resources = {};
        ResourceArray<D3D12MA::Allocation*, TextureHandle> allocations = {};
        ResourceArray<DescriptorRangeHandle, TextureHandle> srvs = {};
        ResourceArray<DescriptorRangeHandle, TextureHandle> uavs = {};
        ResourceArray<DescriptorRangeHandle, TextureHandle> rtvs = {};
        ResourceArray<TextureInfo, TextureHandle> infos = {};
        ResourceArray<RenderTargetInfo, TextureHandle> render_target_infos = {};
        // Note, this is not 1-1 like other arrays in this structure.
        // Instead, we loop through and find the dsv_info with matching TextureHandle.
        // Since we have so few DSVs, this should be pretty cheap.
        DSVStore dsv_infos = {};
    };
}