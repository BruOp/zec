#include <stdint.h>

#include <core/array.h>
#include <core/allocators.hpp>
#include <core/zec_math.h>
#include <core/zec_types.h>
#include <gfx/rhi_public_resources.h>

namespace zec::assets
{

    struct AttributeMetaData
    {
        char const* name = nullptr;
        const u32 stride = 0;
        const rhi::MeshAttribute type;
        const bool required = false;
    };
    static constexpr AttributeMetaData attr_meta_data[] = {
        {.name = "POSITION",    .stride = sizeof(vec3), .type = rhi::MeshAttribute::POSITION, .required = true },
        {.name = "NORMAL",      .stride = sizeof(vec3), .type = rhi::MeshAttribute::NORMAL, .required = true },
        {.name = "TEXCOORD_0",  .stride = sizeof(vec2), .type = rhi::MeshAttribute::TEXCOORD, .required = true },
        {.name = "COLOR_0",     .stride = sizeof(vec3), .type = rhi::MeshAttribute::COLOR, .required = false },
        {.name = "JOINTS_0",    .stride = sizeof(vec4), .type = rhi::MeshAttribute::BLENDINDICES, .required = false },
        {.name = "WEIGHTS_0",   .stride = sizeof(vec4), .type = rhi::MeshAttribute::BLENDWEIGHTS, .required = false }
    };

    struct Allocation
    {
        size_t byte_size = 0;
        void* ptr = nullptr;
    };

    struct String
    {
        size_t byte_size;
        char* ptr;

        explicit operator Allocation() const
        {
            return {
                .byte_size = byte_size,
                .ptr = reinterpret_cast<uint8_t*>(ptr),
            };
        }
    };

    struct AssetFile
    {
        u32 version = 0;
        u32 num_component_descs = 0;
        Allocation components_blob;
        Allocation binary_blob;
    };

    RESOURCE_HANDLE(AssetComponentId);

    // The different components that we store per model
    enum struct AssetComponentType : u16
    {
        RenderNode,
        TransformNode,
        Mesh,
        AABB,
        Submesh,
        Material,
        Texture,
        // TODO -- Sampler,

        Count,
        Invalid = 0,
    };

    /// <summary>
    /// Describes a set of components descriptors in our asset file.
    /// Offset is how far into the file we have to index to start accessing those component descs
    /// </summary>
    struct AssetComponentDesc
    {
        AssetComponentType type = AssetComponentType::Invalid;
        u32 num_components = 0;
        size_t byte_offset = 0;
    };

    /// <summary>
    /// Indexes directly into our binary blob
    /// </summary>
    struct BufferView
    {
        u32 offset = 0;
        u32 length = 0;
        u32 stride = 0;
    };

    /// <summary>
    /// Describes different "attributes" for our submeshes
    /// </summary>
    struct VertexAttribute
    {
        BufferView buffer_view_id = {};
        rhi::MeshAttribute attribute = rhi::MeshAttribute::INVALID;
    };

    struct TextureDesc
    {
        ArrayView path{};
    };

    struct RenderNode
    {
        AssetComponentId transform_id = {};
        AssetComponentId mesh_id = {};
    };

    struct TransformNode
    {
        AssetComponentId parent_transform = {};
        vec3 translation = { 0.f, 0.f, 0.f};
        vec3 scale = { 1.f, 1.f, 1.f };
        quaternion rotation = { 0.f, 0.f, 0.f, 0.f, };
    };

    struct SubmeshDesc
    {
        AssetComponentId material_id = {};
        BufferView index_buffer_view = {};
        FixedArray<VertexAttribute, size_t(rhi::MeshAttribute::COUNT)> vertex_attributes = {};
    };

    /// <summary>
    /// Meshes are made up of multiple renderables
    /// We index directly into the renderables array with an array span
    /// </summary>
    struct MeshDesc
    {
        // TODO this can't be a type array view
        ArrayView submeshes;
        ArrayView AABBs;
    };

    struct MaterialDesc
    {
        enum MaterialSettings : u8
        {
            Default = 0,
            Emissive = (1 << 1),
            AmbientOcclusion = (1 << 2),
            // ??
        };

        enum struct BlendMode : u8
        {
            Opaque = 0,
            AlphaBlend,
            Cutoff,
        };

        struct TextureData
        {
            AssetComponentId texture{};
            uint32_t sampling_mode = 0;
        };

        vec4 albedo = { 1.f, 1.f, 1.f, 1.f };
        vec3 emissive = { 0.f };
        float metalness = 1.0f;
        float roughness = 1.0f;
        float alpha_cutoff = 0.5f;

        TextureData albedo_texture{};
        TextureData normal_texture{};
        TextureData metal_roughness_ao_texture{};
        TextureData emissive_texture{};

        BlendMode blend_mode = BlendMode::Opaque;
        MaterialSettings material_flags = MaterialSettings::Default;
    };

    struct MeshAsset
    {
        u32 version = 0;

        // Views into the components blob
        TypedArrayView<RenderNode> render_nodes = {};
        TypedArrayView<TransformNode> transform_nodes = {};
        TypedArrayView<MeshDesc> meshes = {};
        TypedArrayView<AABB> aabbs = {};
        TypedArrayView<SubmeshDesc> submeshes = {};
        TypedArrayView<MaterialDesc> materials = {};
        TypedArrayView<TextureDesc> textures = { };

        Allocation components_blob;
        Allocation index_buffers;
        Allocation vertex_buffers[size_t(rhi::MeshAttribute::COUNT)];
        Allocation texture_paths;
    };

    void release(IAllocator& allocator, AssetFile& asset);
    ZecResult save_binary_file(const char* path, const MeshAsset& asset);
    ZecResult load_binary_file(const char* path, IAllocator& allocator, MeshAsset& asset);
}
