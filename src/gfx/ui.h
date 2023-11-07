#pragma once
#include "imgui/imgui.h"
#include "gfx/rhi_public_resources.h"

class Window;

namespace zec::rhi
{
    class Renderer;
}

namespace zec::ui
{
    struct UIRenderState;

    class UIRenderer
    {
    public:
        ~UIRenderer();

        void init(rhi::Renderer& renderer, Window& window);
        void destroy();

        void begin_frame();
        void end_frame();
        void draw_frame(rhi::CommandContextHandle context_handle);
    private:
        rhi::Renderer* renderer = nullptr;
        UIRenderState* state = nullptr;
    };
}