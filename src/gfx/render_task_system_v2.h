#pragma once
#include <span>
#include <string>
#include <unordered_map>

#include "../core/zec_types.h"
#include "../core/array.h"
#include "../core/virtual_page_allocator.h"
#include "public_resources.h"
namespace zec::gfx::render_graph
{
   struct ResourceIdentifier
    {
        u32 identifier = UINT32_MAX;

        u32 hash()
        {
            return identifier;
        }
    };

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

        // TODO void handle_resize(u32 width, u32 height);
        const void set_resource_state(const ResourceIdentifier id, const ResourceUsage resource_usage);

        BufferHandle get_buffer(const ResourceIdentifier buffer_identifier) const;
        TextureHandle get_texture(const ResourceIdentifier texture_identifier) const;
        ResourceUsage get_resource_state(const ResourceIdentifier resource_identifier) const;

    private:

        std::unordered_map<ResourceIdentifier, BufferHandle> buffers;
        std::unordered_map<ResourceIdentifier, TextureHandle> textures;
        std::unordered_map< ResourceIdentifier, ResourceUsage> resource_states;
    };

    class ShaderContext
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

    struct SettingsDesc
    {
        u32 identifier = 0;
        std::string_view name = "";
        size_t byte_size = 0;
    };

    class SettingsContext
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
            SettingsEntry& entry = entries.at(settings_id);
            ASSERT(entry.byte_size == sizeof(T))
            *static_cast<T*>(entry.ptr) = data;
        }

        template<typename T>
        const T get_settings(u32 settings_id) const
        {
            ASSERT(entries.count(settings_id) == 1);
            SettingsEntry& entry = entries.at(settings_id);
            ASSERT(entry.byte_size == sizeof(T));
            return *static_cast<T*>(entries.at(settings_id).ptr);
        }

    private:
        VirtualPageAllocator allocator = {};
        std::unordered_map<u32, SettingsEntry> entries = {};
    };


    enum struct PassResourceType : u8
    {
        INVALID = 0,
        BUFFER,
        TEXTURE,
    };

    struct PassResourceUsage
    {
        u32 identifier = UINT32_MAX;
        PassResourceType type = PassResourceType::INVALID;
        ResourceUsage usage = RESOURCE_USAGE_UNUSED;
        std::string_view name = "";
    };
    
    typedef void(*PassExecuteFn)(const ResourceContext* resource_contex, const ShaderContext* shader_context, const SettingsContext* settings_context);

    struct PassDesc
    {
        const std::string_view name = "";
        const CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        const PassExecuteFn execute_fn = nullptr;
        const std::span<ResourceIdentifier const> resource_layout_ids = { };
        const std::span<ResourceIdentifier const> pipeline_ids = { };
        const std::span<PassResourceUsage const> inputs = {};
        const std::span<PassResourceUsage const> outputs = {};
    };

    // TOD: DO we need an "Execution" context?

    /*class RenderTaskList
    {
    public:
        u32 backbuffer_resource_id;
        std::vector<PassTask> render_pass_tasks = {};
        std::vector<PassResourceTransitionDesc> resource_transition_descs = {};
        std::vector<CmdReceipt> receipts = {};
        std::vector<CommandContextHandle> cmd_contexts = {};
        std::vector<PerPassData> per_pass_data = {};
    };*/

    class RenderTaskList
    {
    public:
        RenderTaskList(ResourceContext* resource_context, ShaderContext* shader_context, SettingsContext* settings_context);

        ZecResult add_pass(const PassDesc& render_pass_desc, std::string& out_error_string);

        void execute(/*TaskScheduler& task_scheduler*/);

    private:
        std::vector<PassDesc> pass_descs = {};
        ResourceContext* resource_context = nullptr;
        ShaderContext* shader_context = nullptr;
        SettingsContext* settings_context = nullptr;

    };
}