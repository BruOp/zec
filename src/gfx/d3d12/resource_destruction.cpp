#include "resource_destruction.h"
#include <array>
#include "dx_helpers.h"
#include "command_context.h"

namespace zec::rhi::dx12
{
    ResourceDestructionQueue::~ResourceDestructionQueue()
    {
        for (size_t i = 0; i < std::size(internal_queues); i++) {
            ASSERT(internal_queues[i].size == 0);
        }
    }

    void ResourceDestructionQueue::process(const u64 current_frame_idx)
    {
        ASSERT(current_frame_idx < RENDER_LATENCY);
        auto& internal_queue = internal_queues[current_frame_idx];
        for (size_t i = 0; i < internal_queue.size; i++) {
            auto [resource, allocation] = internal_queue[i];
            ASSERT(resource != nullptr);
            resource->Release();
            if (allocation != nullptr) allocation->Release();
        }
        internal_queue.empty();
    }

    void AsyncResourceDestructionQueue::process(const u64 per_queue_completed_values[])
    {
        size_t node_idx = 0;
        while (node_idx < internal_queue.size) {
            const auto node =internal_queue[node_idx];
            // If it's done
            if (per_queue_completed_values[size_t(node.cmd_receipt.queue_type)] >= node.cmd_receipt.fence_value) {

                // Swap with the back, unless there's only one element
                if (internal_queue.size == 1) {
                   internal_queue.pop_back();
                }
                else {
                   internal_queue[node_idx] = internal_queue.pop_back();
                }

                node.ptr->Release();
                if (node.allocation) {
                    node.allocation->Release();
                }
            }
            else {
                // continue
                ++node_idx;
            }
        }
    }

    void AsyncResourceDestructionQueue::flush()
    {
        for (size_t i = 0; i < internal_queue.size; i++) {
            auto& node = internal_queue[i];
            node.ptr->Release();
            if (node.allocation) {
                node.allocation->Release();
            }
        }
        internal_queue.empty();
    }

    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, Fence& fence)
    {
        queue.enqueue(current_frame_idx, fence.d3d_fence);
        fence.d3d_fence = nullptr;
    }

    void ResourceDestructionQueue::flush()
    {
        for (size_t i = 0; i < std::size(internal_queues); i++) {
            process(i);
        }
    }

    void ResourceDestructionQueue::enqueue(const u64 current_frame_idx, IUnknown* d3d_ptr, D3D12MA::Allocation* allocation)
    {
        ASSERT(current_frame_idx < RENDER_LATENCY);
        internal_queues[current_frame_idx].create_back(d3d_ptr, allocation);
    }

}
