#pragma once
#include <span>
#include <string>
#include <unordered_map>

#include "../core/zec_types.h"
#include "../core/array.h"
#include "../core/allocators.hpp"
#include "rhi.h"
#include "pipeline_store.h"
#include "render_pass_resources.h"

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
        std::wstring_view name = L"";
        rhi::ResourceUsage initial_usage = rhi::RESOURCE_USAGE_UNUSED;
        rhi::BufferDesc desc = {};
    };

    struct TextureResourceDesc
    {
        ResourceIdentifier identifier = {};
        std::wstring_view name = L"";
        // TODO: Re-evaluate whether this data will be managed by ResourceContext at all, and if not, ditch it
        // If sizing == Sizing::RELATIVE_TO_SWAP_CHAIN, then the width and height in the texture_desc are
        // ignored and instead, relative_width_factor * swap_chain.width is used instead. (Same for height)
        Sizing sizing = Sizing::ABSOLUTE;
        // Sizing factors are only used if sizing == Sizing::RELATIVE_TO_SWAP_CHAIN
        float relative_width_factor = 1.0f;
        float relative_height_factor = 1.0f;
        rhi::TextureDesc desc = {};
    };

    // Tracks the state that the resource is in during the execution of the render graph
    struct BarrierState
    {
        rhi::ResourceUsage resource_usage;
        rhi::CommandQueueType queue_type;
    };


    // TODO: Should _all_ resources go through this context? Even the creation of e.g. transient constant buffers?
    // TODO: Should we separate resources managed by the render graph and resources that are updated but the client (like e.g. light buffers)?
    class ResourceManager
    {
    public:
        ResourceManager() = default;
        ResourceManager(const rhi::RenderConfigState& render_config_state) : render_config_state{ render_config_state } {};

        void register_buffer(const BufferResourceDesc& buffer_desc);
        void register_texture(const TextureResourceDesc& texture_desc);

        // Sets an id so that when a pass writes to these resources, they're really just writing to the backbuffer
        void set_backbuffer_id(const ResourceIdentifier id);
        void set_barrier_state(const ResourceIdentifier id, const BarrierState barrier_state);
        // TODO handle resize
        void set_render_config_state(const rhi::RenderConfigState& config_state) { render_config_state = config_state; };

        rhi::BufferHandle get_buffer(const ResourceIdentifier buffer_identifier) const;
        rhi::TextureHandle get_texture(const ResourceIdentifier texture_identifier) const;
        BarrierState get_barrier_state(const ResourceIdentifier resource_identifier) const;
        ResourceIdentifier get_backbuffer_id();

        bool has_buffer(const ResourceIdentifier id) const { return resource_states.contains(id); };
        bool has_texture(const ResourceIdentifier id) const { return resource_states.contains(id); };

        void refresh_backbuffer();
    private:
        // Tracks the state that the resource is in during the execution of the render graph
        struct ResourceState
        {
            rhi::BufferHandle buffer;
            rhi::TextureHandle texture;
            rhi::ResourceUsage resource_usage;
            rhi::CommandQueueType queue_type;
        };

        rhi::Renderer* prenderer = nullptr;
        ResourceIdentifier backbuffer_id = {};
        rhi::RenderConfigState render_config_state;
        std::unordered_map< ResourceIdentifier, ResourceState[RENDER_LATENCY]> resource_states;
    };

    class PassDataStore
    {
        struct DataEntry
        {
            void* ptr = nullptr;
            const size_t byte_size = 0;
        };
    public:
        void init()
        {
            allocator.init(kallocator_byte_size);
        }

        void shutdown()
        {
            allocator.shutdown();
        }

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
            ASSERT_MSG(entries.contains(id), "This per pass data has not been registered");
            DataEntry& entry = entries.at(id);
            ASSERT(entry.byte_size == sizeof(T));
            *static_cast<T*>(entry.ptr) = data;
        }

        template<typename T>
        const T& get(const ResourceIdentifier id) const
        {
            ASSERT_MSG(entries.contains(id), "This per pass data has not been registered");
            const DataEntry& entry = entries.at(id);
            ASSERT(entry.byte_size == sizeof(T));
            return *static_cast<const T*>(entries.at(id).ptr);
        }

    private:
        static constexpr size_t kallocator_byte_size = 1024;
        LinearAllocator allocator = {};
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
        rhi::ResourceUsage usage = rhi::RESOURCE_USAGE_UNUSED;
    };

    struct PassExecutionContext
    {
        const ResourceManager* resource_context = nullptr;
        // TODO: Do we even need this? Can we not just get it through our recompilation desc?
        const PipelineStore* pipeline_context = nullptr;
        const SettingsStore* settings_context = nullptr;
        const PerPassDataStore* per_pass_data_store = nullptr;
        rhi::CommandContextHandle cmd_context = {};
    };

    typedef void(*PassSetupFn)(const SettingsStore* settings_context, PerPassDataStore* per_pass_data_store);
    typedef void(*PassExecuteFn)(const PassExecutionContext* execution_context);
    typedef void(*PassTeardownFn)(PerPassDataStore* per_pass_data_store);
    // TODO: On settings change? We'll need to handle settings changes for passes that rely on them to create resources in Setup

    struct PassDesc
    {
        const std::string_view name = "";
        const rhi::CommandQueueType command_queue_type = rhi::CommandQueueType::GRAPHICS;
        const PassSetupFn setup_fn = nullptr;
        const PassExecuteFn execute_fn = nullptr;
        const PassTeardownFn teardown_fn = nullptr;
        const std::span<PassResourceUsage const> inputs = {};
        const std::span<PassResourceUsage const> outputs = {};
    };

    class PassListBuilder;
    RESOURCE_HANDLE(PassHandle);

    // TODO: rename this
    class RenderTaskList
    {
        friend class PassListBuilder;

        struct Pass
        {
            bool enabled = true;
            bool requires_flush = false;
            u32 dependency_index = UINT32_MAX;
            PassDesc desc = {};
        };
    public:
        RenderTaskList() = default;
        RenderTaskList(ResourceManager* resource_context, PipelineStore* pipeline_context);
        // TODO: Either call or ensure that PassTeardownFn has been called for all passes
        ~RenderTaskList() = default;

        void execute(/*TaskScheduler& task_scheduler*/);
        void setup(/*TaskScheduler& task_scheduler*/);

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

        bool get_pass_enabled(const PassHandle pass_handle)
        {
            ASSERT(pass_handle.idx < passes.size());
            return passes[pass_handle.idx].enabled;
        }

        template<typename T>
        void set_settings(const ResourceIdentifier settings_id, const T data)
        {
            settings_context.set(settings_id, data);
        }

        void set_pass_enabled(const PassHandle pass_handle, const bool pass_is_enabled)
        {
            ASSERT(pass_handle.idx < passes.size());
            passes[pass_handle.idx].enabled = pass_is_enabled;
        }

    private:
        std::vector<Pass> passes = {};

        rhi::Renderer* prenderer = nullptr;
        // TODO: Find a better naming for this. Settings vs Resources vs PerPass isn't really all that helpful I don't think.
        ResourceManager* resource_context = nullptr;
        PipelineStore* shader_store = nullptr;
        SettingsStore settings_context = {};
        PerPassDataStore per_pass_data_store = {};
    };

    class PassListBuilder
    {
    public:
        PassListBuilder(RenderTaskList* list_to_build);

        enum struct StatusCodes : u8
        {
            SUCCESS = 0,
            MISSING_RESOURCE_MANAGER,
            UNSPECIFIED_BACKBUFFER_RESOURCE,
            MISSING_OUTPUT,
            BACKBUFFER_READ,
            BACKBUFFER_USED_AS_NON_RT,
            UNREGISTERED_RESOURCE,
            RESOURCE_USAGE_INVALID,
            COUNT,
        };

        class Result
        {
        public:
            friend class PassListBuilder;

            Result(StatusCodes code) : code{ code }, resource_id{} {};
            Result(StatusCodes code, ResourceIdentifier resource_id) : code{ code }, resource_id{ resource_id } {};

            StatusCodes get_code()
            {
                return code;
            }

            ResourceIdentifier get_associate_resource_id()
            {
                if (code == StatusCodes::MISSING_OUTPUT)
                {
                    return resource_id;
                }
                return ResourceIdentifier{};
            }

            PassHandle get_pass_handle()
            {
                ASSERT(is_success());
                return pass_handle;
            }

            bool is_success() { return code == StatusCodes::SUCCESS; };
        private:
            StatusCodes code = StatusCodes::SUCCESS;
            PassHandle pass_handle;
            ResourceIdentifier resource_id;
        };

        void set_resource_context(ResourceManager* resource_context);
        void set_shader_store(PipelineStore* shader_store);
        void set_pass_list(RenderTaskList* pass_list);
        Result add_pass(const PassDesc& render_pass_desc);

    private:
        struct BuilderResourceState : BarrierState
        {
            u32 last_written_to_by = UINT32_MAX;
        };

        rhi::Renderer* prenderer = nullptr;
        RenderTaskList* out_list = nullptr;
        std::unordered_map<ResourceIdentifier, u32> resource_write_ledger = {};
        std::unordered_map<ResourceIdentifier, BuilderResourceState> resource_states;
    };
}
