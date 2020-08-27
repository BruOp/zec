#pragma once
#include "pch.h"

namespace zec
{
    namespace dx12
    {
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

        struct Buffer
        {
            ID3D12Resource* resource = nullptr;
            u32 srv = UINT32_MAX;
            u64 curr_buffer = 0;
            void* cpu_address = 0;
            u64 gpu_address = 0;
            u64 alignment = 0;
            u64 size = 0;
            u32 dynamic = false;
            u32 cpu_accessible = false;
            ID3D12Heap* heap = nullptr;
            u64 heap_offset = 0;
            u64 upload_frame = UINT64_MAX;
        };

        void destroy(Buffer& buffer);

    }
}