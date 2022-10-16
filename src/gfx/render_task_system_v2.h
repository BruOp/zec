#pragma once
#include <span>
#include <string>
#include <unordered_map>

#include "../core/zec_types.h"
#include "../core/array.h"
#include "../core/linear_allocator.h"
#include "public_resources.h"

namespace zec::render_graph
{
    struct ResourceIdentifier
    {
        u32 identifier = UINT32_MAX;

        bool operator==(const ResourceIdentifier& other) const = default;
    };

    //bool operator==(const ResourceIdentifier& lhs, const ResourceIdentifier& rhs)
    //{
    //    return lhs.identifier == rhs.identifier;
    //}
}


template<>
struct std::hash<zec::render_graph::ResourceIdentifier>
{
    size_t operator()(const zec::render_graph::ResourceIdentifier& id) const
    {
        return static_cast<size_t>(id.identifier);
    }
};

namespace zec::render_graph
{
    enum struct Sizing : u8
    {
        RELATIVE_TO_SWAP_CHAIN = 0,
        ABSOLUTE,
    };

    // TODO: Also support resizing here,but using a divisor -- to enable for instance resizing tile buffers
    struct BufferResourceDesc
    {
        ResourceIdentifier identifier = {};
        std::string_view name = "";
        ResourceUsage initial_usage = RESOURCE_USAGE_UNUSED;
        BufferDesc desc = {};
    };

    struct TextureResourceDesc
    {
        ResourceIdentifier identifier = {};
        std::string_view name = "";
        // TODO: Re-evaluate whether this data will be managed by ResourceContext at all, and if not, ditch it
        // If sizing == Sizing::RELATIVE_TO_SWAP_CHAIN, then the width and height in the texture_desc are
        // ignored and instead, relative_width_factor * swap_chain.width is used instead. (Same for height)
        Sizing sizing = Sizing::ABSOLUTE;
        // Sizing factors are only used if sizing == Sizing::RELATIVE_TO_SWAP_CHAIN
        float relative_width_factor = 1.0f;
        float relative_height_factor = 1.0f;
        TextureDesc desc = {};
    };

    class ResourceContext
    {
    public:
        void register_buffer(const BufferResourceDesc& buffer_desc);
        void register_texture(const TextureResourceDesc& buffer_desc);

        // Sets an id so that when a pass writes to these resources, they're really just writing to the backbuffer
        void set_backbuffer_id(const ResourceIdentifier backbuffer_id);

        // TODO void handle_resize(u32 width, u32 height);
        const void set_resource_state(const ResourceIdentifier id, const ResourceUsage resource_usage);

        BufferHandle get_buffer(const ResourceIdentifier buffer_identifier) const;
        TextureHandle get_texture(const ResourceIdentifier texture_identifier) const;
        ResourceUsage get_resource_state(const ResourceIdentifier resource_identifier) const;

    private:
        std::unordered_map<ResourceIdentifier, BufferHandle[RENDER_LATENCY]> buffers;
        std::unordered_map<ResourceIdentifier, TextureHandle[RENDER_LATENCY]> textures;
        std::unordered_map< ResourceIdentifier, ResourceUsage> resource_states;
    };

    class ShaderStore
    {
    public:
        void register_resource_layout(const ResourceIdentifier id, const ResourceLayoutHandle handle);
        void register_pipeline(const ResourceIdentifier id, const PipelineStateHandle handle);

        ResourceLayoutHandle get_resource_layout(const ResourceIdentifier id) const;
        PipelineStateHandle get_pipeline(const ResourceIdentifier id) const;

    private:
        std::unordered_map<ResourceIdentifier, ResourceLayoutHandle> resource_layouts;
        std::unordered_map<ResourceIdentifier, PipelineStateHandle> pipelines;
    };

    class PassDataStore
    {
        struct DataEntry
        {
            void* ptr = nullptr;
            const size_t byte_size = 0;
        };
    public:

        template<typename T>
        void emplace(const ResourceIdentifier id, const T& data)
        {
            if (entries.count(id) == 0)
            {
                T* ptr = reinterpret_cast<T*>(allocator.allocate(sizeof(T)));
                *ptr = data;
                entries.insert({ id, { ptr, sizeof(T) }});
            }
            else
            {
                ASSERT_MSG(entries[id].byte_size == sizeof(T), "Size mismatch for setting %s, this setting has \
                    already been registered under the same identifier but with a different byte size");
            }
        }

        template<typename T>
        void set(const ResourceIdentifier id, const T data)
        {
            DataEntry& entry = entries.at(id);
            ASSERT(entry.byte_size == sizeof(T))
            *static_cast<T*>(entry.ptr) = data;
        }

        template<typename T>
        const T& get(const ResourceIdentifier id) const
        {
            const DataEntry& entry = entries.at(id);
            ASSERT(entry.byte_size == sizeof(T));
            return *static_cast<const T*>(entries.at(id).ptr);
        }

    private:
        FixedLinearAllocator<1024> allocator = {};
        std::unordered_map<ResourceIdentifier, DataEntry> entries = {};
    };

    using SettingsStore = PassDataStore;
    using PerPassDataStore = PassDataStore;

    enum struct PassResourceType : u8
    {
        INVALID = 0,
        BUFFER,
        TEXTURE,
    };

    struct PassResourceUsage
    {
        ResourceIdentifier identifier = {};
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
    };

    struct PassExecutionContext
    {
        const ResourceContext* resource_context = nullptr;
        const ShaderStore* shader_context = nullptr;
        const SettingsStore* settings_context = nullptr;
        const PerPassDataStore* per_pass_data_store = nullptr;
        CommandContextHandle cmd_context = {};
    };

    typedef void(*PassSetupFn)(const SettingsStore* settings_context, PerPassDataStore* per_pass_data_store);
    typedef void(*PassExecuteFn)(const PassExecutionContext* execution_context);
    typedef void(*PassTeardownFn)(PerPassDataStore* per_pass_data_store);
    // TODO: On settings change? We'll need to handle settings changes for passes that rely on them to create resources in Setup

    struct PassDesc
    {
        const std::string_view name = "";
        const CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        const PassSetupFn setup_fn = nullptr;
        const PassExecuteFn execute_fn = nullptr;
        const PassTeardownFn teardown_fn = nullptr;
        // TODO: Are these really necessary? Is upfront validation really going to be that useful?
        // TODO: Do we want to validate settings?
        //const std::span<ResourceIdentifier const> resource_layout_ids = { };
        //const std::span<ResourceIdentifier const> pipeline_ids = { };
        const std::span<PassResourceUsage const> inputs = {};
        const std::span<PassResourceUsage const> outputs = {};
    };

    class RenderTaskList
    {
    public:
        RenderTaskList() = default;
        RenderTaskList(ResourceContext* resource_context, ShaderStore* shader_context, SettingsStore* settings_context);
        ~RenderTaskList() = default;

        ZecResult add_pass(const PassDesc& render_pass_desc/*, std::string& out_error_string*/);

        void execute(/*TaskScheduler& task_scheduler*/);

        template<typename T>
        void create_settings(const ResourceIdentifier& id, T&& data)
        {
            settings_context.emplace(id, data);
        };

        template<typename T>
        T get_settings(const ResourceIdentifier settings_id) const
        {
            return settings_context.get(settings_id);
        };

        template<typename T>
        void set_settings(const ResourceIdentifier settings_id, const T data)
        {
            settings_context.set(settings_id, data);
        }


    private:
        std::vector<PassDesc> pass_descs = {};
        // TODO: Our render graph doesn't need to manage resource sizes, that can be a separate system.
        ResourceContext* resource_context = nullptr;
        ShaderStore* shader_context = nullptr;
        SettingsStore settings_context = {};
        PerPassDataStore per_pass_data_store = {};
    };
}
