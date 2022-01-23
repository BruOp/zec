#pragma once
#include "core/zec_types.h"
#include "core/zec_math.h"
#include "public_resources.h"

namespace std
{
    template <class _Elem>
    class allocator;

    template <class _Elem>
    struct char_traits; // properties of a string or stream unknown element

    template <class _Elem, class _Traits, class _Alloc>
    class basic_string;

    typedef basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t>> wstring;
    typedef basic_string<char, char_traits<char>, allocator<char>> string;
}

namespace zec::gfx
{
    void init_renderer(const RendererDesc& renderer_desc);
    void destroy_renderer();

    RenderConfigState get_config_state();

    u64 get_current_frame_idx();

    void flush_gpu();

    CommandContextHandle begin_frame();
    void end_frame(CommandContextHandle command_context);

    void reset_for_frame();
    CmdReceipt present_frame();

    TextureHandle get_current_back_buffer_handle();

    void on_window_resize(u32 width, u32 height);

    namespace shader_compilation
    {
        // Please release the blobs after you've used them to create the pipeline
        ZecResult compile_shaders(const ShaderCompilationDesc& shader_compilation_desc, ShaderBlobsHandle& inout_blobs, std::string& inout_errors);

        void release_blobs(ShaderBlobsHandle& blobs);
    }

    namespace pipelines
    {
        ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc);
        PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc, const wchar* name);
    }

    namespace buffers
    {
        BufferHandle create(BufferDesc buffer_desc);

        u32 get_shader_readable_index(const BufferHandle handle);
        u32 get_shader_writable_index(const BufferHandle handle);

        void set_data(const BufferHandle handle, const void* data, const u64 data_byte_size);
        void set_data(CommandContextHandle cmd_ctx, const BufferHandle handle, const void* data, const u64 data_byte_size);

        void update(const BufferHandle buffer_id, const void* data, u64 byte_size);
    }

    namespace meshes
    {
        MeshHandle create(const BufferHandle index_buffer, const BufferHandle* vertex_buffers, u32 num_vertex_buffers);
        MeshHandle create(CommandContextHandle cmd_ctx, MeshDesc mesh_desc);
    }

    namespace textures
    {
        TextureHandle create(TextureDesc texture_desc);

        u32 get_shader_readable_index(const TextureHandle handle);
        u32 get_shader_writable_index(const TextureHandle handle);

        const TextureInfo& get_texture_info(const TextureHandle texture_handle);

        TextureHandle create_from_file(CommandContextHandle cmd_ctx, const char* file_path);
        void save_to_file(const TextureHandle texture_handle, const wchar_t* file_path, const ResourceUsage current_usage);
    }

    namespace cmd
    {
        // ---------- Command Contexts ----------
        CommandContextHandle provision(CommandQueueType type);

        CmdReceipt return_and_execute(CommandContextHandle* context_handles, const size_t num_contexts);

        bool check_status(const CmdReceipt receipt);

        void flush_queue(const CommandQueueType type);

        void cpu_wait(const CmdReceipt receipt);

        void gpu_wait(const CommandQueueType queue_to_insert_wait, const CmdReceipt receipt_to_wait_on);

        //--------- Resource Binding ----------
        void set_graphics_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id);

        void set_graphics_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle);

        void bind_graphics_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx);

        void bind_graphics_constants(const CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot);

        void bind_graphics_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot);

        // Draw
        void draw_lines(const CommandContextHandle ctx, const BufferHandle vertices);
        void draw_mesh(const CommandContextHandle ctx, const BufferHandle index_buffer_id, const size_t num_instances = 1);
        void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_id);



        //--------- Resource Binding ----------
        void set_compute_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id);

        void set_compute_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle);

        void bind_compute_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx);

        void bind_compute_constants(const CommandContextHandle ctx, const void* data, const u32 num_constants, const u32 binding_slot);

        void bind_compute_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, const u32 binding_slot);

        // Dispatch
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

        void compute_write_barrier(const CommandContextHandle ctx, BufferHandle buffer_handle);
        void compute_write_barrier(const CommandContextHandle ctx, TextureHandle texture_handle);
    }

    void set_debug_name(const ResourceLayoutHandle handle, const wchar* name);
    void set_debug_name(const PipelineStateHandle handle, const wchar* name);
    void set_debug_name(const BufferHandle handle, const wchar* name);
    void set_debug_name(const TextureHandle handle, const wchar* name);
}
