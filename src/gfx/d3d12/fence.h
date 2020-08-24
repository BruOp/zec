#pragma once
#include "pch.h"

namespace zec
{
    namespace dx12
    {
        struct Fence
        {
            ID3D12Fence* d3d_fence = nullptr;
            HANDLE fence_event = INVALID_HANDLE_VALUE;
        };

        void init(Fence& fence, u64 initial_value = 0);
        void destroy(Fence& fence);

        void signal_fence(Fence& fence, ID3D12CommandQueue* queue, u64 fence_value);
        void wait(u64 fence_value);
        bool signaled(u64 fence_value);
        void clear(u64 fence_value);
    }
}