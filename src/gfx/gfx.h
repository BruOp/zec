#pragma once
#include "pch.h"
#include "device.h"
#include "resources.h"
#include "swap_chain.h"

namespace zec
{
    struct Fence
    {
        ID3D12Fence* d3d_fence;
        HANDLE fence_event = INVALID_HANDLE_VALUE;
    };

    //void init_fence(Fence& fence, const u64 initial_value = 0);
    //void destroy(Fence& fence);

    struct Renderer
    {
        u64 current_cpu_frame = 0;
        u64 current_gpu_frame = 0;
        u64 current_frame_idx = 0;

        Device device;

        //Fence fence;
    };

    void init_renderer(Renderer& renderer, D3D_FEATURE_LEVEL min_feature_level);
    void destroy(Renderer& renderer);
}