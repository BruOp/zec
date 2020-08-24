#include "pch.h"

#define USE_D3D_RENDERER

#ifdef USE_D3D_RENDERER

#include "gfx/public.h"
#include "globals.h"
#include "utils/utils.h"

namespace zec
{
    void init_renderer(const RendererDesc& renderer_desc)
    {
        dx12::init_renderer(renderer_desc);
    };

    void destroy_renderer()
    {
        dx12::destroy_renderer();
    }

    u32 get_swapchain_width()
    {
        return dx12::swap_chain.width;
    }

    u32 get_swapchain_height()
    {
        return dx12::swap_chain.height;
    }

    void start_frame()
    {
        dx12::start_frame();
    }

    void end_frame()
    {
        dx12::end_frame();
    }
}

#endif // USE_D3D_RENDERER