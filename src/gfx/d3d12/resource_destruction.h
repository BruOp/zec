#pragma once
#include "pch.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "gfx/d3d12/wrappers.h"
#include "gfx/d3d12/resources.h"
#include "gfx/d3d12/resource_managers.h"

namespace zec
{
    namespace dx12
    {
        struct ResourceDestructionQueue
        {
        public:
            ResourceDestructionQueue() = default;
            ~ResourceDestructionQueue()
            {
                ASSERT(resource_ptrs.size == 0);
                ASSERT(allocations.size == 0);
            }

            UNCOPIABLE(ResourceDestructionQueue);
            UNMOVABLE(ResourceDestructionQueue);

            Array<IUnknown*> resource_ptrs = {};
            Array<D3D12MA::Allocation*> allocations;
        };

        inline void queue_destruction(ResourceDestructionQueue& queue, IUnknown* d3d_ptr, D3D12MA::Allocation* allocation = nullptr)
        {
            queue.resource_ptrs.push_back(d3d_ptr);
            queue.allocations.push_back(allocation);
        }

        void process_destruction_queue(ResourceDestructionQueue& queue);

        void destroy(ResourceDestructionQueue& queue)
        {
            process_destruction_queue(queue);
        }

        template<typename Resource, typename ResourceHandle>
        void destroy(ResourceDestructionQueue& queue, ResourceList<typename Resource, ResourceHandle>& list);

        void destroy(ResourceDestructionQueue& queue, Array<Fence>& fences);

        void destroy(
            ResourceDestructionQueue& queue,
            TextureList& texture_list,
            DescriptorHeap& rtv_descriptor_heap,
            SwapChain& swap_chain
        );

        // Destroys only the texture list, the heaps are used to free allocated SRVs, UAVs, etc.
        void destroy(
            ResourceDestructionQueue& destruction_queue,
            DescriptorHeap& srv_descriptor_heap,
            DescriptorHeap& uav_descriptor_heap,
            DescriptorHeap& rtv_descriptor_heap,
            TextureList& texture_list
        );
    }
}