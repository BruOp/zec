#pragma once
#include "pch.h"

namespace zec
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


    struct RenderTexture
    {
        Texture texture;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv;

        u32 mssa_samples = 0;
        u32 msaa_quality = 0;
    };
}