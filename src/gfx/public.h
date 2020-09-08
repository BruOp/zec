#pragma once
#include "pch.h"
#include "constants.h"

namespace zec
{
    constexpr u32 k_invalid_handle = UINT32_MAX;

    // Copied from BGFX and others
#define RESOURCE_HANDLE(_name)      \
	struct _name { u32 idx = k_invalid_handle; }; \
    inline bool is_valid(_name _handle) { return _handle.idx != zec::k_invalid_handle; };

#define INVALID_HANDLE \
    { zec::k_invalid_handle }

    // ---------- Enums ----------
    enum BufferUsage : u16
    {
        UNUSED = 0,
        VERTEX = (1 << 0),
        INDEX = (1 << 1),
        CONSTANT = (1 << 2),
        SHADER_READABLE = (1 << 3),
        //COMPUTE_WRITABLE = (1 << 4),
        DYNAMIC = (1 << 5),
    };

    enum MeshAttribute : u16
    {
        INVALID = 0,
        POSITION = (1 << 0),
        NORMAL = (1 << 1),
        TEXCOORD = (1 << 2),
        COLOR = (1 << 3),
        BLENDINDICES = (1 << 4),
        BLENDWEIGHTS = (1 << 5)
    };

    enum struct BufferType : u16
    {
        DEFAULT = 0,
        TYPED,
        STRUCTURED,
        RAW
    };

    enum struct BufferFormat : u16
    {
        INVALID = 0,
        UINT16,
        UINT32,
        UNORM8_2,
        UNORM16_2,
        FLOAT_2,
        FLOAT_3,
        UINT8_4,
        UNORM8_4,
        UINT16_4,
        UNORM16_4,
        FLOAT_4,

        // TODO: STRUCTURED?
    };

    // ---------- Creation Descriptors ---------- 
    struct RendererDesc
    {
        u32 width = 0;
        u32 height = 0;
        HWND window = {};
        bool fullscreen = false;
        bool vsync = true;
    };

    struct BufferDesc
    {
        u16 usage = BufferUsage::UNUSED;
        BufferType type = BufferType::DEFAULT;
        u32 byte_size = 0;
        u32 stride = 0;
        void* data = nullptr;
    };

    struct InputAssemblyDesc
    {
        struct InputElementDesc
        {
            MeshAttribute attribute_type = MeshAttribute::INVALID;
            u32 semantic_index = 0;
            BufferFormat format = BufferFormat::INVALID;
            u32 input_slot = 0;
            u32 aligned_byte_offset = 0;
            // TODO: Support instancing?
            // input_slot_class (used to distinguish between per vertex data and per instance data)
            // instance_data_step_rate (i'm not sure what this is for just yet)
        };

        InputElementDesc elements[MAX_NUM_MESH_VERTEX_BUFFERS];
    };

    struct MeshDesc
    {
        BufferDesc index_buffer_desc;
        BufferDesc vertex_buffer_descs[MAX_NUM_MESH_VERTEX_BUFFERS];
    };

    // ---------- Handles ----------
    RESOURCE_HANDLE(BufferHandle);
    RESOURCE_HANDLE(MeshHandle);

}
