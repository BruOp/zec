#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/d3d12/globals.h"

#include <imgui-filebrowser/imfilebrowser.h>

using namespace zec;

struct PrefilteringPassConstants
{
    u32 src_texture_idx;
    u32 out_texture_initial_idx;
    u32 mip_idx;
    u32 img_width;
};

struct ViewConstantData
{
    mat4 invVP;
    float exposure = 1.0f;
    u32 mip_level = 0;
    u32 env_map_idx;
    float padding[45];
};
static_assert(sizeof(ViewConstantData) == 256);

struct PrefilterEnvMapDesc
{
    TextureHandle src_texture;
    u32 out_texture_width;
};

struct PrefilterEnvMapTask
{
    enum struct TaskState : u8
    {
        NOT_ISSUED = 0,
        IN_FLIGHT,
        REQUIRES_TRANSITION,
        COMPLETED
    };


    TaskState state = TaskState::NOT_ISSUED;
    CmdReceipt receipt = {};

    ResourceLayoutHandle layout;
    PipelineStateHandle pso = {};

    TextureHandle out_texture;
};

void init(PrefilterEnvMapTask& task)
{
    task.state = PrefilterEnvMapTask::TaskState::NOT_ISSUED;
    task.receipt = { };

    ResourceLayoutDesc prefiltering_layout_desc{
        .constants = {{
            .visibility = ShaderVisibility::COMPUTE,
            .num_constants = sizeof(PrefilteringPassConstants) / 4u,
         }},
        .num_constants = 1,
        .num_constant_buffers = 0,
        .tables = {
            {.usage = ResourceAccess::READ, .count = 4096 },
            {.usage = ResourceAccess::WRITE, .count = 4096 },
        },
        .num_resource_tables = 2,
        .static_samplers = {
            {
                .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                .wrap_u = SamplerWrapMode::CLAMP,
                .wrap_v = SamplerWrapMode::CLAMP,
                .binding_slot = 0,
            },
    },
    .num_static_samplers = 1,
    };

    task.layout = gfx::pipelines::create_resource_layout(prefiltering_layout_desc);

    PipelineStateObjectDesc pipeline_desc = {
        .resource_layout = task.layout,
        .shader_file_path = L"shaders/envmap_prefilter.hlsl",
    };
    pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
    pipeline_desc.used_stages = PIPELINE_STAGE_COMPUTE;

    task.pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);

}

void issue_commands(PrefilterEnvMapTask& task, const PrefilterEnvMapDesc desc)
{
    TextureDesc output_envmap_desc{
        .width = desc.out_texture_width,
        .height = desc.out_texture_width,
        .depth = 1,
        .num_mips = u32(log2(desc.out_texture_width)),
        .array_size = 6,
        .is_cubemap = true,
        .is_3d = false,
        .format = BufferFormat::HALF_4,
        .usage = RESOURCE_USAGE_COMPUTE_WRITABLE | RESOURCE_USAGE_SHADER_READABLE,
        .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
    };
    task.out_texture = gfx::textures::create(output_envmap_desc);

    CommandContextHandle cmd_ctx = gfx::cmd::provision(CommandQueueType::ASYNC_COMPUTE);

    gfx::cmd::set_active_resource_layout(cmd_ctx, task.layout);
    gfx::cmd::set_pipeline_state(cmd_ctx, task.pso);

    gfx::cmd::bind_resource_table(cmd_ctx, 1);
    gfx::cmd::bind_resource_table(cmd_ctx, 2);
    TextureInfo texture_info = gfx::textures::get_texture_info(task.out_texture);

    for (u32 mip_idx = 0; mip_idx < texture_info.num_mips; mip_idx++) {

        // Dispatch size is based on number of threads for each direction
        u32 mip_width = texture_info.width >> mip_idx;

        PrefilteringPassConstants prefilter_constants = {
            .src_texture_idx = gfx::textures::get_shader_readable_index(desc.src_texture),
            .out_texture_initial_idx = gfx::textures::get_shader_writable_index(task.out_texture),
            .mip_idx = mip_idx,
            .img_width = texture_info.width,
        };
        gfx::cmd::bind_constant(cmd_ctx, &prefilter_constants, 4, 0);

        const u32 dispatch_size = max(1u, mip_width / 8u);

        gfx::cmd::dispatch(cmd_ctx, dispatch_size, dispatch_size, 6);
    }

    task.receipt = gfx::cmd::return_and_execute(&cmd_ctx, 1);
    task.state = PrefilterEnvMapTask::TaskState::IN_FLIGHT;
}

void check_status(PrefilterEnvMapTask& task)
{
    if (task.state == PrefilterEnvMapTask::TaskState::IN_FLIGHT && gfx::cmd::check_status(task.receipt)) {
        task.state = PrefilterEnvMapTask::TaskState::REQUIRES_TRANSITION;
    }
}

void handle_transition(PrefilterEnvMapTask& task, const CommandContextHandle cmd_ctx)
{
    ASSERT(task.state == PrefilterEnvMapTask::TaskState::REQUIRES_TRANSITION);
    gfx::cmd::gpu_wait(CommandQueueType::GRAPHICS, task.receipt);

    TextureTransitionDesc transition = {
        .texture = task.out_texture,
        .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
        .after = RESOURCE_USAGE_SHADER_READABLE
    };
    gfx::cmd::transition_textures(cmd_ctx, &transition, 1);
    task.state = PrefilterEnvMapTask::TaskState::COMPLETED;
}

void load_src_envmap(PrefilterEnvMapTask& prefiltering_task, const char* file_path)
{
    gfx::begin_upload();
    TextureHandle input_envmap = gfx::textures::load_from_file(file_path);
    CmdReceipt receipt = gfx::end_upload();
    gfx::cmd::gpu_wait(CommandQueueType::COMPUTE, receipt);
    issue_commands(prefiltering_task, { .src_texture = input_envmap, .out_texture_width = 512 });
}

class EnvMapCreationApp : public zec::App
{
public:
    EnvMapCreationApp() : App{ L"Prefiltered Environment Map Creator" } { }

    float roughness = 0.001f;

    ImGui::FileBrowser file_dialog = ImGui::FileBrowser{
        ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc
    };

    Camera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{};

    PrefilterEnvMapTask prefiltering_task;

    ViewConstantData view_constant_data = {};

    ResourceLayoutHandle background_pass_layout;
    PipelineStateHandle background_pass_pso = {};

    ResourceLayoutHandle tonemapping_layout;
    PipelineStateHandle tonemapping_pso = {};

    BufferHandle view_cb_handle = {};

    TextureHandle background_texture = {};

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

        // (optional) set browser properties
        file_dialog.SetTitle("Save File as");
        file_dialog.SetTypeFilters({ ".dds" });

        // Background Rendering
        {
            ResourceLayoutDesc background_pass_layout_desc{
                .num_constants = 0,
                .constant_buffers = {
                    {.visibility = ShaderVisibility::ALL },
                },
                .num_constant_buffers = 1,
                .tables = {
                    {.usage = ResourceAccess::READ, .count = 4096 },
                },
                .num_resource_tables = 1,
                .static_samplers = {
                    {
                        .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                        .wrap_u = SamplerWrapMode::CLAMP,
                        .wrap_v = SamplerWrapMode::CLAMP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };
            background_pass_layout = gfx::pipelines::create_resource_layout(background_pass_layout_desc);

            PipelineStateObjectDesc pipeline_desc = {
                    .resource_layout = background_pass_layout,
                    .input_assembly_desc = {{
                        { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                        { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
                    } },
                    .rtv_formats = {{ BufferFormat::R8G8B8A8_UNORM_SRGB }},
                    .shader_file_path = L"shaders/background_pass.hlsl",
            };
            pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
            background_pass_pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        }

        ::init(prefiltering_task);

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

        gfx::end_upload();

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            .data = nullptr,
        };
        view_cb_handle = gfx::buffers::create(cb_desc);

        TextureDesc hdr_buffer_desc = {
            .width = width,
            .height = height,
            .depth = 1,
            .num_mips = 1,
            .array_size = 1,
            .is_3d = false,
            .format = BufferFormat::R16G16B16A16_FLOAT,
            .usage = RESOURCE_USAGE_RENDER_TARGET | RESOURCE_USAGE_SHADER_READABLE,
            .initial_state = RESOURCE_USAGE_SHADER_READABLE,
        };

    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        if (is_valid(prefiltering_task.out_texture)) {
            TextureInfo& texture_info = gfx::textures::get_texture_info(prefiltering_task.out_texture);

            camera_controller.update(time_data.delta_seconds_f);
            view_constant_data.invVP = invert(camera.projection * mat4(to_mat3(camera.view), {}));
            view_constant_data.env_map_idx = gfx::textures::get_shader_readable_index(prefiltering_task.out_texture);
        }
    }

    void copy() override final
    {
        if (is_valid(prefiltering_task.out_texture)) {

            gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
        }
    }

    void render() override final
    {
        check_status(prefiltering_task);

        CommandContextHandle cmd_ctx = gfx::begin_frame();

        ui::begin_frame();

        {
            const auto framerate = ImGui::GetIO().Framerate;

            constexpr u32 u32_one = 1u;
            constexpr u32 u32_zero = 0u;

            ImGui::Begin("Envmap Creator");

            switch (prefiltering_task.state) {
            case PrefilterEnvMapTask::TaskState::NOT_ISSUED:
                ImGui::Text("Currently only works with DDS files that already have mip-maps.");
                if (ImGui::Button("Load env map")) {
                    file_dialog.Open();
                }

                file_dialog.Display();

                if (file_dialog.HasSelected()) {
                    auto input_file_path = file_dialog.GetSelected().string();

                    load_src_envmap(prefiltering_task, input_file_path.c_str());
                    file_dialog.ClearSelected();
                }

                break;
            case PrefilterEnvMapTask::TaskState::IN_FLIGHT:
                ImGui::Text("Generating prefiltered environment map");
                break;
            case PrefilterEnvMapTask::TaskState::COMPLETED:
                u32 slider_max = gfx::textures::get_texture_info(prefiltering_task.out_texture).num_mips;
                ImGui::SliderScalar("Mip Level", ImGuiDataType_U32, &view_constant_data.mip_level, &u32_zero, &slider_max, "%u");
                ImGui::DragFloat("Exposure", &view_constant_data.exposure, 0.01f, 0.0f, 5.0f);

                if (ImGui::Button("Save DDS")) {
                    file_dialog.Open();
                }

                file_dialog.Display();

                if (file_dialog.HasSelected()) {
                    gfx::textures::save_to_file(
                        prefiltering_task.out_texture,
                        file_dialog.GetSelected().c_str(),
                        RESOURCE_USAGE_SHADER_READABLE
                    );

                    file_dialog.ClearSelected();
                }
                break;
            }

            ImGui::End();
        }

        vec4 clear_color = { 0.1f, 0.0f, 0.1f, 1.0f };
        TextureHandle render_target = gfx::get_current_back_buffer_handle();
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1);
        gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);

        switch (prefiltering_task.state) {
        case PrefilterEnvMapTask::TaskState::REQUIRES_TRANSITION:
            handle_transition(prefiltering_task, cmd_ctx);
        case PrefilterEnvMapTask::TaskState::COMPLETED:
            gfx::cmd::set_active_resource_layout(cmd_ctx, background_pass_layout);
            gfx::cmd::set_pipeline_state(cmd_ctx, background_pass_pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);
            gfx::cmd::bind_resource_table(cmd_ctx, 1);

            gfx::cmd::bind_constant_buffer(cmd_ctx, view_cb_handle, 0);
            gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
            break;
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
    EnvMapCreationApp app{};
    return app.run();
}