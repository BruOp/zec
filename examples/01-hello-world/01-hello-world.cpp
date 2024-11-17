#include <windows.h>
#include <app.h>
#include <core/zec_math.h>
#include <utils/exceptions.h>
#include <imgui/imgui.h>

using namespace zec;

struct DrawData
{
    mat4 model_transform;
    mat4 view_transform;
    mat4 projection_matrix;
    mat4 VP;
};
static_assert(sizeof(DrawData) == 256);

struct Mesh{
    rhi::BufferHandle index_buffer = {};
    rhi::BufferHandle vertex_buffers[2] = {};
};

class HelloWorldApp : public zec::App
{
public:
    HelloWorldApp() : App{ L"Hello World!" } { }

    vec4 clear_color = { 0.5f, 0.5f, 0.5f, 1.0f };

    rhi::Draw mesh_draw{};
    rhi::BufferHandle cb_handle = {};
    rhi::ResourceLayoutHandle resource_layout = {};
    rhi::PipelineStateHandle pso_handle = {};
    DrawData mesh_transform = {};

    float frame_times[120] = { 0.0f };

protected:
    void init() override final
    {

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            rhi::ResourceLayoutDesc layout_desc{
                .constant_buffers = {{ rhi::ShaderVisibility::VERTEX }},
                .num_constant_buffers = 1,
            };

            resource_layout = renderer.resource_layouts_create(layout_desc);
        }

        // Create the Pipeline State Object
        {
            // Compile the shader
            rhi::ShaderCompilationDesc shader_compilation_desc = {
                .used_stages = rhi::PIPELINE_STAGE_VERTEX | rhi::PIPELINE_STAGE_PIXEL,
                .shader_file_path = L"shaders/basic.hlsl",
            };

            std::string errors{};
            rhi::ShaderBlobsHandle blobs_handle{};
            ZecResult res = renderer.shaders_compile(shader_compilation_desc, blobs_handle, errors);
            if (res != ZecResult::SUCCESS || !errors.empty())
            {
                OutputDebugStringA(errors.c_str());
                ASSERT_FAIL("Shader compilation failed");
            }

            // PSO Desc
            rhi::PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { rhi::MeshAttribute::POSITION, 0, rhi::BufferFormat::FLOAT_3, 0 },
                { rhi::MeshAttribute::COLOR, 0, rhi::BufferFormat::UNORM8_4, 1 }
            } };
            pipeline_desc.rtv_formats[0] = rhi::BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.raster_state_desc.cull_mode = rhi::CullMode::BACK_CCW;
            pipeline_desc.depth_stencil_state.depth_write = FALSE;

            pso_handle = renderer.pipelines_create(blobs_handle, resource_layout, pipeline_desc);
            renderer.shaders_release_blobs(blobs_handle);
        }

        rhi::CommandContextHandle cmd_ctx = renderer.cmd_provision(rhi::CommandQueueType::COPY);
        {
            // Define the geometry for a triangle.
            constexpr float cube_positions[] = {
                -0.5f,  0.5f, -0.5f, // +Y (top face)
                 0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,  // -Y (bottom face)
                 0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
            };

            constexpr u32 cube_colors[] = {
                    0xff00ff00, // +Y (top face)
                    0xff00ffff,
                    0xffffffff,
                    0xffffff00,
                    0xffff0000, // -Y (bottom face)
                    0xffff00ff,
                    0xff0000ff,
                    0xff000000,
            };

            constexpr u16 cube_indices[] = {
                2, 1, 0,
                3, 2, 0,
                5, 1, 2,
                5, 6, 1,
                4, 3, 0,
                7, 4, 0,
                1, 7, 0,
                6, 7, 1,
                4, 2, 3,
                4, 5, 2,
                7, 5, 4,
                7, 6, 5
            };

            {
                const rhi::BufferDesc index_buffer_desc = {
                    .usage = rhi::RESOURCE_USAGE_INDEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = sizeof(cube_indices),
                    .stride = sizeof(cube_indices[0]),
                };
                mesh_draw.index_buffer = renderer.buffers_create(index_buffer_desc);
                renderer.buffers_set_data(cmd_ctx, mesh_draw.index_buffer, cube_indices, sizeof(cube_indices));
                mesh_draw.index_count = ARRAYSIZE(cube_indices);
            }

            {
                rhi::BufferDesc vertex_buffer_desc = {
                    .usage = rhi::RESOURCE_USAGE_VERTEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = sizeof(cube_positions),
                    .stride = 3 * sizeof(cube_positions[0]),
                };
                mesh_draw.vertex_buffers[0] = renderer.buffers_create(vertex_buffer_desc);
                renderer.buffers_set_data(cmd_ctx, mesh_draw.vertex_buffers[0], cube_positions, sizeof(cube_positions));

                vertex_buffer_desc = {
                    .usage = rhi::RESOURCE_USAGE_VERTEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = sizeof(cube_colors),
                    .stride = sizeof(cube_colors[0]),
                };
                mesh_draw.vertex_buffers[1] = renderer.buffers_create(vertex_buffer_desc);
                renderer.buffers_set_data(cmd_ctx, mesh_draw.vertex_buffers[1], cube_colors, sizeof(cube_colors));

                mesh_draw.num_vertex_buffers = 2;
            }
        }
        rhi::CmdReceipt receipt = renderer.cmd_return_and_execute(&cmd_ctx, 1);

        mesh_transform.model_transform = identity_mat4();
        mesh_transform.view_transform = look_at4x4({ 0.0f, 0.0f, -2.0f }, { 0.0f, 0.0f, 0.0f });
        mesh_transform.projection_matrix = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );
        mesh_transform.VP = mesh_transform.projection_matrix * mesh_transform.view_transform;

        // Create constant buffer
        {
            rhi::BufferDesc cb_desc = {};
            cb_desc.byte_size = sizeof(DrawData);
            cb_desc.stride = 0;
            cb_desc.type = rhi::BufferType::DEFAULT;
            cb_desc.usage = rhi::RESOURCE_USAGE_CONSTANT | rhi::RESOURCE_USAGE_DYNAMIC;

            cb_handle = renderer.buffers_create(cb_desc);
            renderer.buffers_set_data(cb_handle, &mesh_transform, sizeof(DrawData));
        }

        renderer.cmd_cpu_wait(receipt);
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        static size_t frame_idx = 0;
        frame_times[frame_idx % 120] = time_data.delta_milliseconds_f;
        frame_idx++;

        ui_renderer.begin_frame();
        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);

            ImGui::PlotHistogram("Frame Times", frame_times, IM_ARRAYSIZE(frame_times), 0, 0, 0);

            ImGui::End();
        }
        ui_renderer.end_frame();

        quaternion q = from_axis_angle(vec3{ 0.0f, 1.0f, -1.0f }, time_data.delta_seconds_f);
        rotate(mesh_transform.model_transform, q);
    }

    void copy(const zec::TimeData& time_data) override final
    {
        renderer.buffers_update(cb_handle, &mesh_transform, sizeof(mesh_transform));
    }

    void render(const zec::TimeData& time_data) override final
    {
        rhi::CommandContextHandle command_ctx = renderer.begin_frame();

        rhi::Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        rhi::Scissor scissor{ 0, 0, width, height };

        rhi::TextureHandle render_target = renderer.get_current_back_buffer_handle();
        renderer.cmd_clear_render_target(command_ctx, render_target, clear_color);

        renderer.cmd_set_graphics_resource_layout(command_ctx, resource_layout);
        renderer.cmd_set_graphics_pipeline_state(command_ctx, pso_handle);
        renderer.cmd_bind_graphics_constant_buffer(command_ctx, cb_handle, 0);
        renderer.cmd_set_viewports(command_ctx, &viewport, 1);
        renderer.cmd_set_scissors(command_ctx, &scissor, 1);

        renderer.cmd_set_render_targets(command_ctx, &render_target, 1);
        renderer.cmd_draw(command_ctx, mesh_draw);
        ui_renderer.draw_frame(command_ctx);
        renderer.end_frame(command_ctx);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HelloWorldApp app{};
    return app.run();
}