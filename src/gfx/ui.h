#pragma once
#include "pch.h"
#include "imgui/imgui.h"
#include "window.h"

namespace zec::ui
{

    void initialize(Window& window);
    void destroy();

    void begin_frame();

    // TODO: Switch to using a command context
    void end_frame(ID3D12GraphicsCommandList* cmd_list);
}