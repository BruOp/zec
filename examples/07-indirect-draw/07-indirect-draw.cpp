#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"

#include "bounding_meshes.h"

using namespace zec;

static constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;
static constexpr size_t MAX_NUM_OBJECTS = 16384;

namespace ResourceNames
{
    constexpr char DEPTH_TARGET[] = "depth";
    constexpr char HDR_BUFFER[] = "hdr";
    constexpr char SDR_BUFFER[] = "sdr";
} // namespace ResourceNames

struct ViewConstantData
{
    mat4 VP;
    mat4 invVP;
    vec3 camera_position;
    float padding;
};

struct VertexShaderData
{
    mat4 model = {};
    u32 vertex_positions_idx = UINT32_MAX;
    u32 vertex_uvs_idx = UINT32_MAX;
    float padding[34 + 12];
};

static_assert(sizeof(VertexShaderData) == 256);

struct PerEntityDataBuffers
{
    static constexpr size_t MATERIAL_DATA_BYTE_SIZE = 256;
    static constexpr size_t INSTANCE_BUFFER_DATA_SIZE = MAX_NUM_OBJECTS * MATERIAL_DATA_BYTE_SIZE; // Since each object gets 256 bytes worth of material data
    BufferHandle vs_buffer = {};
    BufferHandle material_data_buffer = {};
};

void create_per_entity_data_buffers(PerEntityDataBuffers &per_entity_data_buffers)
{
    ASSERT(!is_valid(per_entity_data_buffers.vs_buffer));
    ASSERT(!is_valid(per_entity_data_buffers.material_data_buffer));

    BufferDesc vs_buffer_desc{
        .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
        .type = BufferType::DEFAULT,
        .byte_size = sizeof(VertexShaderData) * MAX_NUM_OBJECTS,
        .stride = sizeof(VertexShaderData),
    };
    per_entity_data_buffers.vs_buffer = gfx::buffers::create(vs_buffer_desc);

    BufferDesc material_data_buffer_desc{
        .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
        .type = BufferType::DEFAULT,
        .byte_size = PerEntityDataBuffers::INSTANCE_BUFFER_DATA_SIZE,
        .stride = PerEntityDataBuffers::MATERIAL_DATA_BYTE_SIZE,
    };
    per_entity_data_buffers.material_data_buffer = gfx::buffers::create(material_data_buffer_desc);
}

class SceneRenderData
{
public:
    size_t num_entities;
    Array<BufferHandle> index_buffers;
    Array<VertexShaderData> vertex_shader_data;
    PerEntityDataBuffers per_entity_data_buffers;
};

namespace ForwardPass
{
    struct Settings
    {
        SceneRenderData *scene_render_data = nullptr;
        BufferHandle view_cb_handle = {};
        // Internal
        PipelineStateHandle pso = {};
        ResourceLayoutHandle resource_layout = {};
    };

    void setup(void *context)
    {
        Settings *settings = reinterpret_cast<Settings *>(context);
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 1},
                {.visibility = ShaderVisibility::VERTEX, .num_constants = 1},
            },
            .num_constants = 2,
            .constant_buffers = {
                {ShaderVisibility::ALL},
            },
            .num_constant_buffers = 1,
            .tables = {
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE},
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE},
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE},
            },
            .num_resource_tables = 3,
        };

        settings->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pipeline_desc.input_assembly_desc = {};
        pipeline_desc.shader_file_path = L"shaders/zec_forward/mesh_forward.hlsl";
        pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = settings->resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
        pipeline_desc.depth_stencil_state.depth_write = TRUE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        settings->pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::pipelines::set_debug_name(settings->pso, L"Forward Pipeline");
    };

    void copy(void *context){
        // Shrug
    };

    void record(RenderSystem::RenderList &render_list, CommandContextHandle cmd_ctx, void *context)
    {
        constexpr float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        Settings *pass_context = reinterpret_cast<Settings *>(context);

        TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle render_target = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureInfo &texture_info = gfx::textures::get_texture_info(render_target);
        Viewport viewport = {0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height)};
        Scissor scissor{0, 0, texture_info.width, texture_info.height};

        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);
        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);

        const BufferHandle view_cb_handle = pass_context->view_cb_handle;
        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->view_cb_handle, 2);
        gfx::cmd::bind_resource_table(cmd_ctx, 3);
        gfx::cmd::bind_resource_table(cmd_ctx, 4);
        gfx::cmd::bind_resource_table(cmd_ctx, 5);

        const auto *scene_data = pass_context->scene_render_data;
        u32 vs_cb_idx = gfx::buffers::get_shader_readable_index(scene_data->per_entity_data_buffers.vs_buffer);
        gfx::cmd::bind_constant(cmd_ctx, &vs_cb_idx, 1, 1);

        for (u32 i = 0; i < scene_data->num_entities; i++)
        {
            gfx::cmd::bind_constant(cmd_ctx, &i, 1, 0);

            gfx::cmd::draw_mesh(cmd_ctx, scene_data->index_buffers[i]);
        }
    };

    void destroy(void *context){};

    RenderSystem::RenderPassDesc generate_desc(Settings *settings)
    {
        return {
            .name = "ForwardPass",
            .inputs = {},
            .outputs = {
                {.name = ResourceNames::SDR_BUFFER,
                 .type = RenderSystem::PassResourceType::TEXTURE,
                 .usage = RESOURCE_USAGE_RENDER_TARGET,
                 .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}},
                {.name = ResourceNames::DEPTH_TARGET,
                 .type = RenderSystem::PassResourceType::TEXTURE,
                 .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                 .texture_desc = {.format = BufferFormat::D32}}},
            .context = settings,
            .setup = ForwardPass::setup,
            .copy = ForwardPass::copy,
            .execute = ForwardPass::record,
            .destroy = ForwardPass::destroy,
        };
    }
} // namespace ForwardPass

class IndirectDrawExample : public zec::App
{
public:
    IndirectDrawExample() : App{L"Indirect Draw Example"} {}
    float frame_times[120] = {0.0f};

    FPSCameraController camera_controller = {};

    MeshHandle fullscreen_mesh;

    PerspectiveCamera camera;
    PerspectiveCamera debug_camera;

    SceneRenderData scene_render_data = {};

    ForwardPass::Settings forward_pass_context = {};
    RenderSystem::RenderList render_list;

    // Are handles necessary for something like this?
    size_t active_camera_idx;

    BufferHandle view_cb_handle = {};
    BufferHandle scene_constants_buffer = {};
    BufferHandle vs_constants_buffers = {};

protected:
    void init() override final
    {
        float vertical_fov = deg_to_rad(65.0f);
        float camera_near = 0.1f;
        float camera_far = 100.0f;

        camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far);
        camera.position = vec3{0.0f, 0.0f, -2.0f};

        camera_controller.init();
        camera_controller.set_camera(&camera);

        gfx::begin_upload();

        constexpr float fullscreen_positions[] = {
            1.0f,
            3.0f,
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
            -3.0f,
            -1.0f,
            1.0f,
        };

        constexpr float fullscreen_uvs[] = {
            1.0f,
            -1.0f,
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
        };

        constexpr u16 fullscreen_indices[] = {0, 1, 2};

        MeshDesc fullscreen_desc{};
        fullscreen_desc.index_buffer_desc.usage = RESOURCE_USAGE_INDEX;
        fullscreen_desc.index_buffer_desc.type = BufferType::DEFAULT;
        fullscreen_desc.index_buffer_desc.byte_size = sizeof(fullscreen_indices);
        fullscreen_desc.index_buffer_desc.stride = sizeof(fullscreen_indices[0]);
        fullscreen_desc.index_buffer_desc.data = (void *)fullscreen_indices;

        view_cb_handle = gfx::buffers::create({.usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                                               .type = BufferType::DEFAULT,
                                               .byte_size = sizeof(ViewConstantData),
                                               .stride = 0,
                                               .data = nullptr});

        fullscreen_desc.vertex_buffer_descs[0] = {
            .usage = RESOURCE_USAGE_VERTEX,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(fullscreen_positions),
            .stride = 3 * sizeof(fullscreen_positions[0]),
            .data = (void *)(fullscreen_positions)};
        fullscreen_desc.vertex_buffer_descs[1] = {
            .usage = RESOURCE_USAGE_VERTEX,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(fullscreen_uvs),
            .stride = 2 * sizeof(fullscreen_uvs[0]),
            .data = (void *)(fullscreen_uvs)};

        fullscreen_mesh = gfx::meshes::create(fullscreen_desc);

        BufferHandle cube_index_buffer = gfx::buffers::create({.usage = RESOURCE_USAGE_INDEX,
                                                               .type = BufferType::DEFAULT,
                                                               .byte_size = sizeof(geometry::k_cube_indices),
                                                               .stride = sizeof(geometry::k_cube_indices[0]),
                                                               .data = geometry::k_cube_indices});
        BufferHandle cube_vertex_positions_buffer = gfx::buffers::create({.usage = RESOURCE_USAGE_SHADER_READABLE,
                                                                          .type = BufferType::STRUCTURED,
                                                                          .byte_size = sizeof(geometry::k_cube_positions),
                                                                          .stride = 3 * sizeof(geometry::k_cube_positions[0]),
                                                                          .data = geometry::k_cube_positions});
        BufferHandle cube_vertex_uvs_buffer = gfx::buffers::create({.usage = RESOURCE_USAGE_SHADER_READABLE,
                                                                    .type = BufferType::STRUCTURED,
                                                                    .byte_size = sizeof(geometry::k_cube_uvs),
                                                                    .stride = 2 * sizeof(geometry::k_cube_uvs[0]),
                                                                    .data = geometry::k_cube_uvs});

        create_per_entity_data_buffers(scene_render_data.per_entity_data_buffers);

        constexpr size_t num_objects = 10;
        scene_render_data.num_entities = num_objects;
        // Need to assign this data to my object somehow
        for (size_t i = 0; i < num_objects; i++)
        {
            scene_render_data.index_buffers.push_back(cube_index_buffer);
            vec3 position{float(i - num_objects / 2), 0.0f, 0.0f};
            mat4 transform = identity_mat4();
            set_translation(transform, position);

            scene_render_data.vertex_shader_data.push_back({
                // TODO: create model matrix
                .model = transform,
                // TODO: create normal matrix
                .vertex_positions_idx = gfx::buffers::get_shader_readable_index(cube_vertex_positions_buffer),
                .vertex_uvs_idx = gfx::buffers::get_shader_readable_index(cube_vertex_uvs_buffer),
            });
        }

        forward_pass_context.view_cb_handle = view_cb_handle;
        forward_pass_context.scene_render_data = &scene_render_data;

        RenderSystem::RenderPassDesc render_pass_descs[] = {
            ForwardPass::generate_desc(&forward_pass_context),
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

    void update(const zec::TimeData &time_data) override final
    {
        camera_controller.update(time_data.delta_seconds_f);
    }

    void copy() override final
    {
        ViewConstantData view_constant_data{
            .VP = camera.projection * camera.view,
            .invVP = invert(camera.projection * mat4(to_mat3(camera.view), {})),
            .camera_position = camera.position,
        };
        gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));

        gfx::buffers::update(
            scene_render_data.per_entity_data_buffers.vs_buffer,
            scene_render_data.vertex_shader_data.data,
            scene_render_data.vertex_shader_data.size * sizeof(VertexShaderData));

        RenderSystem::copy(render_list);
    }

    void render() override final
    {
        RenderSystem::execute(render_list);
    }

    void before_reset() override final
    {
    }

    void after_reset() override final
    {
    }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    int res = 0;

    IndirectDrawExample app{};
    res = app.run();

    return res;
}
