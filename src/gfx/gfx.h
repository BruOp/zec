#pragma once
#include "pch.h"
#include "core/zec_math.h"
#include "public_resources.h"

namespace zec
{
    void init_renderer(const RendererDesc& renderer_desc);
    void destroy_renderer();

    void begin_upload();
    void end_upload();

    void begin_frame();
    void end_frame();

    // ---------- Resource Queries ----------
    u32 get_shader_readable_texture_index(const TextureHandle handle);
    TextureHandle get_current_back_buffer_handle();

    // ---------- Resource creation ----------
    BufferHandle create_buffer(BufferDesc buffer_desc);
    MeshHandle create_mesh(MeshDesc mesh_desc);
    TextureHandle create_texture(TextureDesc texture_desc);
    ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc);
    PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc);

    TextureHandle load_texture_from_file(const char* file_path);

    // ---------- Resource updates ----------
    void update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size);

    namespace gfx::cmd
    {
        // ---------- Command Contexts ----------
        void return_to_pool(CommandContextHandle& command_context);

        CommandContextHandle provision();

        //--------- Resource Binding ----------
        void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id);
        void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle);

        void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx);
        void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot);

        void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_id);
        void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color);
        inline void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const vec4 clear_color)
        {
            clear_render_target(ctx, render_texture, clear_color.data);
        };
        void clear_depth_target(const CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value);

        void set_viewports(const CommandContextHandle ctx, const Viewport* viewport, const u32 num_viewports);
        void set_scissors(const CommandContextHandle ctx, const Scissor* scissor, const u32 num_scissors);
        void set_render_targets(
            const CommandContextHandle ctx,
            TextureHandle* render_textures,
            const u32 num_render_targets,
            const TextureHandle depth_texture = INVALID_HANDLE
        );

    }
}
