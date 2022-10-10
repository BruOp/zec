#include <stdint.h>

#include <core/array.h>
#include <core/tracked_allocator.h>
#include <core/zec_math.h>
#include <core/zec_types.h>
#include <gfx/public_resources.h>

namespace zec::assets
{
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
        u32 num_componen_descts;
        Allocation components_blob;
        Allocation binary_blob;
    };

    RESOURCE_HANDLE(AssetComponentId);

    enum struct AssetComponentType : u16
    {
        TransformNode = 0,
        Mesh,
        Material,
        Texture,
        Sampler,
        VertexAttribute,
        BufferView,

        Count,
        Invalid,
    };

    struct AssetComponentDesc
    {
        AssetComponentType type = AssetComponentType::Invalid;
        u32 num_components = 0;
        size_t offset = 0;
    };

    struct BufferView
    {
        size_t offset;
        size_t length;
    };

    struct MeshAttributeComponent
    {
        MeshAttribute attribute = MeshAttribute::MESH_ATTRIBUTE_INVALID;
        BufferFormat buffer_format = {};
        AssetComponentId buffer_view_id = {};
    };

    struct TextureDesc
    {
        u32 width = 0;
        u32 height = 0;
        u32 depth = 0;
        u32 num_mips = 0;
        // For cube maps, array_size must == 6
        u32 array_size = 0;
        u16 is_cubemap = 0;
        u16 is_3d = 0;
        BufferFormat format;
        u16 usage = 0;
        AssetComponentId buffer_view_id;
    };

    struct TransformNode
    {
        AssetComponentId parent_transform = {};
        vec3 translation = { 0.f, 0.f, 0.f};
        vec3 scale = { 1.f, 1.f, 1.f };
        quaternion rotation = { };
    };

    struct RenderableDesc
    {
        constexpr static u8 max_num_attributes = 6u;

        AssetComponentId transform_id = {};
        AssetComponentId material_id = {};
        AssetComponentId index_buffer_ids = {};
        AssetComponentId vertex_buffer_ids[max_num_attributes] = {};
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

        struct UniformData
        {
            vec3 albedo = { 1.f, 1.f, 1.f };
            float metalness = 1.0f;
            vec3 emissive = { 0.f };
            float roughness = 1.0f;
            float alpha_cutoff = 0.f;
        };

        struct TextureData
        {
            AssetComponentId texture{};
            uint32_t sampling_mode = 0;
        };

        UniformData pbr_data = {};
        
        TextureData albedo_texture{};
        TextureData normal_texture{};
        TextureData metal_roughness_ao_texture{};
        TextureData emissive_texture{};
        
        BlendMode blend_mode = BlendMode::Opaque;
        MaterialSettings material_flags = MaterialSettings::Default;
    };

    struct ModelAsset
    {
        u32 version = 0;
        u32 num_component_descs = 0;

        ArraySpan<AssetComponentDesc> asset_component_descs = { };
        ArraySpan<BufferView> buffer_views = {};
        ArraySpan<MeshAttributeComponent> mesh_attribute_components = { };
        ArraySpan<TextureDesc> textures = { };
        ArraySpan<TransformNode> transform_nodes = {};
        ArraySpan<RenderableDesc> meshes = {};
        ArraySpan<MaterialDesc> materials = {};

        Allocation components_blob;
        Allocation binary_blob;
    };

    void release(IAllocator& allocator, AssetFile& asset);
    ZecResult save_binary_file(const char* path, const AssetFile& asset);
    ZecResult load_binary_file(const char* path, IAllocator& allocator, AssetFile& asset);
}
