#pragma once
#include "pch.h"
#include "imgui/imgui.h"
#include "window.h"
#include "gfx/public_resources.h"

namespace zec::ui
{

    void initialize(Window& window);
    void destroy();

    void begin_frame();

    // TODO: Switch to using a command context
    void end_frame(const CommandContextHandle context_handle);
}