#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/d3d12/globals.h"

using namespace zec;

struct PrefilteringPassConstants
{
    u32 src_texture_idx;
    u32 out_texture_idx;
    u32 mip_level;
};

enum struct TonemappingOperators : u32
{
    NONE = 0,
    REINHARD,
    ACES,
    UCHIMURA,
    NUM_TONEMAPPING_OPERATORS
};

struct TonemapPassConstants
{
    u32 src_texture;
    float exposure;
};

struct EnvMapInfo
{
    u32 current_mip_level = 0;
    u32 num_mips = 1;
};

class EnvMapCreationApp : public zec::App
{
public:
    EnvMapCreationApp() : App{ L"Prefiltered Environment Map Creator" } { }

    float exposure = 1.0f;
    EnvMapInfo env_map_info = {};

    ResourceLayoutHandle prefiltering_layout;
    PipelineStateHandle prefiltering_pso = {};

    ResourceLayoutHandle tonemapping_layout;
    PipelineStateHandle tonemapping_pso = {};

    TextureHandle unfiltered_env_maps = {};
    TextureHandle filtered_env_maps = {};
    TextureHandle input_envmap = {};
    TextureHandle output_envmap = {};
    MeshHandle fullscreen_mesh;

protected:
    void init() override final
    {
        // Initialize UI
        ui::initialize(window);

        // Prefiltering PSO
        {
            ResourceLayoutDesc prefiltering_layout_desc{
                .constants = {{
                    .visibility = ShaderVisibility::COMPUTE,
                    .num_constants = sizeof(PrefilteringPassConstants) / 4u,
                 }},
                .num_constants = 1,
                .num_constant_buffers = 0,
                .tables = {
                    {
                        .ranges = {
                            {.usage = ResourceAccess::READ, .count = ResourceLayoutRangeDesc::UNBOUNDED_COUNT },
                        },
                        .visibility = ShaderVisibility::COMPUTE,
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

            prefiltering_layout = create_resource_layout(prefiltering_layout_desc);
            PipelineStateObjectDesc pipeline_desc = {
                    .resource_layout = prefiltering_layout,
                .input_assembly_desc = {{
                    { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                    { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
                    } },
                    .shader_file_path = L"shaders/envmap_prefilter.hlsl",
            };
            pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
            prefiltering_pso = create_pipeline_state_object(pipeline_desc);
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

        input_envmap = load_texture_from_file("textures/venice_sunset_4k.hdr");

        constexpr u32 cubemap_face_width = 1024;
        TextureDesc output_envmap_desc{
            .width = cubemap_face_width,
            .height = cubemap_face_width,
            .depth = 1,
            .num_mips = 1u + u32(floor(log2(cubemap_face_width))),
            .array_size = 1,
            .is_cubemap = true,
            .is_3d = false,
            .format = BufferFormat::FLOAT_3,
            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE | RESOURCE_USAGE_SHADER_READABLE,
        };
        output_envmap = create_texture(output_envmap_desc);

        end_upload();

        {
            CommandContextHandle cmd_ctx = gfx::cmd::provision(CommandQueueType::COMPUTE);

            gfx::cmd::set_active_resource_layout(cmd_ctx, prefiltering_layout);
            gfx::cmd::set_pipeline_state(cmd_ctx, prefiltering_pso);

            PrefilteringPassConstants prefilter_constants = {
                .src_texture_idx = get_shader_readable_texture_index(input_envmap),
                .out_texture_idx = get_shader_writable_texture_index(output_envmap),
                .mip_level = 0,
            };

            gfx::cmd::bind_constant(cmd_ctx, &prefilter_constants, 2, 0);
            gfx::cmd::bind_resource_table(cmd_ctx, 1);

            // TODO: Calculate dispatch size based on TextureInfo, which I should make public and queryable.
            gfx::cmd::dispatch(cmd_ctx, );

            gfx::cmd::return_and_execute(&cmd_ctx, 1);
        }


    }

    void shutdown() override final
    {
        ui::destroy();
    }

    void update(const zec::TimeData& time_data) override final
    {
        // Check whether our shit is done?
    }

    void render() override final
    {
        CommandContextHandle cmd_ctx = begin_frame();

        ui::begin_frame();

        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::ShowDemoWindow();

            ImGui::Begin("Envmap Creator");

            constexpr u32 u32_one = 1u;
            constexpr u32 u32_zero = 0u;
            u32 slider_max = env_map_info.num_mips - 1;
            ImGui::SliderScalar("input u32", ImGuiDataType_U32, &env_map_info.current_mip_level, &u32_zero, &slider_max, "%u");
            ////ImGui::SliderInt("Mip Level", &env_map_info.current_mip_level, 0, env_map_info.num_mips - 1);
            ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 5.0f);

            ImGui::End();
        }

        vec4 clear_color = { 0.1f, 0.0f, 0.1f, 1.0f };
        TextureHandle render_target = get_current_back_buffer_handle();
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);
        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1);
        gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);

        ui::end_frame(cmd_ctx);
        end_frame(cmd_ctx);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    EnvMapCreationApp app{};
    return app.run();
}