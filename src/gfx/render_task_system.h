#pragma once
#include <span>
#include <unordered_map>

#include "core/virtual_page_allocator.h"
#include "gfx/public_resources.h"
#include "gfx/gfx.h"

namespace zec
{
    class TaskScheduler;

    template<typename T>
    struct RenderPassResource
    {
        u32 identifier = UINT32_MAX;

        u32 hash()
        {
            return std::hash(identifier);
        }
    };

    using ResourceLayoutIdentifier = RenderPassResource<ResourceLayoutHandle>;
    using PipelineStateIdentifier = RenderPassResource<PipelineStateHandle>;

    enum struct Sizing : u8
    {
        RELATIVE_TO_SWAP_CHAIN = 0,
        ABSOLUTE,
    };

    enum struct PassResourceType : u8
    {
        INVALID = 0,
        BUFFER,
        TEXTURE,
    };

    struct RenderPassResourceUsage
    {
        u32 identifier = UINT32_MAX;
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
        std::string_view name = "";
    };

    struct ResourceState
    {
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage last_usages[RENDER_LATENCY] = { RESOURCE_USAGE_UNUSED, RESOURCE_USAGE_UNUSED };

        union
        {
            TextureHandle textures[RENDER_LATENCY] = {};
            BufferHandle buffers[RENDER_LATENCY];
        };
    };

    struct PassResourceTransitionDesc
    {
        u32 resource_id;
        ResourceTransitionType type = ResourceTransitionType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    class ResourceMap
    {
    public:
        ResourceState& operator[](const u32 resource_id)
        {
            return internal_map[resource_id];
        }

        ResourceState& at(const u32 resource_id)
        {
            return internal_map.at(resource_id);
        }
        const ResourceState& at(const u32 resource_id) const
        {
            return internal_map.at(resource_id);
        }

        BufferHandle get_buffer_resource(const u32 resource_id) const;
        TextureHandle get_texture_resource(const u32 resource_id) const;

        void set_backbuffer_resource(const u32 backbuffer_identifier);

    private:
        std::unordered_map<u32, ResourceState> internal_map;
    };

    struct BufferPassResourceDesc
    {
        u32 identifier = UINT32_MAX;
        std::string_view name = "";
        ResourceUsage initial_usage = RESOURCE_USAGE_UNUSED;
        BufferDesc desc = {};
    };

    struct TexturePassResourceDesc
    {
        u32 identifier = UINT32_MAX;
        std::string_view name = "";
        // If sizing == Sizing::RELATIVE_TO_SWAP_CHAIN, then the width and height in the texture_desc are
        // ignored and instead, relative_width_factor * swap_chain.width is used instead. (Same for height)
        Sizing sizing = Sizing::ABSOLUTE;
        // Sizing factors are only used if sizing == Sizing::RELATIVE_TO_SWAP_CHAIN
        float relative_width_factor = 1.0f;
        float relative_height_factor = 1.0f;
        TextureDesc desc = {};
    };

    struct SettingsDesc
    {
        u32 identifier = 0;
        std::string_view name = "";
        size_t byte_size = 0;
    };

    class SettingsData
    {
        struct SettingsEntry
        {
            void* ptr = nullptr;
            const size_t byte_size = 0;
        };
    public:
        // Returns false if the settings already exist
        bool create_settings(const SettingsDesc& desc)
        {
            bool needs_creation = entries.count(desc.identifier) == 0;
            if (needs_creation) {
                void* ptr = allocator.allocate(desc.byte_size);
                entries.insert({ desc.identifier, { ptr, desc.byte_size } });
                memset(ptr, 0, desc.byte_size);
            }
            else {
                ASSERT_MSG(entries[desc.identifier].byte_size == desc.byte_size, "Size mismatch for setting %s, this setting has \
                    already been registered under the same name but with a different byte size", desc.name);
            }
            return needs_creation;
        }

        template<typename T>
        void set_settings(u32 settings_id, const T data)
        {
            ASSERT(entries.count(settings_id) == 1);
            *static_cast<T*>(entries.at(settings_id).ptr) = data;
        }

        template<typename T>
        const T get_settings(u32 settings_id) const
        {
            return *static_cast<T*>(entries.at(settings_id).ptr);
        }

    private:
        VirtualPageAllocator allocator = {};
        std::unordered_map<u32, SettingsEntry> entries = {};

    };

    // 256 bytes for each pass to use as it wishes.
    struct PerPassData
    {
        float data[64];
    };

    struct RenderPassContext;

    typedef void(*RenderPassSetupFn)(PerPassData* per_pass_data);
    typedef void(*RenderPassExecuteFn)(const RenderPassContext* context);
    typedef void(*RenderPassTeardownFn)(PerPassData* per_pass_data);

    struct RenderPassContext
    {
        std::string_view name = {};
        RenderPassExecuteFn execute_fn = nullptr;
        ResourceMap* resource_map = nullptr;
        SettingsData* settings = nullptr;
        std::unordered_map<u32, ResourceLayoutHandle>* resource_layouts = nullptr;
        std::unordered_map<u32, PipelineStateHandle>* pipeline_states = nullptr;
        CommandContextHandle cmd_context = {};
        PerPassData* per_pass_data;
    };

    struct RenderPassTaskDesc
    {
        const std::string_view name = "";
        const CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        const RenderPassSetupFn setup_fn = nullptr;
        const RenderPassExecuteFn execute_fn = nullptr;
        const RenderPassTeardownFn teardown_fn = nullptr;
        const std::span<ResourceLayoutIdentifier const> resource_layout_ids = { };
        const std::span<PipelineStateIdentifier const> pipeline_ids = { };
        const std::span<SettingsDesc const> settings = {};
        const std::span<RenderPassResourceUsage const> inputs = {};
        const std::span<RenderPassResourceUsage const> outputs = {};
    };

    RESOURCE_HANDLE(RenderTaskListHandle);

    struct RenderPassTask
    {
        std::string_view name = "";
        RenderPassSetupFn setup_fn = nullptr;
        RenderPassExecuteFn execute_fn = nullptr;
        RenderPassTeardownFn teardown_fn = nullptr;
        CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        ArrayView resource_transitions_view = {};
        u32 receipt_idx_to_wait_on = UINT32_MAX; // Index into the graph's fence list
        bool requires_flush = false;
    };

    struct RenderTaskListDesc
    {
        u32 backbuffer_resource_id = UINT32_MAX;
        std::span<RenderPassTaskDesc> render_pass_task_descs = {};
    };


    class RenderTaskList
    {
    public:
        u32 backbuffer_resource_id;
        std::vector<RenderPassTask> render_pass_tasks = {};
        std::vector<PassResourceTransitionDesc> resource_transition_descs = {};
        std::vector<CmdReceipt> receipts = {};
        std::vector<CommandContextHandle> cmd_contexts = {};
        std::vector<PerPassData> per_pass_data = {};
    };

    class ResourceContext
    {
        // This is going to track our resources, can be shared accross RenderTaskSystems if desired
        // This would let us run tasks that can then be used as part of separate render task systems?
        // Do we actually care about this?

        // But we'd track last used pass
        // Resource layout per task
        // desc per pass

        // If we wanted to alias we'd need to do things in two passes
    };

    class RenderTaskSystem
    {
    public:

        //RenderPassHandle register_render_pass(const RenderPassTaskDesc& render_pass_task_desc);
        void register_buffer_resources(const BufferPassResourceDesc in_resource_descs[], const size_t num_resources);
        void register_texture_resources(const TexturePassResourceDesc in_resource_descs[], const size_t num_resources);
        void register_resource_layout(const ResourceLayoutHandle resource_layout, const ResourceLayoutIdentifier id);
        void register_pipeline_state_object(const PipelineStateHandle pso, const PipelineStateIdentifier id);

        template<typename TSetting>
        void register_settings(const u32 settings_id)
        {
            settings.create_settings<TSetting>(settings_id);
        };

        template<typename TSetting>
        void set_setting(const u32 settings_id, const TSetting data)
        {
            settings.set_settings(settings_id, data);
        }

        RenderTaskListHandle create_render_task_list(const RenderTaskListDesc& desc, std::vector<std::string>& in_errors);

        void complete_setup();

        void execute(const RenderTaskListHandle list, TaskScheduler& task_scheduler);

        ManagedArray<RenderTaskList> render_task_lists = {};
        Array<ResourceState> resource_states;
        Array<u32> swap_chain_relative_resources;

        std::unordered_map<ResourceLayoutIdentifier, ResourceLayoutHandle> resource_layouts;
        std::unordered_map<PipelineStateIdentifier, PipelineStateHandle> pipelines;
        ResourceMap resource_map;
        SettingsData settings;

        std::unordered_map<u32, BufferPassResourceDesc> buffer_resource_descs;
        std::unordered_map<u32, TexturePassResourceDesc> texture_resource_descs;
    };
}