#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"

// TODO: Remove this once no longer using DXCall directly
using namespace zec;

struct DrawData
{
    mat44 model_view_transform;
    mat44 projection_matrix;
    float padding[32];
};

static_assert(sizeof(DrawData) == 256);

class NormalMappingApp : public zec::App
{
public:
    NormalMappingApp() : App{ L"Normal Mapping" } { }

    float clear_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    Camera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{ input_manager };
    MeshHandle cube_mesh = {};
    BufferHandle cb_handle = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};
    DrawData mesh_transform = {};

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 2.0f;
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            ResourceLayoutDesc layout_desc{
                { ResourceLayoutEntryType::CONSTANT_BUFFER, ShaderVisibility::VERTEX },
            };

            resource_layout = renderer.create_resource_layout(layout_desc);
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

            pso_handle = renderer.create_pipeline_state_object(pipeline_desc);
        }

        renderer.begin_upload();

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
            mesh_desc.index_buffer_desc.usage = BUFFER_USAGE_INDEX;
            mesh_desc.index_buffer_desc.type = BufferType::DEFAULT;
            mesh_desc.index_buffer_desc.byte_size = sizeof(cube_indices);
            mesh_desc.index_buffer_desc.stride = sizeof(cube_indices[0]);
            mesh_desc.index_buffer_desc.data = (void*)cube_indices;

            mesh_desc.vertex_buffer_descs[0] = {
                    BUFFER_USAGE_VERTEX,
                    BufferType::DEFAULT,
                    sizeof(cube_positions),
                    3 * sizeof(cube_positions[0]),
                    (void*)(cube_positions)
            };
            mesh_desc.vertex_buffer_descs[1] = {
               BUFFER_USAGE_VERTEX,
               BufferType::DEFAULT,
               sizeof(cube_colors),
               sizeof(cube_colors[0]),
               (void*)(cube_colors)
            };

            cube_mesh = renderer.create_mesh(mesh_desc);
        }

        renderer.end_upload();

        mesh_transform.model_view_transform = identity_mat44();
        set_translation(mesh_transform.model_view_transform, vec3{ 0.0f, 0.0f, -2.0f });

        mesh_transform.projection_matrix = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create constant buffer
        {
            BufferDesc cb_desc = {};
            cb_desc.byte_size = sizeof(DrawData);
            cb_desc.data = &mesh_transform;
            cb_desc.stride = 0;
            cb_desc.type = BufferType::DEFAULT;
            cb_desc.usage = BUFFER_USAGE_CONSTANT | BUFFER_USAGE_DYNAMIC;

            cb_handle = renderer.create_buffer(cb_desc);
        }
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        camera_controller.update(time_data.delta_seconds_f);

        mesh_transform.model_view_transform = camera.view;
        renderer.update_buffer(cb_handle, &mesh_transform, sizeof(mesh_transform));
    }

    void render() override final
    {
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        D3D12_RECT scissor_rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        dx12::RenderTexture& render_target = renderer.swap_chain.back_buffers[renderer.current_frame_idx];

        // TODO: Provide handles for referencing render targets
        // TODO: Provide interface for clearing a render target
        renderer.cmd_list->ClearRenderTargetView(render_target.rtv, clear_color, 0, nullptr);


        renderer.set_active_resource_layout(resource_layout);
        renderer.set_pipeline_state(pso_handle);
        renderer.bind_constant_buffer(cb_handle, 0);
        renderer.cmd_list->RSSetViewports(1, &viewport);
        renderer.cmd_list->RSSetScissorRects(1, &scissor_rect);
        renderer.cmd_list->OMSetRenderTargets(1, &render_target.rtv, false, nullptr);
        renderer.draw_mesh(cube_mesh);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    NormalMappingApp app{};
    return app.run();
}