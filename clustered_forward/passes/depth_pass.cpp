#include "depth_pass.h"
#include "utils/crc_hash.h"

using namespace zec;
using namespace zec::render_graph;

namespace clustered
{
    namespace
    {
        enum struct BindingSlots : u32
        {
            PER_INSTANCE_CONSTANTS = 0,
            BUFFERS_DESCRIPTORS,
            VIEW_CONSTANT_BUFFER,
            RAW_BUFFERS_TABLE,
        };

        constexpr PassResourceUsage outputs[] = {
            {
                .identifier = pass_resources::DEPTH_TARGET,
                .type = PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };

    }

    namespace depth_pass
    {
        void depth_pass_execution_fn(const PassExecutionContext* context)
        {
            const CommandContextHandle cmd_ctx = context->cmd_context;
            const ResourceContext& resource_context = *context->resource_context;
            const PipelineStore& pipeline_context = *context->pipeline_context;
            const SettingsStore& settings_context = *context->settings_context;
            TextureHandle depth_target = resource_context.get_texture(pass_resources::DEPTH_TARGET);

            const TextureInfo& texture_info = gfx::textures::get_texture_info(depth_target);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout({ DEPTH_PASS_RESOURCE_LAYOUT });
            const PipelineStateHandle pso = pipeline_context.get_pipeline({ DEPTH_PASS_PIPELINE });

            gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 0.0f, 0);
            gfx::cmd::set_render_targets(cmd_ctx, nullptr, 0, depth_target);

            const BufferHandle view_cb_handle = settings_context.get<BufferHandle>(settings::MAIN_PASS_VIEW_CB);
            gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));

            const RenderableScene* scene_data = settings_context.get<RenderableScene*>(settings::RENDERABLE_SCENE_PTR);
            const Renderables& renderables = scene_data->renderables;
            gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));

            u32 buffer_descriptor = gfx::buffers::get_shader_readable_index(renderables.vs_buffer);
            gfx::cmd::bind_graphics_constants(cmd_ctx, &buffer_descriptor, 1, u32(BindingSlots::BUFFERS_DESCRIPTORS));

            for (u32 i = 0; i < renderables.num_entities; i++) {
                gfx::cmd::bind_graphics_constants(cmd_ctx, &i, 1, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
                gfx::cmd::draw_mesh(cmd_ctx, renderables.meshes[i]);
            }
        };

        extern const render_graph::PassDesc pass_desc = {
            .name = "Depth Prepass",
            .command_queue_type = zec::CommandQueueType::GRAPHICS,
            .execute_fn = &depth_pass_execution_fn,
            .inputs = {},
            .outputs = outputs,
        };
    }

}