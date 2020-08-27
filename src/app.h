#pragma once
#include "pch.h"
#include "window.h"
#include "timer.h"

#include "gfx/gfx.h"

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
        virtual void render() = 0;

        virtual void before_reset() = 0;
        virtual void after_reset() = 0;

        void exit();
        void on_window_resize(void* context, HWND hWnd, UINT msg, WPARAM w_param, LPARAM l_param);

        TimeData time_data = {};
        std::wstring app_name;
        D3D_FEATURE_LEVEL min_feature_level = D3D_FEATURE_LEVEL_12_0;

    private:

        //void parse_command_line(const wchar* cmdLine);

        void init_internal();
        void shutdown_internal();

        void update_internal();
        void render_internal();

        void before_reset_internal();
        void after_reset_internal();

        //void CreatePSOs_Internal();
        //void DestroyPSOs_Internal();

        //void DrawLog();
    };
}