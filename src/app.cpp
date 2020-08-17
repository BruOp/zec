#include "pch.h"
#include "app.h"

namespace zec
{
    App::App(const wchar* app_name) :
        app_name{ app_name },
        window{
            nullptr,
            app_name,
            WS_OVERLAPPEDWINDOW,
            WS_EX_APPWINDOW,
            1600,
            900 }
    {
    }

    App::~App()
    {
        // No-op, but necessary since we want to inherit from this class
    }

    i32 App::run()
    {
        init_internal();
        shutdown_internal();

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
        window.show();

        init();
    }

    void App::shutdown_internal()
    { }

    void App::update_internal()
    {
        update_time_data(time_data);

        update(time_data);
    }

    void App::render_internal()
    { }

    void App::before_reset_internal()
    { }

    void App::after_reset_internal()
    { }

}
