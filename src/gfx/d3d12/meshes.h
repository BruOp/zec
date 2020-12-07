#pragma once
#include "pch.h"
#include "gfx/public_resources.h"

namespace zec::gfx::dx12
{
    struct Mesh
    {
        D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
        D3D12_VERTEX_BUFFER_VIEW buffer_views[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
        BufferHandle index_buffer_handle = INVALID_HANDLE;
        BufferHandle vertex_buffer_handles[MAX_NUM_MESH_VERTEX_BUFFERS] = {};
        u32 num_vertex_buffers = 0;
        u32 index_count = 0;
    };
}
