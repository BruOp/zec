#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "utils/utils.h"
#include "dx_utils.h"
#include "descriptor_heap.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include <DirectXTex.h>

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

    struct TextureList
    {
        TextureList() = default;
        ~TextureList()
        {
            ASSERT(resources.size == 0);
            ASSERT(allocations.size == 0);
        }

        Array<ID3D12Resource*> resources = {};
        Array<D3D12MA::Allocation*> allocations = {};
        Array<DescriptorRangeHandle> srvs = {};
        Array<DescriptorRangeHandle> uavs = {};
        Array<DescriptorRangeHandle> rtvs = {};
        Array<TextureInfo> infos = {};
        Array<RenderTargetInfo> render_target_infos = {};
        // Note, this is not 1-1 like other arrays in this structure.
        // Instead, we loop through and find the dsv_info with matching TextureHandle.
        // Since we have so few DSVs, this should be pretty cheap.
        Array<DepthStencilInfo> dsv_infos = {};
    };

    namespace texture_utils
    {
        TextureHandle push_back(TextureList& list, const Texture& texture);

        inline u32 get_srv_index(TextureList& texture_list, TextureHandle handle)
        {
            return descriptor_utils::get_offset(texture_list.srvs[handle.idx]);
        };

        inline u32 get_uav_index(TextureList& texture_list, TextureHandle handle)
        {
            return descriptor_utils::get_offset(texture_list.uavs[handle.idx]);
        };

        inline DescriptorRangeHandle get_rtv(TextureList& texture_list, TextureHandle handle)
        {
            return texture_list.rtvs[handle.idx];
        };

        DescriptorRangeHandle get_dsv(TextureList& texture_list, TextureHandle handle);

        inline void set_rtv(TextureList& texture_list, TextureHandle handle, DescriptorRangeHandle rtv)
        {
            texture_list.rtvs[handle.idx] = rtv;
        };

        inline TextureInfo& get_texture_info(TextureList& texture_list, TextureHandle texture_handle)
        {
            return texture_list.infos[texture_handle.idx];
        }

        inline RenderTargetInfo& get_render_target_info(TextureList& texture_list, TextureHandle handle)
        {
            return texture_list.render_target_infos[handle.idx];
        };
    }
}