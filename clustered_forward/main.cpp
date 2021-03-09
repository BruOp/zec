#include <string>
#include <random>
#include <functional>

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


static constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;
static constexpr size_t MAX_NUM_OBJECTS = 16384;

using namespace zec;

namespace clustered
{
    constexpr float VERTICAL_FOV = deg_to_rad(65.0f);
    const float TAN_FOV = tanf(0.5f * VERTICAL_FOV);
    constexpr float CAMERA_NEAR = 0.1f;
    constexpr float CAMERA_FAR = 100.0f;

    constexpr u32 CLUSTER_HEIGHT = 16;
    constexpr float CLUSTER_MID_PLANE = 5.0f;
    constexpr u32 PRE_MID_DEPTH = 10;

    const static ClusterGridSetup CLUSTER_SETUP = {
        .width = 2 * CLUSTER_HEIGHT,
        .height = CLUSTER_HEIGHT,
        .pre_mid_depth = PRE_MID_DEPTH,
        .post_mid_depth = u32(ceilf(log10f(CAMERA_FAR / CLUSTER_MID_PLANE) / log10f(1.0f + 2.0f * tanf(0.5f * VERTICAL_FOV) / CLUSTER_HEIGHT))),
        .near_x = 16.0f / 9.0f * TAN_FOV * CAMERA_NEAR,
        .near_y = TAN_FOV * CAMERA_NEAR,
        .near_plane = -CAMERA_NEAR,
        .far_plane = -CAMERA_FAR,
        .mid_plane = -CLUSTER_MID_PLANE,
    };

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

        FPSCameraController camera_controller = { input_manager };

        PerspectiveCamera camera;
        PerspectiveCamera debug_camera;
        // Scene data
        Array<SpotLight> spot_lights;
        Array<PointLight> point_lights;

        // Rendering Data
        RenderableScene renderable_scene;
        RenderableCamera renderable_camera;
        Passes render_passes{ };
        render_pass_system::RenderPassList render_list;


    protected:
        void init() override final
        {
            camera = create_camera(float(width) / float(height), VERTICAL_FOV, CAMERA_NEAR, CAMERA_FAR, CAMERA_CREATION_FLAG_REVERSE_Z);
            camera.position = vec3{ -5.5f, 4.4f, -0.2f };

            camera_controller.init();
            camera_controller.yaw = 0.0f;
            camera_controller.set_camera(&camera);

            //ClusterGridSetup cluster_grid_setup = calculate_cluster_grid(width, heigth, 32, tile)

            renderable_camera.initialize(L"Main camera view constant buffer", &camera);

            renderable_scene.initialize(RenderableSceneSettings{
                .max_num_entities = 2000,
                .max_num_materials = 2000,
                .max_num_spot_lights = 1024,
                .max_num_point_lights = 1024,
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

            renderable_scene.scene_constant_data.brdf_lut_idx = gfx::textures::get_shader_readable_index(brdf_lut_map);
            renderable_scene.scene_constant_data.irradiance_map_idx = gfx::textures::get_shader_readable_index(irradiance_map);
            renderable_scene.scene_constant_data.radiance_map_idx = gfx::textures::get_shader_readable_index(radiance_map);

            gltf::Context gltf_context{};
            gltf::load_gltf_file("models/flight_helmet/FlightHelmet.gltf", copy_ctx, gltf_context, gltf::GLTF_LOADING_FLAG_NONE);
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
                gfx::cmd::transition_resources(graphics_ctx, transition_descs, std::size(transition_descs));
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

            AABB scene_aabb{
                .min = {FLT_MAX, FLT_MAX, FLT_MAX},
                .max = {-FLT_MAX, -FLT_MAX, -FLT_MAX}
            };
            for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
                const auto& draw_call = gltf_context.draw_calls[i];
                const mat4& model_transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx];
                AABB aabb = gltf_context.aabbs[i];

                renderable_scene.renderables.push_renderable(
                    draw_call.material_index,
                    draw_call.mesh,
                    {
                        .model_transform = model_transform,
                        .normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx],
                    },
                    aabb
                );

                vec3 corners[] = {
                            aabb.min,
                            {aabb.max.x, aabb.min.y, aabb.min.z},
                            {aabb.min.x, aabb.max.y, aabb.min.z},
                            {aabb.min.x, aabb.min.y, aabb.max.z},
                            {aabb.min.x, aabb.max.y, aabb.max.z},
                            {aabb.max.x, aabb.min.y, aabb.max.z},
                            {aabb.max.x, aabb.max.y, aabb.min.z},
                            aabb.max,
                };

                // Transform corners
                // This only translates to our OBB if our transform is affine
                for (size_t corner_idx = 0; corner_idx < std::size(corners); corner_idx++) {
                    vec3 corner = (model_transform * corners[corner_idx]).xyz;
                    scene_aabb.min = min(corner, scene_aabb.min);
                    scene_aabb.max = max(corner, scene_aabb.max);
                }
            }

            // Create lights
            {
                constexpr vec3 light_colors[] = {
                    {1.0f, 0.6f, 0.6f },
                    {0.6f, 1.0f, 0.6f },
                    {0.6f, 0.6f, 1.0f },
                    {1.0f, 1.0f, 0.6f },
                    {1.0f, 0.6f, 1.0f },
                    {0.6f, 1.0f, 1.0f },
                    {1.0f, 1.0f, 1.0f },
                    {0.5f, 0.3f, 1.0f },
                    {0.7f, 0.8f, 1.0f },
                };

                std::default_random_engine generator;
                std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
                auto random_generator = std::bind(distribution, generator);

                vec3 scene_dims = 0.5f * (scene_aabb.max - scene_aabb.min);
                vec3 scene_origin = 0.5f * (scene_aabb.max + scene_aabb.min);
                constexpr u32 grid_size_x = 8;
                constexpr u32 grid_size_y = 4;
                constexpr u32 grid_size_z = 4;

                constexpr size_t num_lights = 0; // grid_size_x* grid_size_y* grid_size_z;
                for (size_t idx = 0; idx < num_lights; ++idx) {
                    float coeff = float(idx) / float(num_lights - 1);
                    float phase = k_2_pi * coeff;

                    u32 j = idx / (grid_size_x * grid_size_z);
                    u32 k = (idx - j * (grid_size_z * grid_size_x)) / grid_size_x;
                    u32 i = idx - k * (grid_size_x)-j * (grid_size_z * grid_size_x);
                    vec3 cube_coords = 2.0f * vec3{ float(i) / float(grid_size_x), float(j) / float(grid_size_y), float(k) / float(grid_size_z) } - 1.0f;

                    vec3 position = scene_origin + scene_dims * cube_coords;
                    // Set positions in Cartesian
                    spot_lights.push_back({
                        .position = position,
                        .radius = 5.0f,
                        .direction = normalize(vec3{random_generator(), -1.0f, random_generator()}),
                        .umbra_angle = k_half_pi * 0.25f,
                        .penumbra_angle = k_half_pi * 0.2f,
                        .color = light_colors[idx % std::size(light_colors)] * 3.0f,
                        });
                }

                constexpr size_t num_point_lights = 0;
                for (size_t idx = 0; idx < num_point_lights; ++idx) {
                    vec3 position = scene_origin + scene_dims * vec3{ random_generator(), random_generator(), random_generator() };
                    // Set positions in Cartesian
                    point_lights.push_back({
                        .position = position,
                        .radius = 2.0f,
                        .color = light_colors[idx % std::size(light_colors)] * 1.0f,
                        });
                }
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
                //&render_passes.debug_pass,
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

            renderable_scene.copy(spot_lights, point_lights);

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
