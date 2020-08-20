#include "pch.h"
#include "gfx.h"
#include "utils/utils.h"
#include "utils/exceptions.h"

namespace zec
{
    void init_renderer(Renderer& renderer, D3D_FEATURE_LEVEL min_feature_level)
    {
        init(renderer.device, min_feature_level);
        // Set current frame index and reset command allocator and list appropriately
        renderer.current_frame_idx = renderer.current_cpu_frame % NUM_BACK_BUFFERS;

    }

    void destroy(Renderer& renderer)
    {
        destroy(renderer.device);
    }
}
