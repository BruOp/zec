#pragma once
#include "pch.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "gfx/public_resources.h"
#include "wrappers.h"
#include "resources.h"
#include "utils/utils.h"

namespace zec::gfx::dx12
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

        Array<Resource> resources = {};
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

}