#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "wrappers.h"
#include "dx_helpers.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec::gfx::dx12
{
    class CommandContextPool;


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

        void flush();

        inline void enqueue(const u64 current_frame_idx, IUnknown* d3d_ptr, D3D12MA::Allocation* allocation = nullptr)
        {
            internal_queues[current_frame_idx].create_back(d3d_ptr, allocation);
        }

        Array<ResourceToDelete> internal_queues[RENDER_LATENCY] = {};
    };

    class AsyncResourceDestructionQueue
    {
    public:
        struct Node
        {
            IUnknown* ptr = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
            CmdReceipt cmd_receipt;
        };

        Array<Node> internal_queue;

        void flush();

        inline void enqueue(const CmdReceipt receipt, IUnknown* d3d_ptr, D3D12MA::Allocation* allocation = nullptr)
        {
            internal_queue.create_back(d3d_ptr, allocation, receipt);
        }

    };

    void process_destruction_queue(ResourceDestructionQueue& queue, const u64 current_frame_idx);

    void process_destruction_queue(AsyncResourceDestructionQueue& queue, const u64 per_queue_completed_values[]);

    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, Fence& fence);

    template<typename SoA>
    inline void destroy_resources(ResourceDestructionQueue& queue, const u64 current_frame_idx, SoA& list)
    {
        for (size_t i = 0; i < list.size(); i++) {
            ID3D12Resource* resource = list.resources[i];
            D3D12MA::Allocation* allocation = list.allocations[i];
            queue_destruction(queue, current_frame_idx, resource, allocation);
        }
    }

    template<typename T, typename H>
    inline void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, DXPtrArray<T, H>& resources)
    {
        for (T* resource : resources) {
            queue_destruction(queue, current_frame_idx, resource);
        }
    }
}