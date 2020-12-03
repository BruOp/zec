#pragma once
#include "pch.h"
#include "core/zec_math.h"
#include "public_resources.h"

namespace zec::gfx
{
    void init_renderer(const RendererDesc& renderer_desc);
    void destroy_renderer();

    u64 get_current_frame_idx();
    u64 get_current_cpu_frame();

    void wait_for_gpu();

    void begin_upload();
    CmdReceipt end_upload();

    CommandContextHandle begin_frame();
    void end_frame(const CommandContextHandle command_context);

    void reset_for_frame();
    CmdReceipt present_frame();

    TextureHandle get_current_back_buffer_handle();

    MeshHandle create_mesh(MeshDesc mesh_desc);

    namespace buffers
    {
        BufferHandle create(BufferDesc buffer_desc);

        void update(const BufferHandle buffer_id, const void* data, u64 byte_size);
    }

    namespace pipelines
    {
        ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc);
        PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc);
    }

    namespace textures
    {
        TextureHandle create(TextureDesc texture_desc);

        u32 get_shader_readable_index(const TextureHandle handle);
        u32 get_shader_writable_index(const TextureHandle handle);

        TextureInfo& get_texture_info(const TextureHandle texture_handle);

        TextureHandle load_from_file(const char* file_path, const bool force_srgb = false);
        void save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage);
    }

    namespace cmd
    {
        // ---------- Command Contexts ----------
        CommandContextHandle provision(CommandQueueType type);

        CmdReceipt return_and_execute(const CommandContextHandle context_handles[], const size_t num_contexts);

        bool check_status(const CmdReceipt receipt);

        void flush_queue(const CommandQueueType type);

        void cpu_wait(const CmdReceipt receipt);

        void gpu_wait(CommandQueueType type, const CmdReceipt receipt);

        //--------- Resource Binding ----------
        void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id);

        void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle);

        void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx);

        void bind_constant(const CommandContextHandle ctx, const void* data, const u64 num_constants, const u32 binding_slot);

        void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot);

        // Draw / Dispatch
        void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_id);

        void dispatch(
            const CommandContextHandle ctx,
            const u32 thread_group_count_x,
            const u32 thread_group_count_y,
            const u32 thread_group_count_z
        );

        // Misc
        void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color);
        inline void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const vec4 clear_color)
        {
            clear_render_target(ctx, render_texture, clear_color.data);
        };

        void clear_depth_target(const CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value);

        void set_viewports(const CommandContextHandle ctx, const Viewport* viewports, const u32 num_viewports);

        void set_scissors(const CommandContextHandle ctx, const Scissor* scissors, const u32 num_scissors);

        void set_render_targets(
            const CommandContextHandle ctx,
            TextureHandle* render_textures,
            const u32 num_render_targets,
            const TextureHandle depth_target = INVALID_HANDLE
        );

        void transition_textures(
            const CommandContextHandle ctx,
            TextureTransitionDesc* transition_descs,
            u64 num_transitions
        );

        void transition_resources(
            const CommandContextHandle ctx,
            ResourceTransitionDesc* transition_descs,
            u64 num_transitions
        );
    }
}
