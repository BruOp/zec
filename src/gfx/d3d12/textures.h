#pragma once
#include <unordered_map>

#include "core/zec_types.h"
#include "gfx/rhi_public_resources.h"
#include "gfx/resource_array.h"
#include "dx_helpers.h"
// TODO: Remove this
#include "descriptor_heap.h"

namespace D3D12MA
{
    class Allocation;
}

namespace std
{
    template<>
    struct hash<zec::rhi::TextureHandle>
    {
        std::size_t operator()(const zec::rhi::TextureHandle texture_handle) const
        {
            u32 x = ((texture_handle.idx >> 16) ^ texture_handle.idx) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            return std::size_t(x);
        };
    };
}

namespace zec::rhi::dx12
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

    class DSVStore
    {
    public:
        void empty()
        {
            internal_map.clear();
        }

        void set(const TextureHandle handle, const DescriptorRangeHandle dsv)
        {
            internal_map[handle] = dsv;
        }

        DescriptorRangeHandle& operator[](const TextureHandle handle) {
            return internal_map.at(handle);
        };
        const DescriptorRangeHandle& operator[](const TextureHandle handle) const
        {
            return internal_map.at(handle);
        };

        std::unordered_map<TextureHandle, DescriptorRangeHandle>::iterator begin()
        {
            return internal_map.begin();
        }

        std::unordered_map<TextureHandle, DescriptorRangeHandle>::const_iterator begin() const
        {
            return internal_map.begin();
        }

        std::unordered_map<TextureHandle, DescriptorRangeHandle>::iterator end()
        {
            return internal_map.end();
        }

        std::unordered_map<TextureHandle, DescriptorRangeHandle>::const_iterator end() const
        {
            return internal_map.end();
        }

        std::unordered_map<TextureHandle, DescriptorRangeHandle> internal_map;
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

        // TODO: Rename this function so it's not confusing
        template<typename DescriptorDestructionCallback>
        void destroy(void (*resource_destruction_callback)(ID3D12Resource*, D3D12MA::Allocation*), DescriptorDestructionCallback&& descriptor_destruction_callback)
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
            }
            for (size_t i = 0; i < count; i++) {
                if (is_valid(uavs[i])) {
                    descriptor_destruction_callback(uavs[i]);
                    uavs[i] = INVALID_HANDLE;
                }
            }
            for (size_t i = 0; i < count; i++) {
                if (is_valid(rtvs[i])) {
                    descriptor_destruction_callback(rtvs[i]);
                    rtvs[i] = INVALID_HANDLE;
                }
            }
            srvs.empty();
            uavs.empty();
            rtvs.empty();

            for (auto [handle, dsv] : dsvs) {
                descriptor_destruction_callback(dsv);
            }
            dsvs.empty();
            count = 0;
        }

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
        DSVStore dsvs = {};
    };
}