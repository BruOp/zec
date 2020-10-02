#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"

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
    gltf::MaterialData material_data;
    float padding[18];
};

static_assert(sizeof(DrawConstantData) == 256);


class NormalMappingApp : public zec::App
{
public:
    NormalMappingApp() : App{ L"Normal Mapping" } { }

    float clear_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    Camera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{ input_manager };

    ViewConstantData view_constant_data = {};
    Array<DrawConstantData> draw_constant_data;
    gltf::Context gltf_context;

    BufferHandle view_cb_handle = {};
    Array<BufferHandle> draw_data_buffer_handles = {};
    ResourceLayoutHandle resource_layout = {};
    PipelineStateHandle pso_handle = {};

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
                    {
                        .ranges = {
                            {.usage = ResourceLayoutRangeUsage::READ, .count = ResourceLayoutRangeDesc::UNBOUNDED_COUNT },
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
            pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.depth_buffer_format = BufferFormat::D32;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
            pipeline_desc.depth_stencil_state.depth_write = TRUE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = create_pipeline_state_object(pipeline_desc);
        }

        begin_upload();

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
            draw_constant_data.push_back({
                .model = transform,
                .inv_model = invert(transform),
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
    }

    void shutdown() override final
    { }

    void update(const zec::TimeData& time_data) override final
    {
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
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        Scissor scissor{ 0, 0, width, height };

        TextureHandle backbuffer = get_current_back_buffer_handle();
        clear_render_target(backbuffer, clear_color);
        clear_depth_target(depth_target, 1.0f, 0);

        set_active_resource_layout(resource_layout);
        set_pipeline_state(pso_handle);
        set_viewports(&viewport, 1);
        set_scissors(&scissor, 1);

        set_render_targets(&backbuffer, 1, depth_target);

        bind_resource_table(2);
        bind_constant_buffer(view_cb_handle, 1);

        for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
            bind_constant_buffer(draw_data_buffer_handles[i], 0);
            draw_mesh(gltf_context.draw_calls[i].mesh);
        }
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