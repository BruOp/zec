#pragma once
#include "pch.h"
#include "gfx/public.h"
#include "wrappers.h"
#include "utils/utils.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec
{
    namespace dx12
    {
        class ResourceDestructionQueue
        {
        public:
            ResourceDestructionQueue() = default;
            ~ResourceDestructionQueue()
            {
                ASSERT(queue.size == 0);
                ASSERT(allocations.size == 0);
            }

            UNCOPIABLE(ResourceDestructionQueue);
            UNMOVABLE(ResourceDestructionQueue);

            inline void queue_for_destruction(IUnknown* resource, D3D12MA::Allocation* allocation = nullptr)
            {
                if (resource == nullptr) {
                    return;
                }
                // So resource is a reference to a ptr of type T that is castable to IUnknown
                queue.push_back(resource);
                allocations.push_back(allocation);
            }

            void process_queue();
            void destroy();
        private:
            Array<IUnknown*> queue = {};
            Array<D3D12MA::Allocation*> allocations;
        };

        class FenceManager
        {
        public:
            FenceManager() = default;
            ~FenceManager()
            {
                ASSERT(fences.size == 0);
            };

            UNCOPIABLE(FenceManager);
            UNMOVABLE(FenceManager);

            void init(ID3D12Device* pDevice, ResourceDestructionQueue* p_destruction_queue);
            void destroy();

            Fence create_fence(const u64 initial_value = 0);
        private:
            // Not owned
            ID3D12Device* device = nullptr;
            ResourceDestructionQueue* destruction_queue = nullptr;
            // Owned
            Array<Fence> fences = {};
        };

        template< typename Resource, typename ResourceHandle>
        class ResourceList
        {
        public:
            ResourceList(ResourceDestructionQueue* destruction_queue) : destruction_queue{ destruction_queue }, resources{ } { };
            ~ResourceList()
            {
                ASSERT(resources.size == 0);
            }

            inline ResourceHandle create_back()
            {
                const size_t idx = resources.create_back();
                ASSERT(idx < u64(UINT32_MAX));
                return { u32(idx) };
            };

            void destroy()
            {
                for (size_t i = 0; i < resources.size; i++) {
                    destruction_queue->queue_for_destruction(resources[i].resource, resources[i].allocation);
                }
                resources.empty();
            }

            // TODO: Support deleting using a free list or something
            inline void destroy_resource(const ResourceHandle handle)
            {
                Resource& resource = resources.get(handle.idx);
                destruction_queue->queue_for_destruction(resource.resource, resource.allocation);
                resource = {};
            }

            inline Resource& get(const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            inline const Resource& get(const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }
            inline Resource& operator[](const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            inline const Resource& operator[](const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }

            ResourceDestructionQueue* destruction_queue = nullptr;
            Array<Resource> resources;

        };
    }
}