#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "imgui/imgui.h"
#include "gfx/d3d12/globals.h"

using namespace zec;

struct DrawData
{
    mat4 model_transform;
    mat4 view_transform;
    mat4 projection_matrix;
    mat4 VP;
};

static_assert(sizeof(DrawData) == 256);

class HelloWorldApp : public zec::App
{
public:
    HelloWorldApp() : App{ L"Hello World!" } { }

    vec4 clear_color = { 0.5f, 0.5f, 0.5f, 1.0f };

    MeshHandle cube_mesh = {};
    BufferHandle cb_handle = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};
    DrawData mesh_transform = {};

    float frame_times[120] = { 0.0f };

protected:
    void init() override final
    {

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            ResourceLayoutDesc layout_desc{
                .constant_buffers = {{ ShaderVisibility::VERTEX }},
                .num_constant_buffers = 1,
            };

            resource_layout = create_resource_layout(layout_desc);
        }

        // Create the Pipeline State Object
        {
            // Compile the shader
            PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_COLOR, 0, BufferFormat::UNORM8_4, 1 }
            } };
            pipeline_desc.shader_file_path = L"shaders/basic.hlsl";
            pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.depth_stencil_state.depth_write = FALSE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = create_pipeline_state_object(pipeline_desc);
        }

        // Initialize UI
        ui::initialize(window);

        begin_upload();

        // Create the vertex buffer.
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

            MeshDesc mesh_desc{};
            mesh_desc.index_buffer_desc.usage = RESOURCE_USAGE_INDEX;
            mesh_desc.index_buffer_desc.type = BufferType::DEFAULT;
            mesh_desc.index_buffer_desc.byte_size = sizeof(cube_indices);
            mesh_desc.index_buffer_desc.stride = sizeof(cube_indices[0]);
            mesh_desc.index_buffer_desc.data = (void*)cube_indices;

            mesh_desc.vertex_buffer_descs[0] = {
                    RESOURCE_USAGE_VERTEX,
                    BufferType::DEFAULT,
                    sizeof(cube_positions),
                    3 * sizeof(cube_positions[0]),
                    (void*)(cube_positions)
            };
            mesh_desc.vertex_buffer_descs[1] = {
               RESOURCE_USAGE_VERTEX,
               BufferType::DEFAULT,
               sizeof(cube_colors),
               sizeof(cube_colors[0]),
               (void*)(cube_colors)
            };

            cube_mesh = create_mesh(mesh_desc);
        }

        end_upload();

        mesh_transform.model_transform = identity_mat4();
        mesh_transform.view_transform = look_at({ 0.0f, 0.0f, -2.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        mesh_transform.projection_matrix = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );
        mesh_transform.VP = mesh_transform.projection_matrix * mesh_transform.view_transform;

        // Create constant buffer
        {
            BufferDesc cb_desc = {};
            cb_desc.byte_size = sizeof(DrawData);
            cb_desc.data = &mesh_transform;
            cb_desc.stride = 0;
            cb_desc.type = BufferType::DEFAULT;
            cb_desc.usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC;

            cb_handle = create_buffer(cb_desc);
        }
    }

    void shutdown() override final
    {
        ui::destroy();
    }

    void update(const zec::TimeData& time_data) override final
    {
        frame_times[dx12::g_current_cpu_frame % 120] = time_data.delta_milliseconds_f;

        quaternion q = from_axis_angle(vec3{ 0.0f, 1.0f, -1.0f }, time_data.delta_seconds_f);
        rotate(mesh_transform.model_transform, q);

        update_buffer(cb_handle, &mesh_transform, sizeof(mesh_transform));
    }

    void render() override final
    {
        CommandContextHandle command_ctx = begin_frame();
        ui::begin_frame();

        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);

            ImGui::PlotHistogram("Frame Times", frame_times, IM_ARRAYSIZE(frame_times), 0, 0, 0, D3D12_FLOAT32_MAX, ImVec2(240.0f, 80.0f));

            ImGui::End();
        }

        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        TextureHandle render_target = get_current_back_buffer_handle();
        gfx::cmd::clear_render_target(command_ctx, render_target, clear_color);

        gfx::cmd::set_active_resource_layout(command_ctx, resource_layout);
        gfx::cmd::set_pipeline_state(command_ctx, pso_handle);
        gfx::cmd::bind_constant_buffer(command_ctx, cb_handle, 0);
        gfx::cmd::set_viewports(command_ctx, &viewport, 1);
        gfx::cmd::set_scissors(command_ctx, &scissor, 1);

        gfx::cmd::set_render_targets(command_ctx, &render_target, 1);
        gfx::cmd::draw_mesh(command_ctx, cube_mesh);

        ui::end_frame(command_ctx);
        end_frame(command_ctx);
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