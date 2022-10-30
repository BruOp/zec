#pragma once
#include "imgui/imgui.h"
#include "window.h"
#include "gfx/public_resources.h"

namespace zec::ui
{

    void initialize(Window& window);
    void destroy();

    void begin_frame();
    void end_frame();
    void draw_frame(CommandContextHandle context_handle);
}