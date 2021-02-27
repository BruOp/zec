
#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"

#include "pass_resources.h"
#include "bounding_meshes.h"
#include "renderable_scene.h"

#include "passes/depth_pass.h"
#include "passes/forward_pass.h"
#include "passes/background_pass.h"
#include "passes/tone_mapping_pass.h"
#include "passes/light_binning_pass.h"
#include "passes/cluster_debug_pass.h"
#include "passes/debug_pass.h"

#include "compute_tasks/brdf_lut_creator.h"
#include "compute_tasks/irradiance_map_creator.h"

#include <random>

static constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;
static constexpr size_t MAX_NUM_OBJECTS = 16384;

using namespace zec;

namespace clustered
{
    constexpr float VERTICAL_FOV = deg_to_rad(65.0f);
    constexpr float CAMERA_NEAR = 0.1f;
    constexpr float CAMERA_FAR = 100.0f;

    constexpr u32 CLUSTER_HEIGHT = 16;
    const static ClusterGridSetup CLUSTER_SETUP = {
        .width = 2 * CLUSTER_HEIGHT,
        .height = CLUSTER_HEIGHT,
        .depth = u32(ceilf(log10f(CAMERA_FAR / CAMERA_NEAR) / log10f(1.0f + 2.0f * tanf(0.5f * VERTICAL_FOV) / CLUSTER_HEIGHT))),
    };

    ClusterGridSetup calculate_cluster_grid(u32 pixel_width, u32 pixel_height, u32 num_depth_divisions, u32 tile_size)
    {
        return {
            .width = pixel_width + (tile_size - 1) / tile_size,
            .height = pixel_height + (tile_size - 1) / tile_size,
            .depth = num_depth_divisions
        };
    }
    
    class ClusteredForward : public zec::App
    {
        struct Passes
        {
        public:
            DepthPass depth_prepass = {};
            LightBinningPass light_binning = { CLUSTER_SETUP };
            ClusterDebugPass cluster_debug = { CLUSTER_SETUP };
            ForwardPass forward = {};
            DebugPass debug_pass = {};
            BackgroundPass background = {};
            ToneMappingPass tone_mapping = {};
        };

    public:
        ClusteredForward() : App{ L"Clustered Forward Rendering" } { }

        FPSCameraController camera_controller = {};

        PerspectiveCamera camera;
        PerspectiveCamera debug_camera;
        // Scene data
        Array<SpotLight> spot_lights;

        // Rendering Data
        RenderableScene renderable_scene;
        RenderableCamera renderable_camera;
        Passes render_passes{ };
        render_pass_system::RenderPassList render_list;


    protected:
        void init() override final
        {
            camera = create_camera(float(width) / float(height), VERTICAL_FOV, CAMERA_NEAR, CAMERA_FAR, CAMERA_CREATION_FLAG_REVERSE_Z);
            camera.position = vec3{ 0.0f, 0.0f, -10.0f };

            camera_controller.init();
            camera_controller.set_camera(&camera);

            //ClusterGridSetup cluster_grid_setup = calculate_cluster_grid(width, heigth, 32, tile)

            // Create lights
            {
                constexpr vec3 light_colors[] = {
                    {1.0f, 0.0f, 0.0f },
                    {0.0f, 1.0f, 0.0f },
                    {0.0f, 0.0f, 1.0f },
                    {1.0f, 1.0f, 0.0f },
                    {1.0f, 0.0f, 1.0f },
                    {0.0f, 1.0f, 1.0f },
                    {1.0f, 1.0f, 1.0f },
                    {0.5f, 0.3f, 1.0f },
                    {0.1f, 0.4f, 0.4f },
                };

                std::default_random_engine generator;
                std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
                auto random_generator = std::bind(distribution, generator);
                
                constexpr size_t num_lights = 96;
                for (size_t idx = 0; idx < num_lights; ++idx) {
                    constexpr float scene_width = 25.0f;
                    constexpr float scene_length = 12.0f;
                    constexpr float scene_height = 14.0f;
                    float coeff = float(idx) / float(num_lights - 1);
                    float phase = k_2_pi * coeff;
                    constexpr u32 grid_size_x = 6;
                    constexpr u32 grid_size_y = 4;
                    constexpr u32 grid_size_z = 4;

                    u32 k = idx / (grid_size_x * grid_size_z);
                    u32 j = (idx - k * (grid_size_x * grid_size_y)) / grid_size_x;
                    u32 i = idx - j * grid_size_x - k * (grid_size_x * grid_size_y);
                    vec3 position = {
                        (2.0f * float(i) / float(grid_size_x) - 1.0f) * scene_width,
                        float(j) / float(grid_size_y) * scene_height,
                        (2.0f * float(k) / float(grid_size_z) - 1.0f) * scene_length,
                    };
                    // Set positions in Cartesian
                    spot_lights.push_back({
                        .position = position,
                        .radius = 6.0f,
                        .direction = normalize(vec3{random_generator(), -random_generator(), random_generator()}),
                        .umbra_angle = k_half_pi * 0.25f,
                        .penumbra_angle = k_half_pi * 0.2f,
                        .color = light_colors[i % std::size(light_colors)] * 2.0f,
                        });
                }
            }

            renderable_camera.initialize(L"Main camera view constant buffer", &camera);

            renderable_scene.initialize(RenderableSceneSettings{
                .max_num_entities = 2000,
                .max_num_materials = 2000,
                .max_num_spot_lights = 1024
                });

            CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);

            TextureDesc brdf_lut_desc{
                .width = 256,
                .height = 256,
                .depth = 1,
                .num_mips = 1,
                .array_size = 1,
                .is_cubemap = 0,
                .is_3d = 0,
                .format = BufferFormat::FLOAT_2,
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
                .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
            };
            TextureHandle brdf_lut_map = gfx::textures::create(brdf_lut_desc);
            gfx::set_debug_name(brdf_lut_map, L"BRDF LUT");

            TextureDesc irradiance_desc{
                .width = 16,
                .height = 16,
                .depth = 1,
                .num_mips = 1,
                .array_size = 6,
                .is_cubemap = 1,
                .is_3d = 0,
                .format = BufferFormat::R16G16B16A16_FLOAT,
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
                .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
            };
            TextureHandle irradiance_map = gfx::textures::create(irradiance_desc);
            gfx::set_debug_name(irradiance_map, L"Irradiance Map");

            TextureHandle radiance_map = gfx::textures::create_from_file(copy_ctx, "textures/prefiltered_paper_mill.dds");

            gltf::Context gltf_context{};
            gltf::load_gltf_file("models/Sponza/Sponza.gltf", copy_ctx, gltf_context, gltf::GLTF_LOADING_FLAG_NONE);
            CmdReceipt receipt = gfx::cmd::return_and_execute(&copy_ctx, 1);

            gfx::cmd::gpu_wait(CommandQueueType::GRAPHICS, receipt);

            CommandContextHandle graphics_ctx = gfx::cmd::provision(CommandQueueType::GRAPHICS);

            // Compute BRDF and Irraadiance maps
            {
                BRDFLutCreator brdf_lut_creator{};
                brdf_lut_creator.init();
                brdf_lut_creator.record(graphics_ctx, { .out_texture = brdf_lut_map });

                IrradianceMapCreator irradiance_map_creator{};
                irradiance_map_creator.init();
                irradiance_map_creator.record(graphics_ctx, { .src_texture = radiance_map, .out_texture = irradiance_map });

                ResourceTransitionDesc transition_descs[] = {
                    {
                        .type = ResourceTransitionType::TEXTURE,
                        .texture = irradiance_map,
                        .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                        .after = RESOURCE_USAGE_SHADER_READABLE,
                    },
                    {
                        .type = ResourceTransitionType::TEXTURE,
                        .texture = brdf_lut_map,
                        .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                        .after = RESOURCE_USAGE_SHADER_READABLE,
                    },
                };
                gfx::cmd::transition_resources(graphics_ctx, transition_descs, ARRAY_SIZE(transition_descs));
            }

            receipt = gfx::cmd::return_and_execute(&graphics_ctx, 1);

            // Materials buffer
            for (const auto& material : gltf_context.materials) {
                renderable_scene.renderables.push_material({
                    .base_color_factor = material.base_color_factor,
                    .emissive_factor = material.emissive_factor,
                    .metallic_factor = material.metallic_factor,
                    .roughness_factor = material.roughness_factor,
                    .base_color_texture_idx = material.base_color_texture_idx,
                    .metallic_roughness_texture_idx = material.metallic_roughness_texture_idx,
                    .normal_texture_idx = material.normal_texture_idx,
                    .occlusion_texture_idx = material.occlusion_texture_idx,
                    .emissive_texture_idx = material.emissive_texture_idx,
                    });
            }

            for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
                const auto& draw_call = gltf_context.draw_calls[i];
                renderable_scene.renderables.push_renderable(
                    draw_call.material_index,
                    draw_call.mesh,
                    {
                        .model_transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx],
                        .normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx],
                    });
            }

            render_passes.light_binning.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.light_binning.scene_constants_buffer = renderable_scene.scene_constants;
            render_passes.light_binning.camera = &camera;

            render_passes.depth_prepass.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.depth_prepass.scene_renderables = &renderable_scene.renderables;

            render_passes.cluster_debug.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.cluster_debug.scene_renderables = &renderable_scene.renderables;
            render_passes.cluster_debug.scene_constants_buffer = renderable_scene.scene_constants;
            render_passes.cluster_debug.camera = &camera;

            render_passes.debug_pass.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.debug_pass.renderable_scene = &renderable_scene;

            render_passes.forward.camera = &camera;
            render_passes.forward.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.forward.scene_renderables = &renderable_scene.renderables;
            render_passes.forward.scene_constants_buffer = renderable_scene.scene_constants;
            render_passes.forward.cluster_grid_setup = CLUSTER_SETUP;

            render_passes.background.view_cb_handle = renderable_camera.view_constant_buffer;
            render_passes.background.cube_map_buffer = radiance_map;

            render_pass_system::IRenderPass* pass_ptrs[] = {
                &render_passes.depth_prepass,
                &render_passes.light_binning,
                //&render_passes.cluster_debug,
                &render_passes.forward,
                &render_passes.background,
                &render_passes.tone_mapping,
                &render_passes.debug_pass,
            };
            render_pass_system::RenderPassListDesc render_list_desc = {
                .render_passes = pass_ptrs,
                .num_render_passes = std::size(pass_ptrs),
                .resource_to_use_as_backbuffer = PassResources::SDR_TARGET.id,
            };

            render_list.compile(render_list_desc);
            render_list.setup();

            gfx::cmd::cpu_wait(receipt);

        }

        void shutdown() override final
        {
            render_list.shutdown();
        }

        void update(const zec::TimeData& time_data) override final
        {
            camera_controller.update(time_data.delta_seconds_f);
        }

        void copy() override final
        {
            renderable_camera.copy();

            renderable_scene.copy(spot_lights);

            render_list.copy();
        }

        void render() override final
        {
            render_list.execute();
        }

        void before_reset() override final
        { }

        void after_reset() override final
        { }
    };

}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef DEBUG
    int res = 0;
    _CrtMemState s1, s2, s3;
    _CrtMemCheckpoint(&s1);
    {
        clustered::ClusteredForward  app{};
        res = app.run();
    }
    _CrtMemCheckpoint(&s2);


    if (_CrtMemDifference(&s3, &s1, &s2))
        _CrtMemDumpStatistics(&s3);
    _CrtDumpMemoryLeaks();
    return res;
#else
    clustered::ClusteredForward app{};
    return app.run();
#endif // DEBUG
}
