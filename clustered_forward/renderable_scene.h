#pragma once
#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"
#include "gfx/gfx.h"

namespace clustered
{
    struct SpotLight
    {
        zec::vec3 position;
        float radius;
        zec::vec3 direction;
        float umbra_angle;
        float penumbra_angle;
        zec::vec3 color;
    };

    struct SceneConstantData
    {
        float time = 0.0f;
        u32 radiance_map_idx = UINT32_MAX;
        u32 irradiance_map_idx = UINT32_MAX;
        u32 brdf_lut_idx = UINT32_MAX;
        u32 num_spot_lights = 0;
        u32 spot_light_buffer_idx = UINT32_MAX;
    };

    struct MaterialData
    {
        zec::vec4 base_color_factor;
        zec::vec3 emissive_factor;
        float metallic_factor;
        float roughness_factor;
        u32 base_color_texture_idx;
        u32 metallic_roughness_texture_idx;
        u32 normal_texture_idx;
        u32 occlusion_texture_idx;
        u32 emissive_texture_idx;
        float padding[18];
    };

    struct ViewConstantData
    {
        zec::mat4 VP;
        zec::mat4 invVP;
        zec::mat4 view;
        zec::vec3 camera_position;
        float padding[13];
    };

    static_assert(sizeof(ViewConstantData) == 256);

    struct VertexShaderData
    {
        zec::mat4 model_transform = {};
        zec::mat3 normal_transform = {};
    };

    static_assert(sizeof(VertexShaderData) % 16 == 0);

    static_assert(sizeof(MaterialData) == 128);

    class Renderables
    {
    public:

        // Owning
        size_t max_num_entities;
        size_t num_entities;

        zec::Array<zec::MeshHandle> meshes;
        zec::Array<MaterialData> material_instances = {};
        zec::Array<VertexShaderData> vertex_shader_data;

        size_t max_num_materials;
        zec::Array<u32> material_indices;

        u32 material_buffer_idx = {};
        u32 vs_buffer_idx = {};

        zec::BufferHandle materials_buffer = {};
        zec::BufferHandle vs_buffer = {};

        void init(const u32 max_num_entities, const u32 max_num_materials);
        void copy();

        void push_material(const MaterialData& material)
        {
            material_instances.push_back(material);
        }

        void push_renderable(const u32 material_idx, const zec::MeshHandle mesh_handle, const VertexShaderData& vs_data);

    };

    struct RenderableSceneSettings
    {
        u32 max_num_entities = 0;
        u32 max_num_materials = 0;
        u32 max_num_spot_lights = 0;
    };

    struct RenderableScene
    {
        RenderableSceneSettings settings;
        Renderables renderables = {};
        SceneConstantData scene_constant_data = {};

        zec::BufferHandle scene_constants = {};
        zec::BufferHandle spot_lights_buffer = {};
        // GPU side copy of the material instances data

        void initialize(const RenderableSceneSettings& settings);

        void copy(/* const Scene& scene, eventually, */const zec::Array<SpotLight>& spot_lights);
    };

    struct RenderableCamera
    {
        zec::PerspectiveCamera* camera = nullptr;
        ViewConstantData view_constant_data = {};
        zec::BufferHandle view_constant_buffer = {};

        void initialize(const wchar* name, zec::PerspectiveCamera* in_camera);

        void copy();
    };
}
