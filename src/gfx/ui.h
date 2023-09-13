#pragma once
#include "imgui/imgui.h"
#include "gfx/rhi_public_resources.h"

class Window;

namespace zec::ui
{

    void initialize(Window& window);
    void destroy();

    void begin_frame();
    void end_frame();
    void draw_frame(rhi::CommandContextHandle context_handle);
}