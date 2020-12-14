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
    const std::string HDR_BUFFER = "hdr";
    const std::string SDR_BUFFER = "sdr";
}

struct ViewConstantData
{
    mat4 VP;
    mat4 invVP;
    vec3 camera_position;
    u32 radiance_map_idx = UINT32_MAX;
    u32 irradiance_map_idx = UINT32_MAX;
    u32 brdf_LUT = UINT32_MAX;
    float num_env_levels = 0.0;
    float time;
    float padding[24];
};

static_assert(sizeof(ViewConstantData) == 256);

struct ClusterSetup
{
    // The number of bins in each axis
    u32 width;
    u32 height;
    u32 depth;
};

namespace DebugPass
{
    struct Context
    {
        Context() = default;

        // Not owned
        bool active = true;
        Camera* camera = nullptr;
        Camera* debug_camera = nullptr;
        ClusterSetup cluster_setup = {};
        BufferHandle view_cb_handle = {};

        // Owned
        MeshHandle debug_frustum_mesh = {};
        BufferHandle debug_frustum_cb_handle = {};
        BufferHandle debug_view_cb_handle = {};
        Array<MeshHandle>* debug_meshes = nullptr;
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };
    struct FrustumDrawData
    {
        mat4 inv_view;
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

        pass_context->debug_view_cb_handle = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            });

        const ClusterSetup& cl_setup = pass_context->cluster_setup;
        // Each dimension is sliced width/height/depth times, plus 1 for the final boundary we want to represent
        // Each slice gets four positions
        u32 num_slices = (cl_setup.width + cl_setup.height + cl_setup.depth + 3);
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
        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        pipeline_desc.raster_state_desc.fill_mode = FillMode::WIREFRAME;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::OFF;
        pipeline_desc.depth_stencil_state.depth_write = false;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
        pipeline_desc.topology_type = TopologyType::TRIANGLE;
        pass_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::pipelines::set_debug_name(pass_context->pso_handle, L"Debug Pipeline");
    }

    void copy(void* context)
    {
        Context* pass_context = reinterpret_cast<Context*>(context);
        ViewConstantData debug_view_data{
            .VP = pass_context->debug_camera->projection * pass_context->debug_camera->view ,
            .invVP = invert(pass_context->debug_camera->view * mat4(to_mat3(pass_context->debug_camera->view), {})),
            .camera_position = pass_context->debug_camera->position,
            .time = 1.0f,
        };
        gfx::buffers::update(pass_context->debug_view_cb_handle, &debug_view_data, sizeof(debug_view_data));

        FrustumDrawData frustum_draw_data = {
            .inv_view = pass_context->camera->invView,
            .color = vec4(0.5f, 0.5f, 1.0f, 1.0f),
        };
        gfx::buffers::update(pass_context->debug_frustum_cb_handle, &frustum_draw_data, sizeof(frustum_draw_data));
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Context* pass_context = reinterpret_cast<Context*>(context);

        //TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle sdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, sdr_buffer, clear_color);
        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_frustum_cb_handle, 0);
        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_view_cb_handle, 1);

        gfx::cmd::draw_mesh(cmd_ctx, pass_context->debug_frustum_mesh);
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
    OrbitCameraController camera_controller = OrbitCameraController{};

    Camera debug_camera = {};

    ViewConstantData view_constant_data = {};

    BufferHandle view_cb_handle = {};
    MeshHandle fullscreen_mesh;

    DebugPass::Context debug_context = {};

    RenderSystem::RenderList render_list;

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 7.0f;

        float vertical_fov = deg_to_rad(65.0f);
        float camera_near = 0.1f;
        float camera_far = 100.0f;

        camera = create_camera(float(width) / float(height), vertical_fov, camera_near, 10.0f);
        debug_camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far);

        debug_camera.position = { 0.0f, 0.0f, 10.0f };
        mat4 view = look_at(debug_camera.position, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
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

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            .data = nullptr,
        };

        view_cb_handle = gfx::buffers::create(cb_desc);

        ClusterSetup cluster_setup{
            .width = 16, .height = 9, .depth = 24
        };

        debug_context.active = true;
        debug_context.camera = &camera;
        debug_context.cluster_setup = cluster_setup;
        debug_context.view_cb_handle = view_cb_handle;
        debug_context.debug_camera = &debug_camera;

        RenderSystem::RenderPassDesc render_pass_descs[] = {
            {
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {},
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
        view_constant_data.time = time_data.elapsed_seconds_f;
    }

    void copy() override final
    {
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