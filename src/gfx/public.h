#pragma once
#include "pch.h"

namespace zec
{
    constexpr u32 k_invalid_handle = UINT32_MAX;

    // Copied from BGFX and others
#define RESOURCE_HANDLE(_name)      \
	struct _name { u32 idx = k_invalid_handle; }; \
    inline bool is_valid(_name _handle) { return _handle.idx != zec::k_invalid_handle; };

#define INVALID_HANDLE \
    { zec::k_invalid_handle }

    struct RendererDesc
    {
        u32 width;
        u32 height;
        HWND window;
        bool fullscreen = false;
        bool vsync = true;
    };

    //RESOURCE_HANDLE(TextureHandle);
    //RESOURCE_HANDLE(BufferHandle);
}
