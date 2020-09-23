#include "pch.h"
#include "resource_managers.h"
#include "dx_utils.h"

namespace zec
{
    namespace dx12
    {
        void ResourceDestructionQueue::process_queue()
        {
            for (size_t i = 0; i < queue.size; i++) {
                queue[i]->Release();
                if (allocations[i] != nullptr) allocations[i]->Release();
            }
            queue.empty();
            allocations.empty();
        }

        void ResourceDestructionQueue::destroy()
        {
            process_queue();
        }

        void FenceManager::init(ID3D12Device* p_device, ResourceDestructionQueue* p_destruction_queue)
        {
            destruction_queue = p_destruction_queue;
            device = p_device;
        }

        void FenceManager::destroy()
        {
            for (size_t i = 0; i < fences.size; i++) {
                Fence& fence = fences[i];
                destruction_queue->queue_for_destruction(fence.d3d_fence);
            }
            fences.empty();
            device = nullptr;
            destruction_queue = nullptr;
        }

        Fence FenceManager::create_fence(const u64 initial_value)
        {
            size_t idx = fences.create_back();
            Fence& fence = fences[idx];
            DXCall(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3d_fence)));
            fence.fence_event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
            ASSERT(fence.fence_event != 0);
            return fence;
        }
    }
}
