#include "background_pass.h"
#include "../bounding_meshes.h"
#include "../pass_resources.h"

using namespace zec;

namespace clustered
{
    using namespace render_graph;

    namespace
    {
        enum struct BindingSlots : u32
        {
            CONSTANTS = 0,
            VIEW_CONSTANT_BUFFER,
            RESOURCE_TABLE
        };

        static constexpr PassResourceUsage inputs[] = {
            {
                .identifier = to_rid(EResourceIds::DEPTH_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };

        static constexpr PassResourceUsage outputs[] = {
            {
                .identifier = to_rid(EResourceIds::HDR_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
            },
        };
    }

    namespace background_pass
    {
        static void background_pass_execution(const render_graph::PassExecutionContext* context)
        {
            const CommandContextHandle cmd_ctx = context->cmd_context;
            const ResourceContext& resource_context = *context->resource_context;
            const PipelineStore& pipeline_context = *context->pipeline_context;
            const SettingsStore& settings_context = *context->settings_context;

            TextureHandle depth_target = resource_context.get_texture(to_rid(EResourceIds::DEPTH_TARGET));
            TextureHandle hdr_buffer = resource_context.get_texture(to_rid(EResourceIds::HDR_TARGET));
            const TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout(to_rid(EResourceLayoutIds::BACKGROUND_PASS_RESOURCE_LAYOUT));
            const PipelineStateHandle pso = pipeline_context.get_pipeline(to_rid(EPipelineIds::BACKGROUND_PASS_PIPELINE));

            gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            const TextureHandle background_cube_map = settings_context.get<TextureHandle>(to_rid(ESettingsIds::BACKGROUND_CUBE_MAP));
            const BufferHandle view_cb_handle = settings_context.get<BufferHandle>(to_rid(ESettingsIds::MAIN_PASS_VIEW_CB));
            const Draw fullscreen_mesh = settings_context.get<Draw>(to_rid(ESettingsIds::FULLSCREEN_QUAD));

            struct Constants
            {
                u32 cube_map_idx;
                float mip_level;
            };
            Constants constants = {
                .cube_map_idx = gfx::textures::get_shader_readable_index(background_cube_map),
                .mip_level = 1.0f,
            };
            gfx::cmd::bind_graphics_constants(cmd_ctx, &constants, 2, u32(BindingSlots::CONSTANTS));
            gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
            gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RESOURCE_TABLE));

            gfx::cmd::draw(cmd_ctx, fullscreen_mesh);
        };

        extern const render_graph::PassDesc pass_desc = {
            .name = "Background Pass",
            .command_queue_type = zec::CommandQueueType::GRAPHICS,
            .execute_fn = &background_pass_execution,
            .inputs = inputs,
            .outputs = outputs,
        };
    }

}
