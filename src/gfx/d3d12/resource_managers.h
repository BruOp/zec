#pragma once
#include "pch.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "gfx/public_resources.h"
#include "wrappers.h"
#include "resources.h"
#include "utils/utils.h"

namespace zec
{
    namespace dx12
    {

        template<typename List, typename ResourceHandle>
        inline ID3D12Resource* get_resource(const List& list, const ResourceHandle handle)
        {
            return list.resources[handle.idx];
        };

        template<typename List, typename ResourceHandle>
        inline void set_resource(List& list, ID3D12Resource* resource, const ResourceHandle handle)
        {
            list.resources[handle.idx] = resource;
        }

        Fence create_fence(Array<Fence>& fence_list, const u64 initial_value = 0);

        template< typename Resource, typename ResourceHandle>
        struct ResourceList
        {
        public:
            ResourceList() = default;
            ~ResourceList()
            {
                ASSERT(resources.size == 0);
            }

            ResourceHandle create_back()
            {
                const size_t idx = resources.create_back();
                ASSERT(idx < u64(UINT32_MAX));
                return { u32(idx) };
            };

            // WARNING: This does not release or destroy the resource in any way.
            // If you need to destroy the resource you must use the corresponding `destroy` function, if it exists.
            // Instead, this "resets" the resource to it's default initialized state.
            // TODO: Is this useful right now?
            // TODO: Support deleting using a free list or something
            void remove(const ResourceHandle handle)
            {
                resources.get(handle.idx) = {};
            }

            Resource& get(const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            const Resource& get(const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }
            Resource& operator[](const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            const Resource& operator[](const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }

            size_t size()
            {
                return resources.size;
            }

            Array<Resource> resources;
        };

        //template<typename T, typename ResourceHandle>
        //class ResourceMap
        //{
        //public:


        //private:
        //    struct Hasher
        //    {
        //        size_t operator()(const ResourceHandle handle)
        //        {
        //            return size_t(handle.idx);
        //        }
        //    };

        //    ska::bytell_hash_map<ResourceHandle, T, Hasher> map;
        //};

        struct DepthStencilInfo
        {
            TextureHandle handle;
            u32 dsv;
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
            Array<u32> srv_indices = {};
            Array<u32> uav_indices = {};
            Array<u32> rtv_indices = {};
            Array<TextureInfo> infos = {};
            Array<RenderTargetInfo> render_target_infos = {};
            // Note, this is not 1-1 like other arrays in this structure.
            // Instead, we loop through and find the dsv_info with matching TextureHandle.
            // Since we have so few DSVs, this should be pretty cheap.
            Array<DepthStencilInfo> dsv_infos = {};
        };

        namespace TextureUtils
        {
            TextureHandle push_back(TextureList& list, Texture& texture);

            inline u32 get_srv_index(TextureList& texture_list, TextureHandle handle)
            {
                return texture_list.srv_indices[handle.idx];
            };

            inline u32 get_rtv_index(TextureList& texture_list, TextureHandle handle)
            {
                return texture_list.rtv_indices[handle.idx];
            };

            u32 get_dsv_index(TextureList& texture_list, TextureHandle handle);

            inline void set_rtv_index(TextureList& texture_list, TextureHandle handle, u32 rtv_index)
            {
                texture_list.rtv_indices[handle.idx] = rtv_index;
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
}