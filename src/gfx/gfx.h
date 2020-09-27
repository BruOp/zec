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

    RenderTargetHandle get_current_backbuffer_handle();

    // Resource creation
    BufferHandle create_buffer(BufferDesc buffer_desc);
    MeshHandle create_mesh(MeshDesc mesh_desc);
    TextureHandle create_texture(TextureDesc texture_desc);
    ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc);
    ResourceLayoutHandle create_resource_layout(const ResourceLayoutDescV2& desc);
    PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc);

    // Resource Queries
    u32 get_shader_readable_texture_index(const TextureHandle handle);

    // Resource loading
    TextureHandle load_texture_from_file(const char* file_path);

    // Resource updates
    void update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size);

    // Resource Binding
    void set_active_resource_layout(const ResourceLayoutHandle resource_layout_id);
    void set_pipeline_state(const PipelineStateHandle pso_handle);

    void bind_resource_table(const u32 resource_layout_entry_idx);
    void bind_constant_buffer(const BufferHandle& buffer_handle, u32 binding_slot);

    void draw_mesh(const MeshHandle mesh_id);
    void clear_render_target(const RenderTargetHandle render_target, const float* clear_color);
    inline void clear_render_target(const RenderTargetHandle render_target, const vec4 clear_color)
    {
        clear_render_target(render_target, clear_color.data);
    };
    void set_viewports(const Viewport* viewport, const u32 num_viewports);
    void set_scissors(const Scissor* scissor, const u32 num_scissors);
    void set_render_targets(RenderTargetHandle* render_targets, const u32 num_render_targets);
}
