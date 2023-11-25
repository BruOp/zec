#pragma once
#include <d3d12.h>

#include "gfx/rhi.h"

// Pix for Windows
#define USE_PIX
#include "pix3.h"

#include "optick/optick.h"

#define ZEC_CONCAT_IMPL(x, y) x##y
#define ZEC_CONCAT(x, y) ZEC_CONCAT_IMPL(x, y)

#ifdef USE_D3D_RENDERER

namespace zec
{
    class ScopedPixCPUEvent
    {
    public:
        ScopedPixCPUEvent(char const* description)
        {
            PIXBeginEvent(0, description);
        };

        ~ScopedPixCPUEvent()
        {
            PIXEndEvent();
        };
    };

#ifndef PROFILE_FRAME
#define PROFILE_FRAME(NAME) OPTICK_FRAME(NAME);
#endif // PROFILE_FRAME

#ifndef PROFILE_EVENT
#define PROFILE_EVENT(NAME) ::zec::ScopedPixCPUEvent ZEC_CONCAT(pix_event_, __LINE__)(NAME); OPTICK_EVENT(NAME);
#endif // PROFILE_EVENT


}

#endif // USE_D3D_RENDERER