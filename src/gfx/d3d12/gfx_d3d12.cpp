#include "pch.h"
#include "gfx_d3d12.h"
#include "globals.h"
#include "utils/utils.h"

namespace zec
{

    void initialize_renderer(const Window& window)
    {
        dx12::initialize_renderer();
    };

    void destroy_renderer()
    {
        dx12::destroy_renderer();
    };
}
