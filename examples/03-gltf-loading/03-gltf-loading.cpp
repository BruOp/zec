#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"

using namespace zec;

struct ViewConstantData
{
    mat4 view;
    mat4 projection;
    mat4 VP;
    vec3 camera_position;
    float time;
    float padding[12];
};

static_assert(sizeof(ViewConstantData) == 256);

struct DrawConstantData
{
    mat4 model = {};
    mat3 normal_transform = {};
    gltf::MaterialData material_data = {};
    float padding[22] = {};
};

static_assert(sizeof(DrawConstantData) == 256);


class GltfLoadingApp : public zec::App
{
public:
    GltfLoadingApp() : App{ L"Basic GLTF Loading and Rendering" } { }

    float frame_times[120] = { 0.0f };
    float clear_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    PerspectiveCamera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{};

    ViewConstantData view_constant_data = {};
    Array<DrawConstantData> draw_constant_data;
    gltf::Context gltf_context;

    BufferHandle view_cb_handle = {};
    Array<BufferHandle> draw_data_buffer_handles = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};

    // TODO: Shouldn't there be two of these?
    TextureHandle depth_target = {};

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 7.0f;

        camera.projection = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            constexpr u32 num_textures = 1024;
            ResourceLayoutDesc layout_desc{
                .num_constants = 0,
                .constant_buffers = {
                    { ShaderVisibility::ALL },
                    { ShaderVisibility::ALL },
                },
                .num_constant_buffers = 2,
                .tables = {
                    {.usage = ResourceAccess::READ, .count = 4096 },
                },
                .num_resource_tables = 1,
                .static_samplers = {
                    {
                        .filtering = SamplerFilterType::ANISOTROPIC,
                        .wrap_u = SamplerWrapMode::WRAP,
                        .wrap_v = SamplerWrapMode::WRAP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };

            resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
        }

        // Create the Pipeline State Object
        {
            PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
            } };
            pipeline_desc.shader_file_path = L"shaders/gltf_shader.hlsl";
            pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.depth_buffer_format = BufferFormat::D32;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
            pipeline_desc.depth_stencil_state.depth_write = TRUE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        }

        CommandContextHandle upload_cmd_ctx = gfx::cmd::provision(CommandQueueType::COPY);
        //gltf::load_gltf_file("models/damaged_helmet/DamagedHelmet.gltf", gltf_context);
        gltf::load_gltf_file("models/flight_helmet/FlightHelmet.gltf", upload_cmd_ctx, gltf_context);
        CmdReceipt upload_receipt = gfx::cmd::return_and_execute(&upload_cmd_ctx, 1);

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
        };

        view_cb_handle = gfx::buffers::create(cb_desc);

        const size_t num_draws = gltf_context.draw_calls.size;
        draw_constant_data.reserve(num_draws);
        draw_data_buffer_handles.reserve(num_draws);
        for (size_t i = 0; i < num_draws; i++) {
            const auto& draw_call = gltf_context.draw_calls[i];
            const auto& transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx];
            const auto& normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx];

            const u32 data_idx = draw_constant_data.push_back({
                .model = transform,
                .normal_transform = normal_transform,
                .material_data = gltf_context.materials[draw_call.material_index],
                });

            const size_t node_idx = draw_data_buffer_handles.push_back(
                gfx::buffers::create(cb_desc)
            );
            gfx::buffers::set_data(draw_data_buffer_handles[node_idx], &draw_constant_data[data_idx], sizeof(DrawConstantData));
        }

        TextureDesc depth_texture_desc = {
            .width = width,
            .height = height,
            .depth = 1,
            .num_mips = 1,
            .array_size = 1,
            .is_3d = false,
            .format = BufferFormat::D32,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL,
        };
        depth_target = gfx::textures::create(depth_texture_desc);


        gfx::cmd::cpu_wait(upload_receipt);
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        static size_t frame_idx = 0;
        frame_times[frame_idx] = time_data.delta_milliseconds_f;
        frame_idx = (frame_idx + 1) % 120;

        camera_controller.update(time_data.delta_seconds_f);
        view_constant_data.view = camera.view;
        view_constant_data.projection = camera.projection;
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.camera_position = camera.position;
        view_constant_data.time = time_data.elapsed_seconds_f;
    }

    void copy()
    {
        gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
    }

    void render() override final
    {
        CommandContextHandle cmd_ctx = gfx::begin_frame();

        ui::begin_frame();

        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::Begin("GLTF Loader");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);

            ImGui::PlotHistogram("Frame Times", frame_times, IM_ARRAYSIZE(frame_times), 0, 0, 0, D3D12_FLOAT32_MAX, ImVec2(240.0f, 80.0f));

            ImGui::End();
        }

        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        TextureHandle backbuffer = gfx::get_current_back_buffer_handle();
        gfx::cmd::clear_render_target(cmd_ctx, backbuffer, clear_color);
        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);

        gfx::cmd::graphics::set_active_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::graphics::set_pipeline_state(cmd_ctx, pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::set_render_targets(cmd_ctx, &backbuffer, 1, depth_target);

        gfx::cmd::graphics::bind_resource_table(cmd_ctx, 2);
        gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, view_cb_handle, 1);

        for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
            gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, draw_data_buffer_handles[i], 0);
            gfx::cmd::graphics::draw_mesh(cmd_ctx, gltf_context.draw_calls[i].mesh);
        }

        ui::end_frame(cmd_ctx);
        gfx::end_frame(cmd_ctx);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    GltfLoadingApp app{};
    return app.run();
}