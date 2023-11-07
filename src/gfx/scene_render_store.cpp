#include "scene_render_store.h"
#include "rhi.h"

namespace zec
{
    static_assert(sizeof(MeshTransformData) % 16 == 0);

    void SceneRenderStore::Iterator::operator++()
    {
        ASSERT(current_submesh_offset <= pscene_render_store->submeshes.get_size());
        if (current_mesh_offset == pscene_render_store->meshes.get_size() && current_submesh_offset == current_mesh->submeshes.size)
        {
            ASSERT_FAIL("Shouldn't have incremented this again?");
            return;
        }

        ++current_submesh_offset;
        if (current_submesh_offset == current_mesh->submeshes.size)
        {
            current_mesh = pscene_render_store->meshes.end();
            submesh = pscene_render_store->submeshes.end();
            current_submesh_offset = 0;
            // Search for next valid mesh
            for (++current_mesh_offset; current_mesh_offset < pscene_render_store->meshes.get_size(); ++current_mesh_offset)
            {
                current_mesh = &pscene_render_store->meshes[current_mesh_offset];
                if ((current_mesh->state_flags & MESH_STATE_FLAG_DISABLED) == 0u)
                {
                    // Stop when we've found a valid mesh
                    submesh = &pscene_render_store->submeshes[current_mesh->submeshes.offset];
                    return;
                }
            }
            // If we've reached this point in the code, it's time to return nullptr :(
        }
        else
        {
            submesh = &pscene_render_store->submeshes[current_mesh->submeshes.offset + current_submesh_offset];
        }
    }

    MeshHandle SceneRenderStore::add_mesh(const TypedArrayView<rhi::Draw>& in_submeshes)
    {
        ASSERT(in_submeshes.begin() != nullptr);
        size_t start_idx = submeshes.get_size();
        submeshes.grow(in_submeshes.get_size());
        memory::copy(&submeshes[start_idx], in_submeshes.begin(), in_submeshes.get_byte_size());
        mesh_draw_data.create_back();
        return { u32(meshes.create_back(
            ArrayView{ u32(in_submeshes.get_size()), u32(start_idx) },
            MESH_STATE_FLAG_DEFAULT))
        };
    }

    const TypedArrayView<rhi::Draw> SceneRenderStore::get_submeshes(const MeshHandle mesh_handle)
    {
        const Mesh& mesh = meshes[mesh_handle.idx];
        return {
            mesh.submeshes.size,
            &submeshes[mesh.submeshes.offset]
        };
    }

    void SceneRenderStore::push_draw_data(rhi::Renderer& renderer, rhi::BufferHandle draw_data_sb)
    {
        renderer.buffers_update(draw_data_sb, mesh_draw_data.begin(), mesh_draw_data.get_byte_size());
    }
}
