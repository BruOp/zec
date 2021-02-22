#include "pch.h"
#include "meshes.h"
#include "gfx/gfx.h"
#include "render_context.h"

using namespace zec::gfx;
using namespace zec::gfx::dx12;

namespace zec::gfx::meshes
{
    static MeshStore g_meshes = {};

    D3D12_INDEX_BUFFER_VIEW create_index_buffer_view(const BufferInfo& buffer_info)
    {
        D3D12_INDEX_BUFFER_VIEW buffer_view{};
        buffer_view.BufferLocation = buffer_info.gpu_address;
        ASSERT(buffer_info.total_size < u64(UINT32_MAX));
        buffer_view.SizeInBytes = u32(buffer_info.total_size);

        switch (buffer_info.stride) {
        case 2u:
            buffer_view.Format = DXGI_FORMAT_R16_UINT;
            break;
        case 4u:
            buffer_view.Format = DXGI_FORMAT_R32_UINT;
            break;
        default:
            throw std::runtime_error("Cannot create an index buffer that isn't u16 or u32");
        }
        return buffer_view;
    }

    MeshHandle create(const BufferHandle index_buffer, const BufferHandle* vertex_buffers, u32 num_vertex_buffers)
    {
        Mesh mesh{};
        const RenderContext& render_context = get_render_context();

        mesh.index_buffer_handle = index_buffer;
        const BufferInfo& index_buffer_info = render_context.buffers.infos[mesh.index_buffer_handle];
        mesh.index_buffer_view = create_index_buffer_view(index_buffer_info);
        mesh.index_count = index_buffer_info.total_size / index_buffer_info.stride;
        mesh.num_vertex_buffers = num_vertex_buffers;
        ASSERT(num_vertex_buffers > 0 && num_vertex_buffers < MAX_NUM_MESH_VERTEX_BUFFERS);
        for (size_t i = 0; i < num_vertex_buffers; i++) {
            mesh.vertex_buffer_handles[i] = vertex_buffers[i];

            const BufferInfo& vertex_info = render_context.buffers.infos[vertex_buffers[i]];
            D3D12_VERTEX_BUFFER_VIEW& view = mesh.buffer_views[i];
            view.BufferLocation = vertex_info.gpu_address;
            view.StrideInBytes = vertex_info.stride;
            view.SizeInBytes = vertex_info.total_size;
        }

        return { u32(g_meshes.push_back(mesh)) };
    }

    MeshHandle create(CommandContextHandle cmd_ctx, MeshDesc mesh_desc)
    {
        Mesh mesh{};
        RenderContext& render_context = get_render_context();
        // Index buffer creation
        {
            ASSERT(mesh_desc.index_buffer_desc.usage == RESOURCE_USAGE_INDEX);
            mesh.index_buffer_handle = buffers::create(mesh_desc.index_buffer_desc);

            // Copy the index data
            buffers::set_data(cmd_ctx, mesh.index_buffer_handle, mesh_desc.index_buffer_data, mesh_desc.index_buffer_desc.byte_size);

            const BufferInfo& index_buffer_info = render_context.buffers.infos[mesh.index_buffer_handle];
            mesh.index_buffer_view = create_index_buffer_view(index_buffer_info);
            mesh.index_count = mesh_desc.index_buffer_desc.byte_size / mesh_desc.index_buffer_desc.stride;
        }

        for (size_t i = 0; i < ARRAY_SIZE(mesh_desc.vertex_buffer_descs); i++) {
            const BufferDesc& attr_desc = mesh_desc.vertex_buffer_descs[i];
            if (attr_desc.usage == RESOURCE_USAGE_UNUSED) break;

            // TODO: Is this actually needed?
            ASSERT(attr_desc.usage == RESOURCE_USAGE_VERTEX);
            ASSERT(attr_desc.type == BufferType::DEFAULT);

            mesh.vertex_buffer_handles[i] = buffers::create(attr_desc);

            // Copy the attribute data
            buffers::set_data(cmd_ctx, mesh.vertex_buffer_handles[i], mesh_desc.vertex_buffer_data[i], attr_desc.byte_size);

            const BufferInfo& vertex_info = render_context.buffers.infos[mesh.vertex_buffer_handles[i]];

            D3D12_VERTEX_BUFFER_VIEW& view = mesh.buffer_views[i];
            view.BufferLocation = vertex_info.gpu_address;
            view.StrideInBytes = attr_desc.stride;
            view.SizeInBytes = attr_desc.byte_size;
            mesh.num_vertex_buffers++;
        }

        // TODO: Support blend weights, indices
        return { u32(g_meshes.push_back(mesh)) };
    }
}

namespace zec::gfx::cmd
{
    void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_handle)
    {
        const Mesh& mesh = zec::gfx::meshes::g_meshes[mesh_handle];

        RenderContext& render_context = get_render_context();
        ID3D12GraphicsCommandList* cmd_list = get_command_list(render_context, ctx);

        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->IASetIndexBuffer(&mesh.index_buffer_view);
        cmd_list->IASetVertexBuffers(0, mesh.num_vertex_buffers, mesh.buffer_views);
        cmd_list->DrawIndexedInstanced(mesh.index_count, 1, 0, 0, 0);
    }
}