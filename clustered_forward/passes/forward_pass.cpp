#include "forward_pass.h"

using namespace zec;

namespace clustered
{
    void ForwardPass::setup()
    {
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
                {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { ShaderVisibility::ALL },
                { ShaderVisibility::PIXEL },
                { ShaderVisibility::PIXEL },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 3,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::ANISOTROPIC,
                    .wrap_u = SamplerWrapMode::WRAP,
                    .wrap_v = SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
                {
                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_POINT,
                    .wrap_u = SamplerWrapMode::CLAMP,
                    .wrap_v = SamplerWrapMode::CLAMP,
                    .binding_slot = 1,
                },
            },
            .num_static_samplers = 2,
        };

        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
        gfx::set_debug_name(resource_layout, L"Forward Pass Layout");

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {
            .shader_file_path = L"shaders/clustered_forward/mesh_forward.hlsl",
        };
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
            { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
            { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
        } };
        pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::EQUAL;
        pipeline_desc.depth_stencil_state.depth_write = true;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::set_debug_name(pso, L"Forward Pass Pipeline");

        binning_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ClusterGridConstants),
            .stride = 0 });

        gfx::set_debug_name(binning_cb, L"Forward Pass Binning Constants");
    };

    void ForwardPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
    {
        const BufferHandle spot_light_indices_buffer = resource_map.get_buffer_resource(PassResources::SPOT_LIGHT_INDICES.id);
        const BufferHandle point_light_indices_buffer = resource_map.get_buffer_resource(PassResources::POINT_LIGHT_INDICES.id);

        // Hmmm, shouldn't this be in copy?
        binning_constants.setup = cluster_grid_setup;
        binning_constants.spot_light_indices_list_idx = gfx::buffers::get_shader_readable_index(spot_light_indices_buffer);
        binning_constants.point_light_indices_list_idx = gfx::buffers::get_shader_readable_index(point_light_indices_buffer);
        gfx::buffers::update(binning_cb, &binning_constants, sizeof(binning_constants));
        
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.id);
        TextureHandle render_target = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
        const TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);
        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);

        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, scene_constants_buffer, u32(BindingSlots::SCENE_CONSTANT_BUFFER));
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, binning_cb, u32(BindingSlots::CLUSTER_GRID_CONSTANTS));

        const auto* scene_data = scene_renderables;
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_2D_TABLE));
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_CUBE_TABLE));

        u32 buffer_descriptors[2] = {
            gfx::buffers::get_shader_readable_index(scene_data->vs_buffer),
            scene_data->material_buffer_idx,
        };
        gfx::cmd::bind_graphics_constants(cmd_ctx, &buffer_descriptors, 2, u32(BindingSlots::BUFFERS_DESCRIPTORS));

        for (u32 i = 0; i < scene_data->num_entities; i++) {
            u32 per_draw_indices[] = {
                i,
                scene_data->material_indices[i]
            };
            gfx::cmd::bind_graphics_constants(cmd_ctx, &per_draw_indices, 2, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
            gfx::cmd::draw_mesh(cmd_ctx, scene_data->meshes[i]);
        }
    };
}