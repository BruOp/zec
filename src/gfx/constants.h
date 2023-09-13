#pragma once
#include "core/zec_types.h"

namespace zec
{
    constexpr u64 RENDER_LATENCY = 2;
    constexpr u64 NUM_BACK_BUFFERS = RENDER_LATENCY;
    constexpr u64 NUM_CMD_ALLOCATORS = RENDER_LATENCY;

    constexpr u64 MAX_NUM_DRAW_VERTEX_BUFFERS = 8;

    constexpr u32 k_invalid_handle = UINT32_MAX;
}