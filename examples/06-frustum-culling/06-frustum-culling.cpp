#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"
#include "bounding_meshes.h"

using namespace zec;

constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;

namespace ResourceNames
{
    const std::string DEPTH_TARGET = "depth";
    const std::string SDR_BUFFER = "sdr";
}

struct DrawConstantData
{
    mat4 model = {};
    mat3 normal_transform = {};
};

struct ViewConstantData
{
    mat4 VP;
    mat4 invVP;
    vec3 camera_position;
    float padding[29];
};

static_assert(sizeof(ViewConstantData) == 256);

struct Scene
{
    Array<MeshHandle> meshes = {};
    Array<AABB> aabbs = {};
    Array<OBB> clip_space_obbs = {};
    Array<mat4> global_transforms = {};
    Array<BufferHandle> draw_data_buffers = {};
    Array<BufferHandle> debug_draw_data_buffers = {};
};

namespace DebugPass
{
    struct Context
    {
        Context() = default;

        // Not owned
        bool active = true;
        Camera const* camera = nullptr;
        Camera const* debug_camera = nullptr;
        Scene const* scene = nullptr;
        BufferHandle view_cb_handle = {};
        BufferHandle debug_view_cb_handle = {};

        // Owned
        MeshHandle debug_frustum_mesh = {};
        MeshHandle unit_cube = {};
        BufferHandle debug_frustum_cb_handle = {};
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };
    struct FrustumDrawData
    {
        mat4 inv_view;
        mat4 padding;
        vec4 color;
    };

    void setup(void* context)
    {
        Context* pass_context = reinterpret_cast<Context*>(context);

        pass_context->debug_frustum_cb_handle = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(FrustumDrawData),
            .stride = 0,
            });

        Array<vec3> frustum_positions;

        const Camera* camera = pass_context->camera;
        geometry::generate_frustum_data(frustum_positions, camera->near_plane, camera->far_plane, camera->vertical_fov, camera->aspect_ratio);

        // Each slice requires 6 indices
        MeshDesc mesh_desc{
            .index_buffer_desc = {
                .usage = RESOURCE_USAGE_INDEX,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(geometry::k_cube_indices),
                .stride = sizeof(geometry::k_cube_indices[0]),
                .data = (void*)(geometry::k_cube_indices),
             },
             .vertex_buffer_descs = {{
                .usage = RESOURCE_USAGE_VERTEX,
                .type = BufferType::DEFAULT,
                .byte_size = u32(sizeof(frustum_positions[0]) * frustum_positions.size),
                .stride = sizeof(frustum_positions[0]),
                .data = frustum_positions.data
             }}
        };
        pass_context->debug_frustum_mesh = gfx::meshes::create(mesh_desc);

        mesh_desc.vertex_buffer_descs[0].data = (void*)geometry::k_cube_positions;
        pass_context->unit_cube = gfx::meshes::create(mesh_desc);


        ResourceLayoutDesc layout_desc{
           .num_constants = 0,
           .constant_buffers = {
               { ShaderVisibility::ALL },
               { ShaderVisibility::ALL },
           },
           .num_constant_buffers = 2,
           .num_resource_tables = 0,
           .num_static_samplers = 0,
        };

        pass_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
        } };
        pipeline_desc.shader_file_path = L"shaders/06-frustum-culling/debug_shader.hlsl";
        pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
        pipeline_desc.resource_layout = pass_context->resource_layout;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        pipeline_desc.raster_state_desc.fill_mode = FillMode::WIREFRAME;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS_OR_EQUAL;
        pipeline_desc.depth_stencil_state.depth_write = false;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
        pipeline_desc.topology_type = TopologyType::TRIANGLE;
        pass_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::pipelines::set_debug_name(pass_context->pso_handle, L"Debug Pipeline");
    }

    void copy(void* context)
    {
        Context* pass_context = reinterpret_cast<Context*>(context);

        FrustumDrawData frustum_draw_data = {
            .inv_view = invert(pass_context->camera->view),
        };
        gfx::buffers::update(pass_context->debug_frustum_cb_handle, &frustum_draw_data, sizeof(frustum_draw_data));
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Context* pass_context = reinterpret_cast<Context*>(context);

        //TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle sdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);

        TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1, depth_target);

        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_frustum_cb_handle, 0);
        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_view_cb_handle, 1);

        gfx::cmd::draw_mesh(cmd_ctx, pass_context->debug_frustum_mesh);

        for (size_t i = 0; i < pass_context->scene->debug_draw_data_buffers.size; i++) {
            gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->scene->debug_draw_data_buffers[i], 0);
            gfx::cmd::draw_mesh(cmd_ctx, pass_context->unit_cube);
        }

    };

    void destroy(void* context)
    {

    }
}


namespace ForwardPass
{
    struct Context
    {
        // Not owned
        BufferHandle view_cb_handle = {};
        Scene const* scene;

        // Owned
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };

    void setup(void* context)
    {
        Context* forward_context = reinterpret_cast<Context*>(context);

        // What do we need to set up before we can do the forward pass?
        // Well for one, PSOs
        ResourceLayoutDesc layout_desc{
            .num_constants = 0,
            .constant_buffers = {
                { ShaderVisibility::ALL },
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 2,
            .tables = {
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
            },
            .num_resource_tables = 2,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::ANISOTROPIC,
                    .wrap_u = SamplerWrapMode::WRAP,
                    .wrap_v = SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
                {
                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                    .wrap_u = SamplerWrapMode::CLAMP,
                    .wrap_v = SamplerWrapMode::CLAMP,
                    .binding_slot = 1,
                },
            },
            .num_static_samplers = 2,
        };

        forward_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
            { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
            { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
        } };
        pipeline_desc.shader_file_path = L"shaders/06-frustum-culling/simple_diffuse.hlsl";
        pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = forward_context->resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
        pipeline_desc.depth_stencil_state.depth_write = TRUE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        forward_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::pipelines::set_debug_name(forward_context->pso_handle, L"Forward Rendering Pipeline");
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Context* forward_context = reinterpret_cast<Context*>(context);

        TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle render_target = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, forward_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, forward_context->pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);
        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);

        gfx::cmd::bind_constant_buffer(cmd_ctx, forward_context->view_cb_handle, 1);
        gfx::cmd::bind_resource_table(cmd_ctx, 2);
        gfx::cmd::bind_resource_table(cmd_ctx, 3);

        for (size_t i = 0; i < forward_context->scene->meshes.size; i++) {
            gfx::cmd::bind_constant_buffer(cmd_ctx, forward_context->scene->draw_data_buffers[i], 0);
            gfx::cmd::draw_mesh(cmd_ctx, forward_context->scene->meshes[i]);
        }
    };

    void destroy(void* context)
    {

    }
}

class FrustumCullingApp : public zec::App
{
public:
    FrustumCullingApp() : App{ L"View Frustum Culling" } { }
    float frame_times[120] = { 0.0f };

    Camera camera = {};
    FPSCameraController camera_controller = FPSCameraController{};

    Camera debug_camera = {};

    ViewConstantData view_constant_data = {};

    BufferHandle view_cb_handle = {};
    BufferHandle debug_view_cb_handle = {};

    MeshHandle fullscreen_mesh = {};
    MeshHandle cube_mesh = {};

    Scene scene = {};

    ForwardPass::Context forward_context = {};
    DebugPass::Context debug_context = {};

    RenderSystem::RenderList render_list;

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        float vertical_fov = deg_to_rad(65.0f);
        float camera_near = 0.1f;
        float camera_far = 1000.0f;

        camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far - 10.0f);
        debug_camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far);

        debug_camera.position = camera.position + vec3{ 0.0f, 5.0f, 0.0f };
        mat4 view = look_at(debug_camera.position, camera.position, { 0.0f, 0.0f, 1.0f });
        set_camera_view(debug_camera, view);

        gfx::begin_upload();

        constexpr float fullscreen_positions[] = {
             1.0f,  3.0f, 1.0f,
             1.0f, -1.0f, 1.0f,
            -3.0f, -1.0f, 1.0f,
        };

        constexpr float fullscreen_uvs[] = {
             1.0f,  -1.0f,
             1.0f,   1.0f,
            -1.0f,   1.0f,
        };

        constexpr u16 fullscreen_indices[] = { 0, 1, 2 };

        MeshDesc fullscreen_desc{};
        fullscreen_desc.index_buffer_desc.usage = RESOURCE_USAGE_INDEX;
        fullscreen_desc.index_buffer_desc.type = BufferType::DEFAULT;
        fullscreen_desc.index_buffer_desc.byte_size = sizeof(fullscreen_indices);
        fullscreen_desc.index_buffer_desc.stride = sizeof(fullscreen_indices[0]);
        fullscreen_desc.index_buffer_desc.data = (void*)fullscreen_indices;

        fullscreen_desc.vertex_buffer_descs[0] = {
            .usage = RESOURCE_USAGE_VERTEX,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(fullscreen_positions),
            .stride = 3 * sizeof(fullscreen_positions[0]),
            .data = (void*)(fullscreen_positions)
        };
        fullscreen_desc.vertex_buffer_descs[1] = {
           .usage = RESOURCE_USAGE_VERTEX,
           .type = BufferType::DEFAULT,
           .byte_size = sizeof(fullscreen_uvs),
           .stride = 2 * sizeof(fullscreen_uvs[0]),
           .data = (void*)(fullscreen_uvs)
        };

        fullscreen_mesh = gfx::meshes::create(fullscreen_desc);

        // Create the cube mesh
        {
            // Define the geometry for a cube.
            constexpr float cube_positions[] = {
                 0.5f,  0.5f,  0.5f, // +Y (Top face)
                -0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f,  0.5f, // +X (Right face)
                 0.5f,  0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f,  0.5f,
                -0.5f,  0.5f, -0.5f, // -X (Left face)
                -0.5f,  0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f, -0.5f,
                 0.5f,  0.5f, -0.5f, // -Z (Back face)
                -0.5f,  0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
                 0.5f, -0.5f, -0.5f,
                -0.5f,  0.5f,  0.5f, // +Z (Front face)
                 0.5f,  0.5f,  0.5f,
                 0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f, // -Y (Bottom face)
                 0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
            };

            constexpr float cube_normals[] = {
                 0.0f,  1.0f,  0.0f, // +Y (Top face)
                 0.0f,  1.0f,  0.0f,
                 0.0f,  1.0f,  0.0f,
                 0.0f,  1.0f,  0.0f,
                 1.0f,  0.0f,  0.0f, // +X (Right face)
1.0f, 0.0f, 0.0f,
1.0f, 0.0f, 0.0f,
1.0f, 0.0f, 0.0f,
-1.0f, 0.0f, 0.0f, // -X (Left face)
-1.0f, 0.0f, 0.0f,
-1.0f, 0.0f, 0.0f,
-1.0f, 0.0f, 0.0f,
0.0f, 0.0f, -1.0f, // -Z (Back face)
0.0f, 0.0f, -1.0f,
0.0f, 0.0f, -1.0f,
0.0f, 0.0f, -1.0f,
0.0f, 0.0f, 1.0f, // +Z (Front face)
0.0f, 0.0f, 1.0f,
0.0f, 0.0f, 1.0f,
0.0f, 0.0f, 1.0f,
0.0f, -1.0f, 0.0f, // -Y (Bottom face)
0.0f, -1.0f, 0.0f,
0.0f, -1.0f, 0.0f,
0.0f, -1.0f, 0.0f,
            };

            constexpr float cube_uvs[] = {
                0.0f, 0.0f, // +Y (Top face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f, // +X (Right face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f, // -Y (Left face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f, // -Z (Back face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f, // +Z (Front face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f, // -Y (Bottom face)
                1.0f, 0.0f,
                1.0f, 1.0f,
                0.0f, 1.0f,
            };

            constexpr uint16_t cube_indices[] = {
                 0,  2,  1, // Top
                 3,  2,  0,
                 4,  6,  5, // Right
                 7,  6,  4,
                 8, 10,  9, // Left
                11, 10,  8,
                12, 14, 13, // Back
                15, 14, 12,
                16, 18, 17, // Front
                19, 18, 16,
                20, 22, 21, // Bottom
                23, 22, 20,
            };

            MeshDesc mesh_desc{
                .index_buffer_desc = {
                    .usage = RESOURCE_USAGE_INDEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(cube_indices),
                    .stride = sizeof(cube_indices[0]),
                    .data = (void*)cube_indices,
                 },
                .vertex_buffer_descs = {
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(cube_positions),
                    .stride = 3 * sizeof(cube_positions[0]),
                    .data = (void*)(cube_positions)
                    },
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(cube_normals),
                        .stride = 3 * sizeof(cube_normals[0]),
                        .data = (void*)(cube_normals)
                    },
                    {
                       .usage = RESOURCE_USAGE_VERTEX,
                       .type = BufferType::DEFAULT,
                       .byte_size = sizeof(cube_uvs),
                       .stride = 2 * sizeof(cube_uvs[0]),
                       .data = (void*)(cube_uvs)
                    }
                }
            };

            cube_mesh = gfx::meshes::create(mesh_desc);
        }

        AABB aabb;
        mat4 local_transform;
        MeshHandle mesh_handle{};
        {

            gltf::Context gltf_scene{};
            gltf::load_gltf_file("../../models/boom_box/BoomBox.gltf", gltf_scene);
            mesh_handle = gltf_scene.meshes[0];
            local_transform = gltf_scene.scene_graph.global_transforms[0];
            aabb.max = gltf_scene.aabbs[0].max;
            aabb.min = gltf_scene.aabbs[0].min;
        }
        mat4 scale_transform = identity_mat4();
        set_scale(scale_transform, 1.0f / (aabb.max - aabb.min));
        local_transform = scale_transform * local_transform;

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            .data = nullptr,
        };

        view_cb_handle = gfx::buffers::create(cb_desc);
        debug_view_cb_handle = gfx::buffers::create(cb_desc);

        {
            constexpr uvec3 grid_size = { 20, 20, 25 };
            const u32 num_objects = grid_size.x * grid_size.y * grid_size.z;
            const u32 z_layer_area = grid_size.x * grid_size.y;
            //const mat3 rotation_mat = identity_mat3();


            vec3 grid_extent = 2.0f * vec3{ float(grid_size.x), float(grid_size.x), float(grid_size.x) };

            scene.meshes.grow(num_objects);
            scene.global_transforms.grow(num_objects);
            scene.aabbs.grow(num_objects);
            scene.draw_data_buffers.grow(num_objects);
            scene.debug_draw_data_buffers.grow(num_objects);

            BufferDesc draw_cb_desc = cb_desc;
            for (size_t k = 0; k < grid_size.z; k++) {
                for (size_t j = 0; j < grid_size.y; j++) {
                    for (size_t i = 0; i < grid_size.x; i++) {
                        const u32 idx = (i + j * grid_size.x + k * z_layer_area);
                        scene.meshes[idx] = mesh_handle;
                        const vec3 position = 4.0f * (vec3{ float(i), float(j), float(k) } + 0.5f) - grid_extent;
                        scene.global_transforms[idx] = local_transform;
                        set_translation(scene.global_transforms[idx], position);
                        scene.aabbs[idx] = aabb;

                        DrawConstantData draw_data = {
                            .model = scene.global_transforms[idx],
                            .normal_transform = transpose(to_mat3(scene.global_transforms[idx])),
                        };
                        draw_cb_desc.data = &draw_data;
                        scene.draw_data_buffers[idx] = gfx::buffers::create(draw_cb_desc);

                        mat4 debug_transform = identity_mat4();
                        set_scale(debug_transform, (aabb.max - aabb.min));
                        set_translation(debug_transform, 0.5f * (aabb.max + aabb.min));
                        debug_transform = scene.global_transforms[idx] * debug_transform;
                        draw_cb_desc.data = &debug_transform;
                        scene.debug_draw_data_buffers[idx] = gfx::buffers::create(draw_cb_desc);
                    }
                }
            }
        }

        forward_context = {
            .view_cb_handle = view_cb_handle,
            .scene = &scene,
        };

        debug_context.active = true;
        debug_context.camera = &camera;
        debug_context.view_cb_handle = view_cb_handle;
        debug_context.debug_view_cb_handle = view_cb_handle;
        debug_context.debug_camera = &debug_camera;
        debug_context.scene = &scene;

        RenderSystem::RenderPassDesc render_pass_descs[] = {
            {
                .name = "Forward Render Pass",
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {},
                .outputs = {
                    {
                        .name = ResourceNames::SDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_RENDER_TARGET,
                        .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}
                    },
                    {
                        .name = ResourceNames::DEPTH_TARGET,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                        .texture_desc = {.format = BufferFormat::D32 }
                    }
                },
                .context = &forward_context,
                .setup = &ForwardPass::setup,
                .execute = &ForwardPass::record,
                .destroy = &ForwardPass::destroy
            },
            {
                .name = "Debug Pass",
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {
                    {
                        .name = ResourceNames::DEPTH_TARGET,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                    }
                },
                .outputs = {
                    {
                        .name = ResourceNames::SDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_RENDER_TARGET,
                        .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}
                    }
                },
                .context = &debug_context,
                .setup = &DebugPass::setup,
                .execute = &DebugPass::record,
                .destroy = &DebugPass::destroy
            },
        };
        RenderSystem::RenderListDesc render_list_desc = {
            .render_pass_descs = render_pass_descs,
            .num_render_passes = ARRAY_SIZE(render_pass_descs),
            .resource_to_use_as_backbuffer = ResourceNames::SDR_BUFFER,
        };

        RenderSystem::compile_render_list(render_list, render_list_desc);
        RenderSystem::setup(render_list);

        CmdReceipt receipt = gfx::end_upload();
        gfx::cmd::cpu_wait(receipt);

    }

    void shutdown() override final
    {
        RenderSystem::destroy(render_list);
    }

    void update(const zec::TimeData& time_data) override final
    {
        frame_times[gfx::get_current_cpu_frame() % 120] = time_data.delta_milliseconds_f;

        camera_controller.update(time_data.delta_seconds_f);

        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.invVP = invert(camera.projection * mat4(to_mat3(camera.view), {}));
        view_constant_data.camera_position = camera.position;
    }

    void copy() override final
    {
        ViewConstantData debug_view_data{
            .VP = debug_camera.projection * debug_camera.view ,
            .invVP = invert(debug_camera.view * mat4(to_mat3(debug_camera.view), {})),
            .camera_position = debug_camera.position,
        };
        gfx::buffers::update(debug_view_cb_handle, &debug_view_data, sizeof(debug_view_data));

        gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
        DebugPass::copy(&debug_context);
    }

    void render() override final
    {
        RenderSystem::execute(render_list);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    FrustumCullingApp  app{};
    return app.run();
}