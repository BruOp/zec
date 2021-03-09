#include "depth_pass.h"

using namespace zec;

namespace clustered
{
    void DepthPass::setup()
    {
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 1,
            .tables = {
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        };

        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
        gfx::set_debug_name(resource_layout, L"Forward Pass Layout");

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {
            .shader_file_path = L"shaders/clustered_forward/depth_pass.hlsl",
        };
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
        } };
        //pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::GREATER;
        pipeline_desc.depth_stencil_state.depth_write = true;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX;

        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::set_debug_name(pso, L"Depth Pass Pipeline");
    }

    void DepthPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
    {
        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.id);
        const TextureInfo& texture_info = gfx::textures::get_texture_info(depth_target);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 0.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, nullptr, 0, depth_target);

        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));

        const auto* scene_data = scene_renderables;
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));

        u32 buffer_descriptor = gfx::buffers::get_shader_readable_index(scene_data->vs_buffer);
        gfx::cmd::bind_graphics_constants(cmd_ctx, &buffer_descriptor, 1, u32(BindingSlots::BUFFERS_DESCRIPTORS));

        for (u32 i = 0; i < scene_data->num_entities; i++) {
            gfx::cmd::bind_graphics_constants(cmd_ctx, &i, 1, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
            gfx::cmd::draw_mesh(cmd_ctx, scene_data->meshes[i]);
        }
    };
}