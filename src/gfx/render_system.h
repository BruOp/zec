#pragma once
#include <string>

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

        enum struct PassAccess : u8
        {
            UNUSED = 0,
            READ = 1 << 0,
            WRITE = 1 << 1,
        };
        enum struct PassResourceType : u8
        {
            INVALID = 0,
            BUFFER,
            TEXTURE,
        };

        struct PassTextureResoruce
        {
            SizeClass size_class = SizeClass::RELATIVE_TO_SWAP_CHAIN;
            BufferFormat format = BufferFormat::R8G8B8A8_UNORM;
            float width = 1.0;
            float height = 1.0;
            u32 depth = 1;
            u32 num_mips = 1;
            u32 array_size = 1;
            u16 is_cubemap = 0;
            u16 is_3d = 0;
        };

        struct PassBufferResoruce
        {
            BufferType type = BufferType::DEFAULT;
            BufferFormat format = BufferFormat::UNKOWN;
            u32 byte_size = 0;
            u32 stride = 0;
        };

        struct PassOutputDesc
        {
            char name[64] = "";
            PassResourceType type = PassResourceType::INVALID;
            ResourceUsage usage = RESOURCE_USAGE_UNUSED;
            union
            {
                TextureDesc texture_desc = {};
                BufferDesc buffer_desc;
            };
        };

        struct PassInputDesc
        {
            char name[64] = "";
            PassResourceType type = PassResourceType::INVALID;
            ResourceUsage usage = RESOURCE_USAGE_UNUSED;
        };

        typedef void(*SetupFn)(void);
        typedef void(*ExecuteFn)(PassResourceList);
        typedef void(*DestroyFn)(void);

        struct RenderPassDesc
        {
            PassInputDesc inputs[8] = {};
            PassOutputDesc outputs[8] = {};

            SetupFn setup = nullptr;
            ExecuteFn execute = nullptr;
            DestroyFn destroy = nullptr;
        };

        struct RenderList
        {
            std::unordered_map<std::string, ResourceListEntry> resource_map = {};
            Array<PassResourceList> resources_per_pass = {};
            Array<SetupFn> setup_fns;
            Array<ExecuteFn> execute_fns;
            Array<DestroyFn> destroy_fns;

            // TODO: Replace with our own map?
        };

        void compile_render_list(RenderList& in_render_list, RenderPassDesc* render_passes, size_t num_render_passes);
        void setup(RenderList& render_list);
        void execute(RenderList& render_list);
        void destroy(RenderList& render_list);
    }
}