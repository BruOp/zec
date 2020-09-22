#pragma once
#include "pch.h"
#include "core/zec_math.h"
#include "public_resources.h"

#define USE_D3D_RENDERER

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
    PipelineStateHandle  create_pipeline_state_object(const PipelineStateObjectDesc& desc);

    // Resource updates
    void update_buffer(const BufferHandle buffer_id, const void* data, u64 byte_size);

    // Resource Binding
    void set_active_resource_layout(const ResourceLayoutHandle resource_layout_id);
    void set_pipeline_state(const PipelineStateHandle pso_handle);
    void bind_constant_buffer(const BufferHandle& buffer_handle, u32 binding_slot);

    void draw_mesh(const MeshHandle mesh_id);
    void clear_render_target(const RenderTargetHandle render_target, const vec4 clear_color);
    void set_viewports(const Viewport* viewport, const u32 num_viewports);
    void set_scissors(const Scissor* scissor, const u32 num_scissors);
    void set_render_targets(RenderTargetHandle* render_targets, const u32 num_render_targets);
}
