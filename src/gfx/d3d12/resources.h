#pragma once
#include "pch.h"
#include "gfx/public.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec
{
    namespace dx12
    {
        struct Texture
        {
            ID3D12Resource* resource = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
            u32 srv = UINT32_MAX;
            D3D12_CPU_DESCRIPTOR_HANDLE uav = {};
            u32 width = 0;
            u32 height = 0;
            u32 depth = 0;
            u32 num_mips = 0;
            u32 array_size = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            u32 is_cubemap = 0;
        };

        struct RenderTexture : public Texture
        {
            D3D12_CPU_DESCRIPTOR_HANDLE rtv;

            u32 mssa_samples = 0;
            u32 msaa_quality = 0;
        };

        struct Buffer
        {
            ID3D12Resource* resource = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
            u32 srv = UINT32_MAX;
            D3D12_CPU_DESCRIPTOR_HANDLE uav = {};
            void* cpu_address = 0;
            u64 gpu_address = 0;
            u64 alignment = 0;
            u64 size = 0;
            u32 dynamic = false;
            u32 cpu_accessible = false;
        };

        struct Mesh
        {
            D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
            D3D12_VERTEX_BUFFER_VIEW buffer_views[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
            BufferHandle index_buffer_handle = INVALID_HANDLE;
            BufferHandle vertex_buffer_handles[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
            u32 index_count = 0;
        };
    }
}