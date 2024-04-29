#include <windows.h>
#include <app.h>
#include <core/zec_math.h>
#include <utils/exceptions.h>
#include <imgui/imgui.h>
#include <camera.h>

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
    mat4 model;
    mat4 inv_model;
    u32 albedo_map_idx;
    u32 normal_map_idx;
    float padding[30];
};

static_assert(sizeof(DrawConstantData) == 256);


class NormalMappingApp : public zec::App
{
public:
    NormalMappingApp() : App{ L"Normal Mapping" } { }

    float clear_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    PerspectiveCamera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{ };
    rhi::Draw cube_mesh = {};
    rhi::BufferHandle view_cb_handle = {};
    rhi::BufferHandle draw_cb_handle = {};
    rhi::TextureHandle albedo_map = {};
    rhi::TextureHandle normal_map = {};
    rhi::ResourceLayoutHandle resource_layout = {};
    rhi::PipelineStateHandle pso_handle = {};
    ViewConstantData view_constant_data = {};
    DrawConstantData draw_constant_data = {};

protected:
    void init() override final
    {
        camera_controller.settings = {
            .yaw_sensitivity = 200.0f,
            .pitch_sensitivity = 200.0f,
        };
        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 2.0f;
        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            constexpr u32 num_textures = 1024;
            rhi::ResourceLayoutDesc layout_desc{
                .num_constants = 0,
                .constant_buffers = {
                    { rhi::ShaderVisibility::ALL },
                    { rhi::ShaderVisibility::ALL },
                },
                .num_constant_buffers = 2,
            };

            resource_layout = renderer.resource_layouts_create(layout_desc);
        }

        // Create the Pipeline State Object
        {
            // Compile the shader
            rhi::ShaderCompilationDesc shader_compilation_desc = {
                .used_stages = rhi::PIPELINE_STAGE_VERTEX | rhi::PIPELINE_STAGE_PIXEL,
                .shader_file_path = L"shaders/normal_mapping.hlsl",
            };

            std::string errors{};
            rhi::ShaderBlobsHandle blobs_handle{};
            ZecResult res = renderer.shaders_compile(shader_compilation_desc, blobs_handle, errors);
            if (res != ZecResult::SUCCESS || !errors.empty())
            {
                OutputDebugStringA(errors.c_str());
                ASSERT_FAIL("Shader compilation failed");
            }

            rhi::PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { rhi::MeshAttribute::POSITION, 0, rhi::BufferFormat::FLOAT_3, 0 },
                { rhi::MeshAttribute::NORMAL, 0, rhi::BufferFormat::FLOAT_3, 1 },
                { rhi::MeshAttribute::TEXCOORD, 0, rhi::BufferFormat::FLOAT_2, 2 },
            } };
            pipeline_desc.rtv_formats[0] = rhi::BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.raster_state_desc.cull_mode = rhi::CullMode::BACK_CCW;
            pipeline_desc.depth_stencil_state.depth_write = FALSE;

            pso_handle = renderer.pipelines_create(blobs_handle, resource_layout, pipeline_desc);
            renderer.shaders_release_blobs(blobs_handle);
        }

        rhi::CommandContextHandle cmd_ctx = renderer.cmd_provision(rhi::CommandQueueType::COPY);
        // Create the vertex buffer.
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
                 1.0f,  0.0f,  0.0f,
                 1.0f,  0.0f,  0.0f,
                 1.0f,  0.0f,  0.0f,
                -1.0f,  0.0f,  0.0f, // -X (Left face)
                -1.0f,  0.0f,  0.0f,
                -1.0f,  0.0f,  0.0f,
                -1.0f,  0.0f,  0.0f,
                 0.0f,  0.0f, -1.0f, // -Z (Back face)
                 0.0f,  0.0f, -1.0f,
                 0.0f,  0.0f, -1.0f,
                 0.0f,  0.0f, -1.0f,
                 0.0f,  0.0f,  1.0f, // +Z (Front face)
                 0.0f,  0.0f,  1.0f,
                 0.0f,  0.0f,  1.0f,
                 0.0f,  0.0f,  1.0f,
                 0.0f, -1.0f,  0.0f, // -Y (Bottom face)
                 0.0f, -1.0f,  0.0f,
                 0.0f, -1.0f,  0.0f,
                 0.0f, -1.0f,  0.0f,
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

            rhi::BufferDesc index_buffer_desc = {
                .usage = rhi::RESOURCE_USAGE_INDEX,
                .type = rhi::BufferType::DEFAULT,
                .byte_size = sizeof(cube_indices),
                .stride = sizeof(cube_indices[0]),
            };
            cube_mesh.index_buffer = renderer.buffers_create(index_buffer_desc);
            cube_mesh.index_count = ARRAYSIZE(cube_indices);
            renderer.buffers_set_data(cmd_ctx, cube_mesh.index_buffer, cube_indices, sizeof(cube_indices));

            rhi::BufferDesc vertex_buffer_desc = {
                    .usage = rhi::RESOURCE_USAGE_VERTEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = sizeof(cube_positions),
                    .stride = 3 * sizeof(cube_positions[0]),
            };
            cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers] = renderer.buffers_create(vertex_buffer_desc);
            renderer.buffers_set_data(cmd_ctx, cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers++], cube_positions, sizeof(cube_positions));

            vertex_buffer_desc = {
                .usage = rhi::RESOURCE_USAGE_VERTEX,
                .type = rhi::BufferType::DEFAULT,
               .byte_size = sizeof(cube_normals),
               .stride = 3 * sizeof(cube_normals[0]),
            };
            cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers] = renderer.buffers_create(vertex_buffer_desc);
            renderer.buffers_set_data(cmd_ctx, cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers++], cube_normals, sizeof(cube_normals));
            vertex_buffer_desc = {
                .usage = rhi::RESOURCE_USAGE_VERTEX,
                .type = rhi::BufferType::DEFAULT,
               .byte_size = sizeof(cube_uvs),
               .stride = 2 * sizeof(cube_uvs[0]),
            };
            cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers] = renderer.buffers_create(vertex_buffer_desc);
            renderer.buffers_set_data(cmd_ctx, cube_mesh.vertex_buffers[cube_mesh.num_vertex_buffers++], cube_uvs, sizeof(cube_uvs));
        }

        // Texture creation
        {
            albedo_map = renderer.textures_create_from_file(cmd_ctx, L"textures/stone01.dds");
            normal_map = renderer.textures_create_from_file(cmd_ctx, L"textures/bump01.dds");

            draw_constant_data.albedo_map_idx = renderer.get_readable_index(albedo_map);
            draw_constant_data.normal_map_idx = renderer.get_readable_index(normal_map);
        }

        rhi::CmdReceipt receipt = renderer.cmd_return_and_execute(&cmd_ctx, 1);

        camera.projection = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create constant buffers
        {
            rhi::BufferDesc cb_desc = {
                .usage = rhi::RESOURCE_USAGE_CONSTANT | rhi::RESOURCE_USAGE_DYNAMIC,
                .type = rhi::BufferType::DEFAULT,
                .byte_size = sizeof(ViewConstantData),
                .stride = 0,
            };

            view_cb_handle = renderer.buffers_create(cb_desc);

            // Same size and usage etc, so we can use the same desc
            draw_cb_handle = renderer.buffers_create(cb_desc);
        }

        renderer.cmd_cpu_wait(receipt);
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        const input::InputState input_state = input_manager.get_state();
        camera_controller.update(camera, input_state, time_data.delta_seconds_f);
        camera_controller.apply(camera);
    }

    void copy(const zec::TimeData& time_data) override final
    {
        view_constant_data.view = camera.view;
        view_constant_data.projection = camera.projection;
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.camera_position = get_translation(camera.view);
        view_constant_data.time = time_data.elapsed_seconds_f;

        draw_constant_data.model = identity_mat4();
        draw_constant_data.inv_model = identity_mat4();
    }

    void render(const zec::TimeData& time_data) override final
    {
        renderer.buffers_update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
        renderer.buffers_update(draw_cb_handle, &draw_constant_data, sizeof(draw_constant_data));

        rhi::CommandContextHandle cmd_ctx = renderer.begin_frame();
        rhi::Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        rhi::Scissor scissor{ 0, 0, width, height };

        rhi::TextureHandle backbuffer = renderer.get_current_back_buffer_handle();
        renderer.cmd_clear_render_target(cmd_ctx, backbuffer, clear_color);

        renderer.cmd_set_graphics_resource_layout(cmd_ctx, resource_layout);
        renderer.cmd_set_graphics_pipeline_state(cmd_ctx, pso_handle);
        renderer.cmd_bind_graphics_constant_buffer(cmd_ctx, draw_cb_handle, 0);
        renderer.cmd_bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, 1);

        renderer.cmd_set_viewports(cmd_ctx, &viewport, 1);
        renderer.cmd_set_scissors(cmd_ctx, &scissor, 1);

        renderer.cmd_set_render_targets(cmd_ctx, &backbuffer, 1);
        renderer.cmd_draw(cmd_ctx, cube_mesh);

        renderer.end_frame(cmd_ctx);
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