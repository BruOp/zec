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

        void signal(Fence& fence, ID3D12CommandQueue* queue, u64 fence_value);
        void wait(Fence& fence, u64 fence_value);
        bool is_signaled(Fence& fence, u64 fence_value);
        void clear(Fence& fence, u64 fence_value);

        struct Texture
        {
            ID3D12Resource* resource = nullptr;
            u32 srv = UINT32_MAX;
            D3D12_CPU_DESCRIPTOR_HANDLE uav;
            u32 width = 0;
            u32 height = 0;
            u32 depth = 0;
            u32 num_mips = 0;
            u32 array_size = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            u32 is_cubemap = 0;
        };

        void destroy(Texture& texture);

        struct RenderTexture
        {
            Texture texture;
            D3D12_CPU_DESCRIPTOR_HANDLE rtv;

            u32 mssa_samples = 0;
            u32 msaa_quality = 0;
        };

        void destroy(RenderTexture& render_texture);
    }
}