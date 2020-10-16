#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx.h"

namespace zec
{
    namespace RenderSystem
    {
        enum struct SizeClass : u8
        {
            RELATIVE_TO_SWAP_CHAIN = 0,
            ABSOLUTE,
        };

        enum struct PassResourceType : u8
        {
            Buffer = 0,
            Texture,
        };

        struct InputDesc
        {
            char name[32] = "";
            ResourceUsage usage = RESOURCE_USAGE_UNUSED;
        };

        struct OutputDesc
        {
            char name[32] = "";
            ResourceUsage usage = RESOURCE_USAGE_UNUSED;
            BufferFormat format = BufferFormat::FLOAT_4;
            PassResourceType type = PassResourceType::Buffer;
            SizeClass size_class = SizeClass::RELATIVE_TO_SWAP_CHAIN;
            float width = 1.0f;
            float height = 1.0f;
            u32 depth = 1;
            u32 array_size = 1;
            u16 is_cubemap = 0;
            u16 is_3d = 0;
        };

        struct RenderList;

        struct RenderPassDesc
        {
            InputDesc inputs[8];
            OutputDesc outputs[8];

            std::function<void()> setup;
            std::function<void(u32 inputs[8], u32 outputs[8])> execute;
            std::function<void()> destroy;
        };

        struct RenderList
        {
            Array<RenderPassDesc> render_passes = {};
            Array<BufferHandle> buffers = {};
            Array<TextureHandle> textures = {};
            // TODO: Replace with our own map?
            std::unordered_map<char[32], u32> resource_map = {};
        };

        void compile_render_list(RenderList& in_render_list, RenderPassDesc* render_passes, size_t num_render_passes);
        void setup(RenderList& render_list);
        void execute(RenderList& render_list);
        void destroy(RenderList& render_list);
    }
}