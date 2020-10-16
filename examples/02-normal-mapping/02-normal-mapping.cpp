#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"

// TODO: Remove this once no longer using DXCall directly
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

    Camera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{ input_manager };
    MeshHandle cube_mesh = {};
    BufferHandle view_cb_handle = {};
    BufferHandle draw_cb_handle = {};
    TextureHandle albedo_map = {};
    TextureHandle normal_map = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};
    ViewConstantData view_constant_data = {};
    DrawConstantData draw_constant_data = {};

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 2.0f;
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
            pipeline_desc.shader_file_path = L"shaders/normal_mapping.hlsl";
            pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.depth_stencil_state.depth_write = FALSE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = create_pipeline_state_object(pipeline_desc);
        }

        begin_upload();

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

            MeshDesc mesh_desc{};
            mesh_desc.index_buffer_desc.usage = RESOURCE_USAGE_INDEX;
            mesh_desc.index_buffer_desc.type = BufferType::DEFAULT;
            mesh_desc.index_buffer_desc.byte_size = sizeof(cube_indices);
            mesh_desc.index_buffer_desc.stride = sizeof(cube_indices[0]);
            mesh_desc.index_buffer_desc.data = (void*)cube_indices;

            mesh_desc.vertex_buffer_descs[0] = {
                    .usage = RESOURCE_USAGE_VERTEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(cube_positions),
                    .stride = 3 * sizeof(cube_positions[0]),
                    .data = (void*)(cube_positions)
            };
            mesh_desc.vertex_buffer_descs[1] = {
               .usage = RESOURCE_USAGE_VERTEX,
               .type = BufferType::DEFAULT,
               .byte_size = sizeof(cube_normals),
               .stride = 3 * sizeof(cube_normals[0]),
               .data = (void*)(cube_normals)
            };
            mesh_desc.vertex_buffer_descs[2] = {
               .usage = RESOURCE_USAGE_VERTEX,
               .type = BufferType::DEFAULT,
               .byte_size = sizeof(cube_uvs),
               .stride = 2 * sizeof(cube_uvs[0]),
               .data = (void*)(cube_uvs)
            };

            cube_mesh = create_mesh(mesh_desc);
        }

        // Texture creation
        {
            albedo_map = load_texture_from_file("textures/stone01.dds");
            normal_map = load_texture_from_file("textures/bump01.dds");
        }

        end_upload();

        camera.projection = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create constant buffers
        {
            BufferDesc cb_desc = {
                .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(ViewConstantData),
                .stride = 0,
                .data = nullptr,
            };

            view_cb_handle = create_buffer(cb_desc);

            // Same size and usage etc, so we can use the same desc
            draw_cb_handle = create_buffer(cb_desc);
        }
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
        camera_controller.update(time_data.delta_seconds_f);
        view_constant_data.view = camera.view;
        view_constant_data.projection = camera.projection;
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.camera_position = get_translation(camera.view);
        view_constant_data.time = time_data.elapsed_seconds_f;
        update_buffer(view_cb_handle, &view_constant_data, sizeof(view_constant_data));

        draw_constant_data.model = identity_mat4();
        draw_constant_data.inv_model = identity_mat4();
        // TODO: Actually grab the relevant descriptors?
        draw_constant_data.albedo_map_idx = get_shader_readable_texture_index(albedo_map);
        draw_constant_data.normal_map_idx = get_shader_readable_texture_index(normal_map);
        update_buffer(draw_cb_handle, &draw_constant_data, sizeof(draw_constant_data));
    }

    void render() override final
    {
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        TextureHandle backbuffer = get_current_back_buffer_handle();
        clear_render_target(backbuffer, clear_color);

        set_active_resource_layout(resource_layout);
        set_pipeline_state(pso_handle);
        bind_resource_table(2);
        bind_constant_buffer(draw_cb_handle, 0);
        bind_constant_buffer(view_cb_handle, 1);
        set_viewports(&viewport, 1);
        set_scissors(&scissor, 1);

        set_render_targets(&backbuffer, 1);
        draw_mesh(cube_mesh);
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