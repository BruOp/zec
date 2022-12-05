#include "tone_mapping_pass.h"

namespace clustered
{
    using namespace zec;
    using namespace zec::render_graph;

    namespace
    {
        struct TonemapPassConstants
        {
            u32 src_texture;
            float exposure;
        };

        static constexpr PassResourceUsage inputs[] = {
            {
                .identifier = to_rid(EResourceIds::HDR_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_SHADER_READABLE,
            },
        };

        static constexpr PassResourceUsage outputs[] = {
            {
                .identifier = to_rid(EResourceIds::SDR_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
            },
        };
    }

    namespace tone_mapping_pass
    {
        static void tone_mapping_pass_exectution(const PassExecutionContext* context)
        {
            const CommandContextHandle cmd_ctx = context->cmd_context;
            const ResourceContext& resource_context = *context->resource_context;
            const PipelineStore& pipeline_context = *context->pipeline_context;
            const SettingsStore& settings_context = *context->settings_context;

            TextureHandle sdr_buffer = resource_context.get_texture(to_rid(EResourceIds::SDR_TARGET));
            gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

            const TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout(to_rid(EResourceLayoutIds::TONE_MAPPING_PASS_RESOURCE_LAYOUT));
            const PipelineStateHandle pso = pipeline_context.get_pipeline(to_rid(EPipelineIds::TONE_MAPPING_PASS_PIPELINE));
            gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            TextureHandle hdr_buffer = resource_context.get_texture(to_rid(EResourceIds::HDR_TARGET));
            const float exposure = settings_context.get<float>(to_rid(ESettingsIds::EXPOSURE));
            TonemapPassConstants tonemapping_constants = {
                .src_texture = gfx::textures::get_shader_readable_index(hdr_buffer),
                .exposure = exposure
            };
            gfx::cmd::bind_graphics_constants(cmd_ctx, &tonemapping_constants, 2, 0);
            gfx::cmd::bind_graphics_resource_table(cmd_ctx, 1);

            // TODO: Why isn't this just a resource? We just need three indices in a buffer actually
            const MeshHandle fullscreen_mesh = settings_context.get<MeshHandle>(to_rid(ESettingsIds::FULLSCREEN_QUAD));
            gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
        }

        extern const render_graph::PassDesc pass_desc = {
            .name = "Tone Mapping Pass",
            .command_queue_type = zec::CommandQueueType::GRAPHICS,
            .setup_fn = nullptr,
            .execute_fn = &tone_mapping_pass_exectution,
            .inputs = inputs,
            .outputs = outputs,
        };
    }

}
