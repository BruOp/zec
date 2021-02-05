#pragma once
#include "murmur/MurmurHash3.h"
#include "pch.h"
#include "core/array.h"
#include "gfx.h"

namespace zec::render_pass_system
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
        BufferFormat format = BufferFormat::UNKNOWN;
        u32 byte_size = 0;
        u32 stride = 0;
    };

    struct PassOutputDesc
    {
        u64 id;
        const char* name;
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
        u64 id;
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    struct RenderPassList;

    struct ResourceState
    {
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage last_usages[RENDER_LATENCY] = { RESOURCE_USAGE_UNUSED, RESOURCE_USAGE_UNUSED };
        TextureHandle textures[RENDER_LATENCY] = {};
        BufferHandle buffers[RENDER_LATENCY] = {};
    };

    class ResourceMap
    {
    public:
        ResourceState& operator[](const u64 resource_id)
        {
            return internal_map[resource_id];
        }

        ResourceState& at(const u64 resource_id)
        {
            return internal_map.at(resource_id);
        }
        const ResourceState& at(const u64 resource_id) const
        {
            return internal_map.at(resource_id);
        }

        BufferHandle get_buffer_resource(const u64 resource_id) const;
        TextureHandle get_texture_resource(const u64 resource_id) const;

    private:
        std::unordered_map<u64, ResourceState> internal_map;
    };

    // SetupFn returns pointer to internal state for us to store
    typedef void* (*SetupFn)(void* settings);
    typedef void(*CopyFn)(void* settings, void* internal_state);
    typedef void(*ExecuteFn)(const ResourceMap& resource_map, CommandContextHandle cmd_context, void* settings, void* internal_state);
    typedef void* (*DestroyFn)(void* settings, void* internal_state);

    struct RenderPassDesc
    {
        char name[64] = "";
        CommandQueueType queue_type = CommandQueueType::GRAPHICS;
        PassInputDesc inputs[8] = {};
        PassOutputDesc outputs[8] = {};

        void* settings = nullptr;
        SetupFn setup = nullptr;
        CopyFn copy = nullptr;
        ExecuteFn execute = nullptr;
        DestroyFn destroy = nullptr;
    };

    struct RenderPassListDesc
    {
        RenderPassDesc* render_pass_descs;
        u32 num_render_passes;
        u64 resource_to_use_as_backbuffer;
    };

    struct RenderPass
    {
        char name[64] = "";
        CommandQueueType queue_type = CommandQueueType::GRAPHICS;
        ArrayView resource_transitions_view = {};
        u32 receipt_idx_to_wait_on = UINT32_MAX; // Index into the graph's fence list
        bool requires_flush = false;

        void* settings = nullptr;
        void* internal_state = nullptr; // Created during setup
        SetupFn setup = nullptr;
        CopyFn copy = nullptr;
        ExecuteFn execute = nullptr;
        DestroyFn destroy = nullptr;
    };

    struct PassResourceTransitionDesc
    {
        u64 resource_id;
        ResourceTransitionType type = ResourceTransitionType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    struct RenderPassList
    {
        u64 backbuffer_resource_id;

        ResourceMap resource_map;

        // Do I need two seperate lists? Probably not?
        Array<RenderPass> render_passes = {};
        Array<PassResourceTransitionDesc> resource_transition_descs = {};
        Array<CmdReceipt> receipts = {};
        Array<CommandContextHandle> cmd_contexts{ };
    };

    void compile_render_list(RenderPassList& in_render_list, const RenderPassListDesc& render_list_desc);
    void setup(RenderPassList& render_list);
    void copy(RenderPassList& render_list);
    void execute(RenderPassList& render_list);
    void destroy(RenderPassList& render_list);

}
