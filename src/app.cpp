#include "pch.h"
#include "app.h"
#include "gfx/gfx.h"
// TODO: Remove this
#include "gfx/d3d12/globals.h"

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
            height }
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

    void App::on_window_resize(void* context, HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param)
    { }

    void App::init_internal()
    {
        init_time_data(time_data);
        window.show(true);

        RendererDesc renderer_desc{ };
        renderer_desc.width = width;
        renderer_desc.height = height;
        renderer_desc.fullscreen = false;
        renderer_desc.vsync = true;
        renderer_desc.window = window.hwnd;
        gfx::init_renderer(renderer_desc);

        ui::initialize(window);
        input::initialize(width, height);
        init();
    }

    void App::shutdown_internal()
    {
        gfx::wait_for_gpu();
        ui::destroy();
        shutdown();
        gfx::destroy_renderer();
    }

    void App::update_internal()
    {
        input::update(time_data);
        update_time_data(time_data);

        update(time_data);
    }

    void App::render_internal()
    {
        render();
    }

    void App::before_reset_internal()
    { }

    void App::after_reset_internal()
    { }

}
