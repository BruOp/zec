#include "tone_mapping_pass.h"
#include "../bounding_meshes.h"

using namespace zec;

namespace clustered
{
    void ToneMappingPass::setup()
    {
        // Create fullscreen quad
        MeshDesc fullscreen_desc{
            .index_buffer_desc = {
                .usage = RESOURCE_USAGE_INDEX,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(geometry::k_fullscreen_indices),
                .stride = sizeof(geometry::k_fullscreen_indices[0]),
            },
            .vertex_buffer_descs = {
                {
                    .usage = RESOURCE_USAGE_VERTEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(geometry::k_fullscreen_positions),
                    .stride = 3 * sizeof(geometry::k_fullscreen_positions[0]),
                },
                {
                    .usage = RESOURCE_USAGE_VERTEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(geometry::k_fullscreen_uvs),
                    .stride = 2 * sizeof(geometry::k_fullscreen_uvs[0]),
                },
            },
            .index_buffer_data = geometry::k_fullscreen_indices,
            .vertex_buffer_data = {
                geometry::k_fullscreen_positions,
                geometry::k_fullscreen_uvs
            }
        };
        CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);
        fullscreen_mesh = gfx::meshes::create(copy_ctx, fullscreen_desc);
        gfx::cmd::cpu_wait(gfx::cmd::return_and_execute(&copy_ctx, 1));

        ResourceLayoutDesc tonemapping_layout_desc{
            .constants = {{
                .visibility = ShaderVisibility::PIXEL,
                .num_constants = 2
             }},
            .num_constants = 1,
            .num_constant_buffers = 0,
            .tables = {
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
                    .wrap_u = SamplerWrapMode::CLAMP,
                    .wrap_v = SamplerWrapMode::CLAMP,
                    .binding_slot = 0,
                },
            },
            .num_static_samplers = 1,
        };

        resource_layout = gfx::pipelines::create_resource_layout(tonemapping_layout_desc);
        PipelineStateObjectDesc pipeline_desc = {
             .resource_layout = resource_layout,
            .input_assembly_desc = {{
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
             } },
             .rtv_formats = {{ BufferFormat::R8G8B8A8_UNORM_SRGB }},
             .shader_file_path = L"shaders/clustered_forward/basic_tone_map.hlsl",
        };
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::OFF;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void ToneMappingPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
    {
        TextureHandle hdr_buffer = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
        TextureHandle sdr_buffer = resource_map.get_texture_resource(PassResources::SDR_TARGET.id);

        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        const TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        TonemapPassConstants tonemapping_constants = {
            .src_texture = gfx::textures::get_shader_readable_index(hdr_buffer),
            .exposure = exposure
        };
        gfx::cmd::bind_graphics_constants(cmd_ctx, &tonemapping_constants, 2, 0);
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, 1);

        gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
    };
}