#include <string>
#include <random>
#include <functional>

#include "app.h"

#include "core/zec_math.h"
#include "cpu_tasks.h"
#include "gfx/render_task_system.h"
#include "camera.h"
#include "geometry_helpers.h"
#include "utils/utils.h"

#include "gfx/render_passes/simple_sky_pass.h"
#include "gfx/render_passes/clouds_pass.h"
#include "gfx/render_passes/pass_resources.h"
#include "gfx/render_passes/tone_mapping_pass.h"

static constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;
static constexpr size_t MAX_NUM_OBJECTS = 16384;

using namespace zec;

namespace clouds
{
    constexpr float VERTICAL_FOV = deg_to_rad(65.0f);
    const float TAN_FOV = tanf(0.5f * VERTICAL_FOV);
    constexpr float CAMERA_NEAR = 0.1f;
    constexpr float CAMERA_FAR = 100.0f;

    struct ViewConstantData
    {
        zec::mat4 VP;
        zec::mat4 invVP;
        zec::mat4 view;
        zec::vec3 camera_position;
        float padding[13];
    };

    class CloudsApp : public zec::App
    {

    public:
        CloudsApp() : App{ L"CLOUDS!!" } { }

        FPSCameraController camera_controller = { input_manager };

        PerspectiveCamera camera;
        PerspectiveCamera debug_camera;

        MeshHandle fullscreen_mesh;

        RenderTaskSystem render_task_system = {};
        RenderTaskListHandle complete_task_list = {};

        BufferHandle view_constant_buffer = {};

        TaskScheduler task_scheduler = {};

    protected:
        void init() override final
        {
            task_scheduler.init();

            camera = create_camera(float(width) / float(height), VERTICAL_FOV, CAMERA_NEAR, CAMERA_FAR, CAMERA_CREATION_FLAG_REVERSE_Z);
            camera.position = vec3{ -5.5f, 4.4f, -0.2f };

            camera_controller.init();
            camera_controller.yaw = 0.0f;
            camera_controller.movement_sensitivity = 50.0f;
            camera_controller.set_camera(&camera);

            CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);

            // Create view Constant Buffer
            {
                view_constant_buffer = gfx::buffers::create({
                    .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(ViewConstantData),
                    .stride = 0, });
                gfx::set_debug_name(view_constant_buffer, L"View Constant Buffer");

            }

            // Create fullscreen quad
            {
                MeshDesc fullscreen_desc{
                    .index_buffer_desc = {
                        .usage = RESOURCE_USAGE_INDEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(geometry::k_fullscreen_indices),
                        .stride = sizeof(geometry::k_fullscreen_indices[0]),
                    },
                    .vertex_buffer_descs = {
                        {
                            .usage = RESOURCE_USAGE_VERTEX,
                            .type = BufferType::DEFAULT,
                            .byte_size = sizeof(geometry::k_fullscreen_positions),
                            .stride = 3 * sizeof(geometry::k_fullscreen_positions[0]),
                        },
                        {
                            .usage = RESOURCE_USAGE_VERTEX,
                            .type = BufferType::DEFAULT,
                            .byte_size = sizeof(geometry::k_fullscreen_uvs),
                            .stride = 2 * sizeof(geometry::k_fullscreen_uvs[0]),
                        },
                    },
                    .index_buffer_data = geometry::k_fullscreen_indices,
                    .vertex_buffer_data = {
                        geometry::k_fullscreen_positions,
                        geometry::k_fullscreen_uvs
                    }
                };
                fullscreen_mesh = gfx::meshes::create(copy_ctx, fullscreen_desc);
            }
            
            // Will need to create the various textures for cloud rendering:
            // Low-frequency noise used for cloud shapes
            TextureHandle cloud_shape_tex = gfx::textures::create_from_file(copy_ctx, "textures/cloud_shape.dds");
            
            // High-frequency noise used to "erode" cloud shapes
            TextureHandle cloud_erosion_tex = gfx::textures::create_from_file(copy_ctx, "textures/cloud_erosion.dds");
            
            CmdReceipt receipt = gfx::cmd::return_and_execute(&copy_ctx, 1);
            gfx::cmd::gpu_wait(CommandQueueType::GRAPHICS, receipt);

            TexturePassResourceDesc texture_resources[] = {
                PassResources::DEPTH_TARGET,
                PassResources::HDR_TARGET,
            };
            render_task_system.register_texture_resources(texture_resources, std::size(texture_resources));

            RenderPassTaskDesc render_pass_task_descs[] = {
                    clouds_pass_desc,
                    tone_mapping_pass_desc,
            };
            RenderTaskListDesc task_list_desc = {
                .backbuffer_resource_id = PassResources::SDR_TARGET.identifier,
                .render_pass_task_descs = render_pass_task_descs,
            };
            std::vector<std::string> errors{};
            complete_task_list = render_task_system.create_render_task_list(task_list_desc, errors);


            for (const auto& error : errors) {
                write_log(error.c_str());
            }
            if (errors.size() != 0) {
                throw std::runtime_error("Render Pass Task List was invalid :(");
            };

            render_task_system.complete_setup();
            // TODO: Make validation ensure these have already been set rather than relying on us to define them.
            render_task_system.set_setting(RenderPassSharedSettings::exposure.identifier, 1.0f);
            render_task_system.set_setting(RenderPassSharedSettings::fullscreen_quad.identifier, fullscreen_mesh);
            render_task_system.set_setting(RenderPassSharedSettings::main_pass_view_cb.identifier, view_constant_buffer);
            render_task_system.set_setting(RenderPassSharedSettings::cloud_shape_texture.identifier, cloud_shape_tex);
            render_task_system.set_setting(RenderPassSharedSettings::cloud_erosion_texture.identifier, cloud_erosion_tex);

            gfx::cmd::cpu_wait(receipt);
        }

        void shutdown() override final
        {
            //render_task_system.shutdown();
        }

        void update(const zec::TimeData& time_data) override final
        {
            camera_controller.update(time_data.delta_seconds_f);
        }

        void copy() override final
        {
            ViewConstantData view_constant_data{
                .VP = camera.projection * camera.view,
                .invVP = invert(camera.projection * mat4(to_mat3(camera.view), {})),
                .view = camera.view,
                .camera_position = camera.position,
            };
            gfx::buffers::update(view_constant_buffer, &view_constant_data, sizeof(view_constant_data));
            // Make sure the relevant settings are updated
        }

        void render() override final
        {
            render_task_system.execute(complete_task_list, task_scheduler);
        }

        void before_reset() override final
        { }

        void after_reset() override final
        { }
    };

}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
    int res = 0;
    _CrtMemState s1, s2, s3;
    _CrtMemCheckpoint(&s1);
    {
        clouds::CloudsApp app{};
        res = app.run();
    }
    _CrtMemCheckpoint(&s2);


    if (_CrtMemDifference(&s3, &s1, &s2))
        _CrtMemDumpStatistics(&s3);
    _CrtDumpMemoryLeaks();
    return res;
#else
    clouds::CloudsApp app{};
    return app.run();
#endif // DEBUG
}
