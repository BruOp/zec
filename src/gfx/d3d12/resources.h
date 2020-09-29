#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "wrappers.h"

namespace zec
{
    namespace dx12
    {
        constexpr size_t CONSTANT_BUFFER_ALIGNMENT = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        constexpr size_t VERTEX_BUFFER_ALIGNMENT = 4;
        constexpr size_t INDEX_BUFFER_ALIGNMENT = 4;

        struct TextureInfo
        {
            u32 width = 0;
            u32 height = 0;
            u32 depth = 0;
            u32 num_mips = 0;
            u32 array_size = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            u32 is_cubemap = 0;
        };

        struct RenderTargetInfo
        {
            u8 mssa_samples = 0;
            u8 msaa_quality = 0;
        };

        struct Texture
        {
            ID3D12Resource* resource = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
            u32 srv = INVALID_SRV;
            D3D12_CPU_DESCRIPTOR_HANDLE uav = INVALID_CPU_HANDLE;
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = INVALID_CPU_HANDLE;
            TextureInfo info = {};
            RenderTargetInfo render_target_info = {};
        };

        struct Buffer
        {
            ID3D12Resource* resource = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE uav = {};
            void* cpu_address = 0;
            u64 gpu_address = 0;
            u64 alignment = 0;
            u64 size = 0;
            u32 srv = UINT32_MAX;
            u32 cpu_accessible = false;
        };

        struct Mesh
        {
            D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
            D3D12_VERTEX_BUFFER_VIEW buffer_views[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
            BufferHandle index_buffer_handle = INVALID_HANDLE;
            BufferHandle vertex_buffer_handles[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
            u32 num_vertex_buffers = 0;
            u32 index_count = 0;
        };

        void init(Buffer& buffer, const BufferDesc& desc, D3D12MA::Allocator* allocator);
        void update_buffer(Buffer& buffer, const void* data, const u64 data_size, const u64 frame_idx);
    }
}