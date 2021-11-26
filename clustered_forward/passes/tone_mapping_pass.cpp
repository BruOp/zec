#include "tone_mapping_pass.h"

namespace clustered
{
    using namespace zec;

    struct TonemapPassConstants
    {
        u32 src_texture;
        float exposure;
    };


    static constexpr RenderPassResourceUsage inputs[] = {
        {
            .identifier = PassResources::HDR_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_SHADER_READABLE,
            .name = PassResources::HDR_TARGET.name,
        },
    };

    static constexpr RenderPassResourceUsage outputs[] = {
        {
            .identifier = PassResources::SDR_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
            .name = PassResources::SDR_TARGET.name,
        },
    };

    static constexpr RenderPassResourceLayoutDesc resource_layout_descs[] = { {
        .identifier = ctcrc32("Tone Mapping Pass Layout Desc"),
        .resource_layout_desc = {
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
        },
    } };

    static constexpr RenderPassPipelineStateObjectDesc pipeline_descs[] = { {
        .identifier = ctcrc32("Tone Mapping Pipeline"),
        .resource_layout_id = resource_layout_descs[0].identifier,
        .pipeline_desc = {
            .input_assembly_desc = {{
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = ComparisonFunc::OFF,
            },
            .raster_state_desc = {
                .cull_mode = CullMode::NONE,
            },
            .rtv_formats = {{ PassResources::SDR_TARGET.desc.format }},
            .used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL,
            .shader_file_path = L"shaders/clustered_forward/basic_tone_map.hlsl",
        }
    } };

    static constexpr SettingsDesc settings[] = {
        Settings::fullscreen_quad,
        Settings::exposure,
    };

    static void tone_mapping_pass_exectution(const RenderPassContext* context)
    {
        const ResourceMap& resource_map = *context->resource_map;

        TextureHandle hdr_buffer = resource_map.get_texture_resource(PassResources::HDR_TARGET.identifier);
        TextureHandle sdr_buffer = resource_map.get_texture_resource(PassResources::SDR_TARGET.identifier);

        const CommandContextHandle cmd_ctx = context->cmd_context;
        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        const TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        const ResourceLayoutHandle resource_layout = context->resource_layouts->at(resource_layout_descs[0].identifier);
        const PipelineStateHandle pso = context->pipeline_states->at(pipeline_descs[0].identifier);
        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        const float exposure = context->settings->get_settings<float>(Settings::exposure.identifier);
        TonemapPassConstants tonemapping_constants = {
            .src_texture = gfx::textures::get_shader_readable_index(hdr_buffer),
            .exposure = exposure
        };
        gfx::cmd::bind_graphics_constants(cmd_ctx, &tonemapping_constants, 2, 0);
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, 1);

        const MeshHandle fullscreen_mesh = context->settings->get_settings<MeshHandle>(Settings::fullscreen_quad.identifier);
        gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
    }

    extern const RenderPassTaskDesc tone_mapping_pass_desc = {
        .name = "Tone Mapping Pass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .setup_fn = nullptr,
        .execute_fn = &tone_mapping_pass_exectution,
        .resource_layout_descs = resource_layout_descs,
        .pipeline_descs = pipeline_descs,
        .settings = settings,
        .inputs = inputs,
        .outputs = outputs,
    };
}
