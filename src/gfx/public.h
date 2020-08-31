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

    enum BufferUsage : uint8_t
    {
        UNUSED = 0,
        VERTEX = (1 << 0),
        INDEX = (1 << 1),
        SHADER_READABLE = (1 << 2),
        COMPUTE_WRITABLE = (1 << 3),
        CPU_WRITABLE = (1 << 4),
        CONSTANT = (1 << 5),

    };

    //RESOURCE_HANDLE(TextureHandle);

    RESOURCE_HANDLE(BufferHandle);

    enum struct BufferUsage : u8
    {
        VERTEX = 0,
        INDEX,
        CONSTANT,
    };

    struct BufferDesc
    {
        BufferType type = BufferType::CONSTANT;
        ResourceUsage usage = ResourceUsage::STATIC;
        u32 width = 0;
        u32 height = 0;
        u32 depth = 0;

    };

}
