#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/d3d12/globals.h"

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

struct TonemapPassConstants
{
    u32 src_texture;
    float exposure;
};

class ToneMappingApp : public zec::App
{
public:
    ToneMappingApp() : App{ L"Basic Tone Mapping" } { }
    u64 frame_ctr = 0;
    float frame_times[120] = { 0.0f };
    float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float exposure = 1.0f;

    Camera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{};

    ViewConstantData view_constant_data = {};
    Array<DrawConstantData> draw_constant_data;
    gltf::Context gltf_context;

    BufferHandle view_cb_handle = {};
    Array<BufferHandle> draw_data_buffer_handles = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};
    ResourceLayoutHandle tonemapping_layout;
    PipelineStateHandle tonemapping_pso = {};

    TextureHandle depth_target = {};
    TextureHandle hdr_buffers[RENDER_LATENCY] = {};

    MeshHandle fullscreen_mesh;

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

        // Initialize UI
        ui::initialize(window);

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
                    {
                        .ranges = {
                            {.usage = ResourceAccess::READ, .count = ResourceLayoutRangeDesc::UNBOUNDED_COUNT },
                        },
                        .visibility = ShaderVisibility::PIXEL,
                    }
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

            resource_layout = create_resource_layout(layout_desc);
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
            pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
            pipeline_desc.depth_buffer_format = BufferFormat::D32;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
            pipeline_desc.depth_stencil_state.depth_write = TRUE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = create_pipeline_state_object(pipeline_desc);
        }

        // Tone mapping PSO
        {
            ResourceLayoutDesc tonemapping_layout_desc{
                .constants = {{
                    .visibility = ShaderVisibility::PIXEL,
                    .num_constants = 2
                 }},
                .num_constants = 1,
                .num_constant_buffers = 0,
                .tables = {
                    {
                        .ranges = {
                            {.usage = ResourceAccess::READ, .count = ResourceLayoutRangeDesc::UNBOUNDED_COUNT },
                        },
                        .visibility = ShaderVisibility::PIXEL,
                    }
                },
                .num_resource_tables = 1,
                .static_samplers = {
                    {
                        .filtering = SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
                        .wrap_u = SamplerWrapMode::CLAMP,
                        .wrap_v = SamplerWrapMode::CLAMP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };

            tonemapping_layout = create_resource_layout(tonemapping_layout_desc);
            PipelineStateObjectDesc pipeline_desc = {
                 .resource_layout = tonemapping_layout,
                .input_assembly_desc = {{
                    { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                    { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
                 } },
                 .rtv_formats = {{ BufferFormat::R8G8B8A8_UNORM_SRGB }},
                 .shader_file_path = L"shaders/basic_tone_map.hlsl",
            };
            pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            tonemapping_pso = create_pipeline_state_object(pipeline_desc);
        }

        begin_upload();

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

        fullscreen_mesh = create_mesh(fullscreen_desc);

        //gltf::load_gltf_file("models/damaged_helmet/DamagedHelmet.gltf", gltf_context);
        gltf::load_gltf_file("models/flight_helmet/FlightHelmet.gltf", gltf_context);

        end_upload();

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            .data = nullptr,
        };

        view_cb_handle = create_buffer(cb_desc);

        const size_t num_draws = gltf_context.draw_calls.size;
        draw_constant_data.reserve(num_draws);
        draw_data_buffer_handles.reserve(num_draws);
        for (size_t i = 0; i < num_draws; i++) {
            const size_t node_idx = draw_data_buffer_handles.push_back(
                create_buffer(cb_desc)
            );

            const auto& draw_call = gltf_context.draw_calls[i];
            const auto& transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx];
            const auto& normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx];

            draw_constant_data.push_back({
                .model = transform,
                .normal_transform = normal_transform,
                .material_data = gltf_context.materials[draw_call.material_index],
                });
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
        depth_target = create_texture(depth_texture_desc);

        TextureDesc hdr_buffer_desc = {
            .width = width,
            .height = height,
            .depth = 1,
            .num_mips = 1,
            .array_size = 1,
            .is_3d = false,
            .format = BufferFormat::R16G16B16A16_FLOAT,
            .usage = RESOURCE_USAGE_RENDER_TARGET | RESOURCE_USAGE_SHADER_READABLE
        };
        for (size_t i = 0; i < RENDER_LATENCY; i++) {
            hdr_buffers[i] = create_texture(hdr_buffer_desc);
        }
    }

    void shutdown() override final
    {
        ui::destroy();
    }

    void update(const zec::TimeData& time_data) override final
    {
        frame_times[dx12::g_current_cpu_frame % 120] = time_data.delta_milliseconds_f;

        camera_controller.update(time_data.delta_seconds_f);
        view_constant_data.view = camera.view;
        view_constant_data.projection = camera.projection;
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.camera_position = camera.position;
        view_constant_data.time = time_data.elapsed_seconds_f;
        update_buffer(view_cb_handle, &view_constant_data, sizeof(view_constant_data));

        for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
            update_buffer(draw_data_buffer_handles[i], &draw_constant_data[i], sizeof(DrawConstantData));
        }
    }

    void render() override final
    {
        CommandContextHandle cmd_ctx = begin_frame();

        ui::begin_frame();

        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::Begin("GLTF Loader");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);

            ImGui::PlotHistogram("Frame Times", frame_times, IM_ARRAYSIZE(frame_times), 0, 0, 0, D3D12_FLOAT32_MAX, ImVec2(240.0f, 80.0f));

            ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 5.0f);

            ImGui::End();
        }

        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };
        TextureHandle hdr_buffer = hdr_buffers[frame_ctr % RENDER_LATENCY];


        gfx::cmd::set_active_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);


        TextureTransitionDesc transitions[] = {
            {.texture = hdr_buffer, .before = RESOURCE_USAGE_SHADER_READABLE, .after = RESOURCE_USAGE_RENDER_TARGET},
        };
        gfx::cmd::transition_textures(cmd_ctx, transitions, ARRAY_SIZE(transitions));
        gfx::cmd::clear_render_target(cmd_ctx, hdr_buffer, clear_color);
        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);

        gfx::cmd::bind_resource_table(cmd_ctx, 2);
        gfx::cmd::bind_constant_buffer(cmd_ctx, view_cb_handle, 1);

        for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
            gfx::cmd::bind_constant_buffer(cmd_ctx, draw_data_buffer_handles[i], 0);
            gfx::cmd::draw_mesh(cmd_ctx, gltf_context.draw_calls[i].mesh);
        }

        gfx::cmd::set_active_resource_layout(cmd_ctx, tonemapping_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, tonemapping_pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        TextureHandle backbuffer = get_current_back_buffer_handle();
        gfx::cmd::set_render_targets(cmd_ctx, &backbuffer, 1);

        TonemapPassConstants tonemapping_constants = {
            .src_texture = get_shader_readable_texture_index(hdr_buffer),
            .exposure = exposure
        };
        gfx::cmd::bind_constant(cmd_ctx, &tonemapping_constants, 2, 0);
        gfx::cmd::bind_resource_table(cmd_ctx, 1);

        TextureTransitionDesc tonemapping_transitions[] = {
            {.texture = hdr_buffer, .before = RESOURCE_USAGE_RENDER_TARGET, .after = RESOURCE_USAGE_SHADER_READABLE},
        };
        gfx::cmd::transition_textures(cmd_ctx, tonemapping_transitions, ARRAY_SIZE(tonemapping_transitions));
        gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);

        ui::end_frame(cmd_ctx);
        end_frame(cmd_ctx);
        frame_ctr++;
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    ToneMappingApp app{};
    return app.run();
}