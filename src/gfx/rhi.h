#pragma once
#include "core/zec_types.h"
#include "core/zec_math.h"
#include "rhi_public_resources.h"
#include "utils/string_fwd.h"

namespace zec::ui
{
    class UIRenderer;
}

namespace zec::rhi
{
    class RenderContext;

    class Renderer
    {
    public:
        UNCOPIABLE(Renderer);
        UNMOVABLE(Renderer);

        Renderer() = default;
        ~Renderer();

        void init(const RendererDesc& renderer_desc);
        void destroy();

        // Getters
        RenderConfigState get_config_state() const;
        u64 get_current_frame_idx() const;
        TextureHandle get_current_back_buffer_handle();

        // Returns indices for use inside shaders (for direct indexing into our bindless descriptor tables)
        u32 get_readable_index(const BufferHandle handle) const;
        u32 get_writable_index(const BufferHandle handle) const;
        u32 get_readable_index(const TextureHandle handle) const;
        u32 get_writable_index(const TextureHandle handle) const;

        // Frame lifecycle methods
        void flush_gpu();
        CommandContextHandle begin_frame();
        void end_frame(CommandContextHandle command_context);
        void reset_for_frame();
        CmdReceipt present_frame();

        // Additional life cycle methods
        void on_window_resize(u32 width, u32 height);

        // ------------ Shader Compilation ------------
        // Please release the blobs after you've used them to create the pipeline!
        ZecResult shaders_compile(const ShaderCompilationDesc& shader_compilation_desc, ShaderBlobsHandle& inout_blobs, std::string& inout_errors);
        void shaders_release_blobs(ShaderBlobsHandle& blobs);

        // ------------ Pipelines ------------
        ResourceLayoutHandle resource_layouts_create(const ResourceLayoutDesc& desc);
        PipelineStateHandle pipelines_create(const ShaderBlobsHandle& shader_blobs_handle, const ResourceLayoutHandle& resource_layout_handle, const PipelineStateObjectDesc& desc);
        // This basically creates a new PSO and replaces the existing pipeline_state_handle that's stored at the index.
        // Obviously you probably don't want to do this in the middle of a frame, but it _should_ be safe to do so.
        ZecResult pipelines_recreate(const ShaderBlobsHandle& shader_blobs_handle, const ResourceLayoutHandle& resource_layout_handle, const PipelineStateObjectDesc& desc, const PipelineStateHandle pipeline_state_handle);

        // ------------ Buffers ------------
        BufferHandle buffers_create(BufferDesc buffer_desc);
        void buffers_set_data(const BufferHandle handle, const void* data, const u64 data_byte_size);
        void buffers_set_data(CommandContextHandle cmd_ctx, const BufferHandle handle, const void* data, const u64 data_byte_size);
        void buffers_update(const BufferHandle buffer_id, const void* data, u64 byte_size);

        // ------------ Textures ------------
        TextureHandle textures_create(TextureDesc texture_desc);
        TextureHandle textures_create_from_file(CommandContextHandle cmd_ctx, const char* file_path);
        void textures_save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage);

        const TextureInfo& textures_get_info(const TextureHandle texture_handle) const;

        // The following all involved command queues or lists. All functions are prefixed with cmd for searchability
        // For functions that accept a CommandContextHandle, you may NOT call them on different threads with the same CommandContext
        // However, with different handles they should be safe to call on separate threads.
        // ---------- Command Contexts ----------
        CommandContextHandle cmd_provision(CommandQueueType type);
        CmdReceipt cmd_return_and_execute(CommandContextHandle* context_handles, const size_t num_contexts);

        //--------- Synchronization ----------
        bool cmd_check_status(const CmdReceipt receipt);
        void cmd_flush_queue(const CommandQueueType type);
        void cmd_cpu_wait(const CmdReceipt receipt);
        void cmd_gpu_wait(const CommandQueueType queue_to_insert_wait, const CmdReceipt receipt_to_wait_on);
        void cmd_compute_write_barrier(CommandContextHandle ctx, BufferHandle buffer_handle);
        void cmd_compute_write_barrier(CommandContextHandle ctx, TextureHandle texture_handle);

        //--------- Resource Transitions ----------
        void cmd_transition_resources(
            CommandContextHandle ctx,
            ResourceTransitionDesc* transition_descs,
            u64 num_transitions
        );

        //--------- Resource Binding ----------
        void cmd_set_graphics_resource_layout(CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id) const;
        void cmd_set_graphics_pipeline_state(CommandContextHandle ctx, const PipelineStateHandle pso_handle) const;
        void cmd_bind_graphics_resource_table(CommandContextHandle ctx, const u32 resource_layout_entry_idx) const;
        void cmd_bind_graphics_constants(CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot) const;
        void cmd_bind_graphics_constant_buffer(CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot) const;

        void cmd_set_compute_resource_layout(CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id) const;
        void cmd_set_compute_pipeline_state(CommandContextHandle ctx, const PipelineStateHandle pso_handle) const;
        void cmd_bind_compute_resource_table(CommandContextHandle ctx, const u32 resource_layout_entry_idx) const;
        void cmd_bind_compute_constants(CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot) const;
        void cmd_bind_compute_constant_buffer(CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot);
        //--------- Drawing ----------
        void cmd_draw_lines(CommandContextHandle ctx, const BufferHandle vertices) const;
        void cmd_draw(CommandContextHandle ctx, const BufferHandle index_buffer_view_id, const size_t num_instances = 1) const;
        void cmd_draw(CommandContextHandle ctx, const Draw& draw) const;

        //--------- Dispatch ----------
        void cmd_dispatch(
            CommandContextHandle ctx,
            const u32 thread_group_count_x,
            const u32 thread_group_count_y,
            const u32 thread_group_count_z
        ) const;

        //--------- Pipeline State Setup ---------
        void cmd_clear_render_target(CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color) const;
        inline void cmd_clear_render_target(CommandContextHandle ctx, const TextureHandle render_texture, const vec4 clear_color) const
        {
            cmd_clear_render_target(ctx, render_texture, clear_color.data);
        };
        void cmd_clear_depth_target(CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value) const;
        void cmd_set_viewports(CommandContextHandle ctx, const Viewport* viewports, const u32 num_viewports) const;
        void cmd_set_scissors(CommandContextHandle ctx, const Scissor* scissors, const u32 num_scissors) const;
        void cmd_set_render_targets(
            CommandContextHandle ctx,
            TextureHandle* render_textures,
            const u32 num_render_targets,
            const TextureHandle depth_target = INVALID_HANDLE
        ) const;

        //--------- Debug ----------
        void set_debug_name(const ResourceLayoutHandle handle, const wchar* name);
        void set_debug_name(const PipelineStateHandle handle, const wchar* name);
        void set_debug_name(const BufferHandle handle, const wchar* name);
        void set_debug_name(const TextureHandle handle, const wchar* name);

    private:
        friend class ::zec::ui::UIRenderer;

        RenderContext& get_render_context();

        RenderContext* pcontext = nullptr;
    };
}
