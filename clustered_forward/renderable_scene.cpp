#include "renderable_scene.h"

using namespace zec;

namespace clustered
{
    void Renderables::init(const u32 in_num_entities, const u32 in_num_materials)
    {
        max_num_entities = in_num_entities;
        max_num_materials = in_num_materials;
        materials_buffer = gfx::buffers::create({
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::RAW,
            .byte_size = sizeof(MaterialData) * u32(max_num_materials),
            .stride = 4,
            });
        gfx::set_debug_name(materials_buffer, L"Materials Buffer");

        material_buffer_idx = gfx::buffers::get_shader_readable_index(materials_buffer);

        vs_buffer = gfx::buffers::create({
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::RAW,
            .byte_size = sizeof(VertexShaderData) * u32(max_num_entities),
            .stride = 4,
            });
        gfx::set_debug_name(vs_buffer, L"Vertex Shader Constants Buffer");

        vs_buffer_idx = gfx::buffers::get_shader_readable_index(vs_buffer);
    }

    void Renderables::copy()
    {
        gfx::buffers::update(
            vs_buffer,
            vertex_shader_data.data,
            vertex_shader_data.size * sizeof(VertexShaderData));

        gfx::buffers::update(
            materials_buffer,
            material_instances.data,
            material_instances.size * sizeof(MaterialData));
    }

    void Renderables::push_renderable(const u32 material_idx, const MeshHandle mesh_handle, const VertexShaderData& vs_data)
    {
        ASSERT(num_entities < max_num_entities);
        num_entities++;
        material_indices.push_back(material_idx);
        meshes.push_back(mesh_handle);
        vertex_shader_data.push_back(vs_data);
        ASSERT(meshes.size == vertex_shader_data.size && meshes.size == num_entities);
    }

    void RenderableScene::initialize(const RenderableSceneSettings& settings)
    {
        this->settings = settings;
        renderables.init(settings.max_num_entities, settings.max_num_materials);
        // Initialize our buffers
        scene_constants = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(SceneConstantData), // will get bumped up to 256
            .stride = 0,
            });
        gfx::set_debug_name(scene_constants, L"Scene Constants Buffers");

        spot_lights_buffer = gfx::buffers::create({
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::RAW,
            .byte_size = sizeof(SpotLight) * u32(settings.max_num_spot_lights),
            .stride = 4,
            });
        gfx::set_debug_name(spot_lights_buffer, L"Spot Lights Buffer");

        point_lights_buffer = gfx::buffers::create({
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::RAW,
            .byte_size = sizeof(PointLight) * u32(settings.max_num_point_lights),
            .stride = 4,
            });
        gfx::set_debug_name(point_lights_buffer, L"Point Lights Buffer");
    }

    void RenderableScene::copy(/* const Scene& scene, eventually, */ const Array<SpotLight>& spot_lights, const Array<PointLight>& point_lights)
    {
        scene_constant_data.num_spot_lights = u32(spot_lights.size);
        scene_constant_data.num_point_lights = u32(point_lights.size);
        scene_constant_data.spot_light_buffer_idx = gfx::buffers::get_shader_readable_index(spot_lights_buffer);
        scene_constant_data.point_light_buffer_idx = gfx::buffers::get_shader_readable_index(point_lights_buffer);

        gfx::buffers::update(
            scene_constants,
            &scene_constant_data,
            sizeof(scene_constant_data));

        gfx::buffers::update(
            spot_lights_buffer,
            spot_lights.data,
            spot_lights.size * sizeof(SpotLight));

        gfx::buffers::update(
            point_lights_buffer,
            point_lights.data,
            point_lights.size * sizeof(PointLight));

        renderables.copy();
    }

    void RenderableCamera::initialize(const wchar* name, PerspectiveCamera* in_camera)
    {
        camera = in_camera;
        view_constant_buffer = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0, });
        gfx::set_debug_name(view_constant_buffer, name);
    }

    void RenderableCamera::copy()
    {
        ViewConstantData view_constant_data{
            .VP = camera->projection * camera->view,
            .invVP = invert(camera->projection * mat4(to_mat3(camera->view), {})),
            .view = camera->view,
            .camera_position = camera->position,
        };
        gfx::buffers::update(view_constant_buffer, &view_constant_data, sizeof(view_constant_data));
    }
}
