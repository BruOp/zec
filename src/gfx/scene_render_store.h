#include "rhi_public_resources.h"
#include "core/zec_math.h"
#include "core/array.h"

namespace zec
{
    RESOURCE_HANDLE(MeshHandle);
    RESOURCE_HANDLE(SubMeshHandle);

    // Forward declarations
    namespace rhi
    {
        class Renderer;
    }

    enum MeshStateFlags : u8
    {
        MESH_STATE_FLAG_DEFAULT = 0,
        MESH_STATE_FLAG_DISABLED = 1 < 1,
    };

    struct Mesh
    {
        ArrayView submeshes{};
        MeshStateFlags state_flags = MESH_STATE_FLAG_DEFAULT;
    };

    struct MeshTransformData
    {
        zec::mat34 model_transform = {};
        zec::mat3 normal_transform = {};
    };

    class SceneRenderStore
    {
    public:
        class Iterator
        {
        public:
            Iterator(SceneRenderStore const* pscene_render_store) : pscene_render_store{ pscene_render_store } {};
            Iterator(
                SceneRenderStore const* pscene_render_store,
                size_t current_mesh_offset,
                size_t current_submesh_offset,
                Mesh const* current_mesh,
                rhi::Draw const* submesh)
            : pscene_render_store{ pscene_render_store }
            , current_mesh_offset{ current_mesh_offset }
            , current_submesh_offset{ current_submesh_offset }
            , current_mesh{ current_mesh }
            , submesh{ submesh }
            {};

            void operator++();

            const rhi::Draw& operator*() const { return *submesh; };

            bool operator==(const Iterator& other) { return submesh == other.submesh; };

            size_t get_mesh_offset() const { return current_mesh_offset; };
            size_t get_submesh_offset() const { return current_submesh_offset; };
        private:
            SceneRenderStore const* pscene_render_store = nullptr;
            size_t current_mesh_offset = 0;
            size_t current_submesh_offset = 0;
            Mesh const* current_mesh = nullptr;
            rhi::Draw const* submesh = nullptr;
        };

        MeshHandle add_mesh(const TypedArrayView<rhi::Draw>& submeshes);
        const TypedArrayView<rhi::Draw> get_submeshes(const MeshHandle mesh_handle);
        void set_draw_data(const MeshHandle mesh_handle, const MeshTransformData& draw_data)
        {
            mesh_draw_data[mesh_handle.idx] = draw_data;
        }

        TypedArrayView<const MeshTransformData> get_draw_data_view() {
            return { mesh_draw_data.get_size(), mesh_draw_data.begin() };
        };

        Iterator begin() const {
            return Iterator( this, 0, 0, meshes.data, submeshes.data );
        };

        Iterator end() const {
            return Iterator{ this, meshes.get_size(), 0, meshes.end(), submeshes.end() };
        };

        void push_draw_data(rhi::Renderer& renderer, rhi::BufferHandle draw_data_sb);

    private:
        Array<Mesh> meshes{};
        Array<rhi::Draw> submeshes{};
        Array<MeshTransformData> mesh_draw_data{};
    };

}