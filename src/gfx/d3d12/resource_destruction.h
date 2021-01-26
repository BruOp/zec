#pragma once
#include "pch.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "descriptor_heap.h"
#include "wrappers.h"
#include "resource_managers.h"
#include "textures.h"
#include "buffers.h"

namespace zec::gfx::dx12
{
    class ResourceDestructionQueue
    {
        struct ResourceToDelete
        {
            IUnknown* ptr = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
        };

    public:
        ResourceDestructionQueue() = default;
        ~ResourceDestructionQueue()
        {
            for (size_t i = 0; i < ARRAY_SIZE(internal_queues); i++) {
                ASSERT(internal_queues[i].size == 0);
            }
        }

        UNCOPIABLE(ResourceDestructionQueue);
        UNMOVABLE(ResourceDestructionQueue);

        Array<ResourceToDelete> internal_queues[RENDER_LATENCY] = {};
    };

    inline void queue_destruction(ResourceDestructionQueue& queue, const u64 current_frame_idx, IUnknown* d3d_ptr, D3D12MA::Allocation* allocation = nullptr)
    {
        queue.internal_queues[current_frame_idx].create_back(d3d_ptr, allocation);
    }

    void process_destruction_queue(ResourceDestructionQueue& queue, const u64 current_frame_idx);

    inline void flush_destruction_queue(ResourceDestructionQueue& queue)
    {
        for (size_t i = 0; i < ARRAY_SIZE(queue.internal_queues); i++) {
            process_destruction_queue(queue, i);
        }
    }

    template<typename Resource, typename ResourceHandle>
    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, ResourceList<typename Resource, ResourceHandle>& list);

    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, Array<Fence>& fences);

    // Destroys only the buffer list, the heaps are used to free allocated SRVs, UAVs, etc.
    void destroy(
        ResourceDestructionQueue& destruction_queue,
        const u64 current_frame_idx,
        DescriptorHeap* heaps,
        BufferList& buffer_list
    );

    // Destroys only the texture list, the heaps are used to free allocated SRVs, UAVs, etc.
    void destroy(
        ResourceDestructionQueue& destruction_queue,
        const u64 current_frame_idx,
        DescriptorHeap* heaps,
        TextureList& texture_list
    );

    template<typename Resource, typename ResourceHandle>
    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, ResourceList<typename Resource, ResourceHandle>& list)
    {
        for (size_t i = 0; i < list.size(); i++) {
            Resource& resource = list.resources[i];
            queue_destruction(queue, current_frame_idx, resource.resource, resource.allocation);
        }
        list.resources.empty();
    }
}