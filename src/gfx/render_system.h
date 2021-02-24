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

        // Used as optimized clear values when creating for render targets
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        // Used as the optimized clear alue when creating a depth stencil target
        float clear_depth;
        u8 clear_stencil;
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
        u64 id = 0;
        const char* name = nullptr;
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

    class IRenderPass
    {
    public:
        struct InputList
        {
            PassInputDesc const* ptr;
            u32 count;
        };

        struct OutputList
        {
            PassOutputDesc const* ptr;
            u32 count;
        };

        virtual void setup() = 0;
        virtual void copy() = 0;
        virtual void record(const ResourceMap& resource_map, CommandContextHandle cmd_context) = 0;
        virtual void shutdown() = 0;

        virtual const char* get_name() const = 0;
        virtual const CommandQueueType get_queue_type() const = 0;
        virtual const InputList get_input_list() const = 0;
        virtual const OutputList get_output_list() const = 0;
    };

    struct RenderPassListDesc
    {
        IRenderPass** render_passes;
        u32 num_render_passes;
        u64 resource_to_use_as_backbuffer;
    };

    struct PassResourceTransitionDesc
    {
        u64 resource_id;
        ResourceTransitionType type = ResourceTransitionType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    class RenderPassList
    {
        struct PerPassSubmissionInfo
        {
            ArrayView resource_transitions_view = {};
            u32 receipt_idx_to_wait_on = UINT32_MAX; // Index into the graph's fence list
            bool requires_flush = false;
        };
    public:
        void compile(RenderPassListDesc& render_list_desc);

        void setup();
        void copy();
        void execute();
        void shutdown();

    private:
        u64 backbuffer_resource_id;

        ResourceMap resource_map;

        // Do I need two seperate lists? Probably not?
        Array<IRenderPass*> render_passes = {};
        Array<PerPassSubmissionInfo> per_pass_submission_info = {};
        Array<PassResourceTransitionDesc> resource_transition_descs = {};
        Array<CmdReceipt> receipts = {};
        Array<CommandContextHandle> cmd_contexts{};
    };


}
