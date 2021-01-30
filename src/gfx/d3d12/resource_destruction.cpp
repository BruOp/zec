#include "pch.h"
#include "resource_destruction.h"
#include "dx_helpers.h"
#include "command_context.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec::gfx::dx12
{
    void process_destruction_queue(ResourceDestructionQueue& queue, const u64 current_frame_idx)
    {
        ASSERT(current_frame_idx < RENDER_LATENCY);
        auto& internal_queue = queue.internal_queues[current_frame_idx];
        for (size_t i = 0; i < internal_queue.size; i++) {
            auto [resource, allocation] = internal_queue[i];
            ASSERT(resource != nullptr);
            resource->Release();
            if (allocation != nullptr) allocation->Release();
        }
        internal_queue.empty();
    }

    void process_destruction_queue(AsyncResourceDestructionQueue& queue, CommandContextManager& command_pool_manager)
    {
        u64 completed_values[ARRAY_SIZE(command_pool_manager.fences)];
        for (size_t i = 0; i < ARRAY_SIZE(command_pool_manager.fences); i++) {
            auto& fence = command_pool_manager.fences[i];
            completed_values[i] = get_completed_value(fence);
        }
        size_t node_idx = 0;
        while (node_idx < queue.internal_queue.size) {
            const auto node = queue.internal_queue[node_idx];
            // If it's done
            if (completed_values[size_t(node.cmd_receipt.queue_type)] <= node.cmd_receipt.fence_value) {

                // Swap with the back, unless there's only one element
                if (queue.internal_queue.size == 1) {
                    queue.internal_queue.pop_back();
                }
                else {
                    queue.internal_queue[node_idx] = queue.internal_queue.pop_back();
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

    void destroy(ResourceDestructionQueue& queue, const u64 current_frame_idx, Fence& fence)
    {
        queue_destruction(queue, current_frame_idx, fence.d3d_fence);
        fence.d3d_fence = nullptr;
    }

}
