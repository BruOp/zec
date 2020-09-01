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

    enum BufferUsage : u32
    {
        UNUSED = 0,
        VERTEX = (1 << 0),
        INDEX = (1 << 1),
        SHADER_READABLE = (1 << 2),
        COMPUTE_WRITABLE = (1 << 3),
        CPU_WRITABLE = (1 << 4),
        CONSTANT = (1 << 5),

    };

    RESOURCE_HANDLE(BufferHandle);

    struct BufferDesc
    {
        BufferUsage usage = BufferUsage::UNUSED;
        u32 byte_size = 0;
        void* data = nullptr;

    };

    BufferHandle create_buffer(const BufferDesc& desc);
}
