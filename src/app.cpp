#include "app.h"
#include "gfx/gfx.h"
#include "gfx/profiling_utils.h"

namespace zec
{
    App::App(const wchar* app_name) :
        app_name{ app_name },
        width{ 1600 },
        height{ 900 },
        window{
            nullptr,
            app_name,
            WS_OVERLAPPEDWINDOW,
            WS_EX_APPWINDOW,
            width,
            height },
            input_manager{ width, height }
    {
    }

    App::~App()
    {
        // No-op, but necessary since we want to inherit from this class
    }

    i32 App::run()
    {
        init_internal();
        after_reset_internal();

        while (window.is_alive()) {
            PROFILE_FRAME("Main Thread");
            if (!window.is_minimized()) {
                update_internal();
                render_internal();
            }

            window.message_loop();
        }

        shutdown_internal();
        return 0;
    }

    void App::exit()
    {
        window.destroy();
    }

    void App::window_message_callback(void* context, HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param)
    {
        App* app = reinterpret_cast<App*>(context);
        ImGuiIO io = ImGui::GetIO();
        if (!(io.WantCaptureMouse || io.WantCaptureKeyboard)) {
            app->input_manager.handle_msg({ hWnd, msg, w_param, l_param });
        }

        if (msg == WM_SIZE) {

            if (w_param != SIZE_MINIMIZED) {
                int width, height;
                app->window.get_client_area(width, height);
                app->input_manager.set_dimensions(width, height);

                gfx::on_window_resize(width, height);
                app->width = width;
                app->height = height;
            }
        }
    }

    void App::init_internal()
    {
        PROFILE_EVENT("App Initialization");

        init_time_data(time_data);
        window.show(true);

        RendererDesc renderer_desc{ };
        renderer_desc.width = width;
        renderer_desc.height = height;
        renderer_desc.fullscreen = false;
        renderer_desc.vsync = true;
        renderer_desc.window = &window;
        gfx::init_renderer(renderer_desc);

        ui::initialize(window);

        window.register_message_callback(window_message_callback, this);

        init();
    }

    void App::shutdown_internal()
    {
        gfx::flush_gpu();
        ui::destroy();
        shutdown();
        gfx::destroy_renderer();
    }

    void App::update_internal()
    {
        PROFILE_EVENT("App Update");

        input_manager.update(time_data);
        update_time_data(time_data);

        update(time_data);
    }

    void App::render_internal()
    {
        gfx::reset_for_frame();
        {
            PROFILE_EVENT("App Copy");
            copy();
        }

        {
            PROFILE_EVENT("App Render");
            render();
        }

        gfx::present_frame();
    }

    void App::before_reset_internal()
    { }

    void App::after_reset_internal()
    { }
}
