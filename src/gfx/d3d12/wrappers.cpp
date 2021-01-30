#include "pch.h"
#include "wrappers.h"
#include "dx_utils.h"

namespace zec::gfx::dx12
{
    void signal(Fence& fence, ID3D12CommandQueue* queue, u64 value)
    {
        ASSERT(fence.d3d_fence != nullptr);
        DXCall(queue->Signal(fence.d3d_fence, value));
    }

    void wait(Fence& fence, u64 fence_value)
    {
        OPTICK_EVENT("Fence Wait");
        ASSERT(fence.d3d_fence != nullptr);
        if (fence.d3d_fence->GetCompletedValue() < fence_value) {
            fence.d3d_fence->SetEventOnCompletion(fence_value, fence.fence_event);
            WaitForSingleObjectEx(fence.fence_event, 5000, FALSE);
        }
    }

    bool is_signaled(Fence& fence, u64 fence_value)
    {
        ASSERT(fence.d3d_fence != nullptr);
        return fence.d3d_fence->GetCompletedValue() >= fence_value;
    }

    void clear(Fence& fence, u64 fence_value)
    {
        ASSERT(fence.d3d_fence != nullptr);
        fence.d3d_fence->Signal(fence_value);
    }
}
