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
            READ_WRITE = READ & WRITE,
        };
        enum struct PassResourceType : u8
        {
            INVALID = 0,
            BUFFER,
            TEXTURE,
        };

        struct PassResourceDesc
        {
            std::string name = "";
            PassResourceType type = PassResourceType::INVALID;
            ResourceUsage usage = RESOURCE_USAGE_UNUSED;
            //bool needs_ping_pong = false; // For internal use...
            // Only fill out the descs for WRITE resources
            union
            {
                TextureDesc texture_desc;
                BufferDesc buffer_desc;
            };
        };

        struct ResourceListEntry
        {
            PassResourceType type = PassResourceType::INVALID;
            union
            {
                BufferHandle buffer = INVALID_HANDLE;
                TextureHandle texture;
            };
        };

        typedef void(*SetupFn)(void);
        typedef void(*ExecuteFn)(ResourceListEntry[8]);
        typedef void(*DestroyFn)(void);

        struct RenderPassDesc
        {
            PassResourceDesc resources[8] = {};

            SetupFn setup = nullptr;
            ExecuteFn execute = nullptr;
            DestroyFn destroy = nullptr;
        };

        struct RenderPass
        {
            ResourceListEntry resources[8];
        };

        struct RenderListResources
        { };

        struct RenderList
        {
            std::unordered_map<std::string, ResourceListEntry> resource_map = {};
            Array<ResourceListEntry[8]> resources_per_pass = {};
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