#include "pch.h"
#include "resources.h"
#include "utils/utils.h"
#include "globals.h"
#include "descriptor_heap.h"

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

        void wait(Fence& fence, u64 fence_value)
        {
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

        void destroy(Texture& texture)
        {
            queue_resource_destruction(texture.resource);
        }

        void destroy(RenderTexture& render_texture)
        {
            destroy(render_texture.texture);
            free_persistent_alloc(rtv_descriptor_heap, render_texture.rtv);
        }
    }
}
