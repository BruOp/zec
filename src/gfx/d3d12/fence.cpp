#include "pch.h"
#include "fence.h"
#include "globals.h"
#include "utils/utils.h"

namespace zec
{
    namespace dx12
    {
        void init(Fence& fence, u64 initial_value)
        {
            DXCall(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3d_fence)));
            fence.fence_event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
            Win32Call(fence.fence_event != 0);
        }

        void destroy(Fence& fence)
        {
            queue_resource_destruction(fence.d3d_fence);
            fence.d3d_fence = nullptr;
        }

        void signal(Fence& fence, ID3D12CommandQueue* queue, u64 value)
        {
            ASSERT(fence.d3d_fence != nullptr);
            DXCall(queue->Signal(fence.d3d_fence, value));
        }
    }
}