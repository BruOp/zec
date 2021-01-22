#pragma once
#include <string>

#include "pch.h"
#include "core/array.h"
#include "gfx.h"

namespace zec::RenderSystem
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

    struct PassTextureResource
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

    struct PassBufferResource
    {
        BufferType type = BufferType::DEFAULT;
        BufferFormat format = BufferFormat::UNKOWN;
        u32 byte_size = 0;
        u32 stride = 0;
    };

    struct PassOutputDesc
    {
        std::string name = "";
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
        union
        {
            PassTextureResource texture_desc = {};
            PassBufferResource buffer_desc;
        };
    };

    struct PassInputDesc
    {
        std::string name = "";
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    struct RenderList;

    typedef void(*SetupFn)(void* context);
    typedef void(*CopyFn)(void* context);
    typedef void(*ExecuteFn)(RenderList& render_list, CommandContextHandle cmd_context, void* context);
    typedef void(*DestroyFn)(void* context);

    struct RenderPassDesc
    {
        char name[64] = "";
        CommandQueueType queue_type = CommandQueueType::GRAPHICS;
        PassInputDesc inputs[8] = {};
        PassOutputDesc outputs[8] = {};

        void* context = nullptr;
        SetupFn setup = nullptr;
        CopyFn copy = nullptr;
        ExecuteFn execute = nullptr;
        DestroyFn destroy = nullptr;
    };

    struct RenderListDesc
    {
        RenderPassDesc* render_pass_descs;
        u32 num_render_passes;
        std::string resource_to_use_as_backbuffer;
    };

    struct RenderPass
    {
        char name[64] = "";
        CommandQueueType queue_type = CommandQueueType::GRAPHICS;
        ArrayView resource_transitions_view = {};
        u32 receipt_idx_to_wait_on = UINT32_MAX; // Index into the graph's fence list
        bool requires_flush = false;

        void* context = nullptr;
        SetupFn setup = nullptr;
        CopyFn copy = nullptr;
        ExecuteFn execute = nullptr;
        DestroyFn destroy = nullptr;
    };

    struct PassResourceTransitionDesc
    {
        char name[64] = "";
        ResourceTransitionType type = ResourceTransitionType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    struct ResourceState
    {
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage last_usages[RENDER_LATENCY] = { RESOURCE_USAGE_UNUSED, RESOURCE_USAGE_UNUSED };
        TextureHandle textures[RENDER_LATENCY] = {};
        BufferHandle buffers[RENDER_LATENCY] = {};
    };

    struct RenderList
    {
        std::string backbuffer_resource_name = "";

        // TODO: Replace with our own map? And get rid of std::string?
        std::unordered_map<std::string, ResourceState> resource_map = {};

        // Do I need two seperate lists? Probably not?
        Array<RenderPass> render_passes = {};
        Array<PassResourceTransitionDesc> resource_transition_descs = {};
        Array<CmdReceipt> receipts = {};
        Array<CommandContextHandle> cmd_contexts{ };
    };

    void compile_render_list(RenderList& in_render_list, const RenderListDesc& render_list_desc);
    void setup(RenderList& render_list);
    void copy(RenderList& render_list);
    void execute(RenderList& render_list);
    void destroy(RenderList& render_list);

    BufferHandle get_buffer_resource(RenderList& render_list, const std::string& resource_name);
    TextureHandle get_texture_resource(RenderList& render_list, const std::string& resource_name);
}
