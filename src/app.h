#pragma once
#include "window.h"
#include "timer.h"
#include "input_manager.h"
#include "gfx/gfx.h"
#include "gfx/ui.h"

namespace zec
{
    class App
    {
    public:
        App(const wchar* app_name);
        virtual ~App();

        virtual i32 run();

        u32 width;
        u32 height;
        Window window;

    protected:
        virtual void init() = 0;
        virtual void shutdown() = 0;

        virtual void update(const TimeData& time_data) = 0;
        virtual void copy() = 0;
        virtual void render() = 0;

        virtual void before_reset() = 0;
        virtual void after_reset() = 0;

        void exit();
        static void window_message_callback(void* context, HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param);

        TimeData time_data = {};
        std::wstring app_name;
        
        input::InputManager input_manager = {};
    private:

        //void parse_command_line(const wchar* cmdLine);

        void init_internal();
        void shutdown_internal();

        void update_internal();
        void render_internal();

        void before_reset_internal();
        void after_reset_internal();

    };
}