#include "pch.h"
#include "meshes.h"
#include "globals.h"
#include "buffers.h"

using namespace zec::gfx::dx12;

namespace zec::gfx::meshes
{
    MeshHandle create(MeshDesc mesh_desc)
    {
        Mesh mesh{};
        // Index buffer creation
        {
            ASSERT(mesh_desc.index_buffer_desc.usage == RESOURCE_USAGE_INDEX);
            mesh.index_buffer_handle = buffers::create(mesh_desc.index_buffer_desc);

            const Buffer& index_buffer = g_buffers[mesh.index_buffer_handle];
            mesh.index_buffer_view.BufferLocation = index_buffer.gpu_address;
            ASSERT(index_buffer.size < u64(UINT32_MAX));
            mesh.index_buffer_view.SizeInBytes = u32(index_buffer.size);

            switch (mesh_desc.index_buffer_desc.stride) {
            case 2u:
                mesh.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
                break;
            case 4u:
                mesh.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
                break;
            default:
                throw std::runtime_error("Cannot create an index buffer that isn't u16 or u32");
            }

            mesh.index_count = mesh_desc.index_buffer_desc.byte_size / mesh_desc.index_buffer_desc.stride;
        }

        for (size_t i = 0; i < ARRAY_SIZE(mesh_desc.vertex_buffer_descs); i++) {
            const BufferDesc& attr_desc = mesh_desc.vertex_buffer_descs[i];
            if (attr_desc.usage == RESOURCE_USAGE_UNUSED) break;

            // TODO: Is this actually needed?
            ASSERT(attr_desc.usage == RESOURCE_USAGE_VERTEX);
            ASSERT(attr_desc.type == BufferType::DEFAULT);

            mesh.vertex_buffer_handles[i] = buffers::create(attr_desc);
            const Buffer& buffer = g_buffers[mesh.vertex_buffer_handles[i]];

            D3D12_VERTEX_BUFFER_VIEW& view = mesh.buffer_views[i];
            view.BufferLocation = buffer.gpu_address;
            view.StrideInBytes = attr_desc.stride;
            view.SizeInBytes = attr_desc.byte_size;
            mesh.num_vertex_buffers++;
        }

        // TODO: Support blend weights, indices
        return { u32(g_meshes.push_back(mesh)) };
    }
}