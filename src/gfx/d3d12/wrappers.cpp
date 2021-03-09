#include "wrappers.h"
#include "dx_utils.h"

#include "optick/optick.h"

namespace zec::gfx::dx12
{
    Fence create_fence(ID3D12Device* device, const u64 initial_value)
    {
        Fence fence;
        DXCall(device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3d_fence)));
        fence.fence_event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        ASSERT(fence.fence_event != 0);
        return fence;
    }

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
