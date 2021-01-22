#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"

#include "resource_names.h"
#include "views.h"
#include "passes.h"
#include "scene.h"

using namespace zec;

constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;

namespace clustered
{
    struct PassContexts
    {
        DebugPass::Context debug_context = {};
        UIPass::Context ui_context = {};
    };


    class ClusteredForward : public zec::App
    {
    public:
        ClusteredForward() : App{ L"Clustered Forward Rendering" } { }
        float frame_times[120] = { 0.0f };

        Scene scene = {};

        MeshHandle fullscreen_mesh;

        SceneViewManager scene_view_manager = {};
        SceneViewHandle main_camera_scene_view;

        PassContexts pass_contexts = {};
        RenderSystem::RenderList render_list;

        // Are handles necessary for something like this?
        size_t active_camera_idx;

    protected:
        void init() override final
        {
            float vertical_fov = deg_to_rad(65.0f);
            float camera_near = 0.1f;
            float camera_far = 100.0f;
            {
                PerspectiveCamera camera = create_camera(float(width) / float(height), vertical_fov, camera_near, 10.0f);
                size_t main_camera_idx = scene.perspective_cameras.push_back(camera);
                scene.camera_names.emplace_back("Main camera");
                active_camera_idx = main_camera_idx;

                PerspectiveCamera debug_camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far);
                debug_camera.position = { 0.0f, 0.0f, 10.0f };
                mat4 view = look_at(debug_camera.position, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
                set_camera_view(debug_camera, view);
                size_t debug_camera_idx = scene.perspective_cameras.push_back(debug_camera);
                scene.camera_names.emplace_back("Debug camera");


                FPSCameraController camera_controller{};
                camera_controller.init();
                camera_controller.set_camera(&camera);

                scene_view_manager.set_scene(&scene);
                scene_view_manager.create_new_view(u32(main_camera_idx));
                scene_view_manager.create_new_view(u32(debug_camera_idx));
            }

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

            pass_contexts.ui_context.active_camera_idx = &active_camera_idx;
            pass_contexts.ui_context.scene = &scene;

            RenderSystem::RenderPassDesc render_pass_descs[] = {
                UIPass::generate_desc(&pass_contexts.ui_context),
            };
            RenderSystem::RenderListDesc render_list_desc = {
                .render_pass_descs = render_pass_descs,
                .num_render_passes = ARRAY_SIZE(render_pass_descs),
                .resource_to_use_as_backbuffer = ResourceNames::SDR_BUFFER,
            };

            RenderSystem::compile_render_list(render_list, render_list_desc);
            RenderSystem::setup(render_list);

            CmdReceipt receipt = gfx::end_upload();
            gfx::cmd::cpu_wait(receipt);

        }

        void shutdown() override final
        {
            RenderSystem::destroy(render_list);
        }

        void update(const zec::TimeData& time_data) override final
        {
            //frame_times[gfx::get_current_cpu_frame() % 120] = time_data.delta_milliseconds_f;

            //camera_controller.update(time_data.delta_seconds_f);

            //view_constant_data.VP = camera.projection * camera.view;
            //view_constant_data.invVP = invert(camera.projection * mat4(to_mat3(camera.view), {}));
            //view_constant_data.camera_position = camera.position;
            //view_constant_data.time = time_data.elapsed_seconds_f;
        }

        void copy() override final
        {
            //gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
            RenderSystem::copy(render_list);
        }

        void render() override final
        {
            RenderSystem::execute(render_list);
        }

        void before_reset() override final
        { }

        void after_reset() override final
        { }
    };

}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    _CrtSetBreakAlloc(457);
    int res = 0;
    _CrtMemState s1, s2, s3;
    _CrtMemCheckpoint(&s1);
    _CrtMemDumpStatistics(&s1);
    {
        clustered::ClusteredForward  app{};
        res = app.run();
    }
    _CrtMemCheckpoint(&s2);


    if (_CrtMemDifference(&s3, &s1, &s2))
        _CrtMemDumpStatistics(&s3);
    _CrtDumpMemoryLeaks();
    return res;
}