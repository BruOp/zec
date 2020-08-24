#pragma once
#include "pch.h"

namespace zec
{
    struct RendererDesc
    {
        u32 width;
        u32 height;
        HWND window;
        bool fullscreen = false;
        bool vsync = true;
    };

    void init_renderer(const RendererDesc& renderer_desc);
    void destroy_renderer();

    u32 get_swapchain_width();
    u32 get_swapchain_height();

    void start_frame();
    void end_frame();
}