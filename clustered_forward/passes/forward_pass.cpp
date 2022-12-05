#include "forward_pass.h"

namespace clustered
{
    using namespace zec;
    using namespace zec::render_graph;

    namespace
    {
        enum struct BindingSlots : u32
        {
            PER_INSTANCE_CONSTANTS = 0,
            BUFFERS_DESCRIPTORS,
            VIEW_CONSTANT_BUFFER,
            SCENE_CONSTANT_BUFFER,
            CLUSTER_GRID_CONSTANTS,
            RAW_BUFFERS_TABLE,
            TEXTURE_2D_TABLE,
            TEXTURE_CUBE_TABLE,
        };

        struct ClusterGridConstants
        {
            ClusterGridSetup setup;
            u32 spot_light_indices_list_idx;
            u32 point_light_indices_list_idx;
        };

        struct ForwardPassData
        {
            BufferHandle cluster_grid_cb;
        };
        MAKE_IDENTIFIER(FORWARD_PASS_DATA)

        static constexpr PassResourceUsage inputs[] = {
            {
                .identifier = to_rid(EResourceIds::DEPTH_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            },
            {
                .identifier = to_rid(EResourceIds::SPOT_LIGHT_INDICES),
                .type = PassResourceType::BUFFER,
                .usage = RESOURCE_USAGE_SHADER_READABLE,
            },
            {
                .identifier = to_rid(EResourceIds::POINT_LIGHT_INDICES),
                .type = PassResourceType::BUFFER,
                .usage = RESOURCE_USAGE_SHADER_READABLE,
            }
        };

        static constexpr PassResourceUsage outputs[] = {
            {
                .identifier = to_rid(EResourceIds::HDR_TARGET),
                .type = PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_RENDER_TARGET,
            },
        };
    }

    namespace forward_pass
    {
        static void forward_pass_setup(const SettingsStore* settings_context, PerPassDataStore* per_pass_data_store)
        {
            BufferHandle cluster_grid_cb = gfx::buffers::create({
                .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(ClusterGridConstants),
                .stride = 0 });
            gfx::set_debug_name(cluster_grid_cb, L"Forward Pass Binning Constants");
            per_pass_data_store->emplace(FORWARD_PASS_DATA, ForwardPassData{ .cluster_grid_cb = cluster_grid_cb });
        };

        static void forward_pass_execution(const PassExecutionContext* context)
        {
            const CommandContextHandle cmd_ctx = context->cmd_context;
            const ResourceContext& resource_context = *context->resource_context;
            const PipelineStore& pipeline_context = *context->pipeline_context;
            const SettingsStore& settings_context = *context->settings_context;
            const PerPassDataStore& per_pass_data_store = *context->per_pass_data_store;

            const ForwardPassData pass_data = per_pass_data_store.get<ForwardPassData>(FORWARD_PASS_DATA);

            const BufferHandle spot_light_indices_buffer = resource_context.get_buffer(to_rid(EResourceIds::SPOT_LIGHT_INDICES));
            const BufferHandle point_light_indices_buffer = resource_context.get_buffer(to_rid(EResourceIds::POINT_LIGHT_INDICES));

            const ClusterGridSetup cluster_grid_setup = settings_context.get<ClusterGridSetup>(to_rid(ESettingsIds::CLUSTER_GRID_SETUP));

            ClusterGridConstants binning_constants = {
                .setup = cluster_grid_setup,
                .spot_light_indices_list_idx = gfx::buffers::get_shader_readable_index(spot_light_indices_buffer),
                .point_light_indices_list_idx = gfx::buffers::get_shader_readable_index(point_light_indices_buffer),
            };
            gfx::buffers::update(pass_data.cluster_grid_cb, &binning_constants, sizeof(binning_constants));

            TextureHandle depth_target = resource_context.get_texture(to_rid(EResourceIds::DEPTH_TARGET));
            TextureHandle render_target = resource_context.get_texture(to_rid(EResourceIds::HDR_TARGET));
            const TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout(to_rid(EResourceLayoutIds::FORWARD_PASS_RESOURCE_LAYOUT));
            const PipelineStateHandle pso = pipeline_context.get_pipeline(to_rid(EPipelineIds::FORWARD_PASS_PIPELINE));

            gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);


            const RenderableScene* scene_data = settings_context.get<RenderableScene*>(to_rid(ESettingsIds::RENDERABLE_SCENE_PTR));
            const BufferHandle view_cb_handle = settings_context.get<BufferHandle>(to_rid(ESettingsIds::MAIN_PASS_VIEW_CB));
            gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
            gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, scene_data->scene_constants, u32(BindingSlots::SCENE_CONSTANT_BUFFER));
            gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, pass_data.cluster_grid_cb, u32(BindingSlots::CLUSTER_GRID_CONSTANTS));

            gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));
            gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_2D_TABLE));
            gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_CUBE_TABLE));

            const Renderables& renderables = scene_data->renderables;

            u32 buffer_descriptors[2] = {
                gfx::buffers::get_shader_readable_index(renderables.vs_buffer),
                renderables.material_buffer_idx,
            };
            gfx::cmd::bind_graphics_constants(cmd_ctx, &buffer_descriptors, 2, u32(BindingSlots::BUFFERS_DESCRIPTORS));

            for (u32 i = 0; i < renderables.num_entities; i++) {
                u32 per_draw_indices[] = {
                    i,
                    renderables.material_indices[i]
                };
                gfx::cmd::bind_graphics_constants(cmd_ctx, &per_draw_indices, 2, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
                gfx::cmd::draw_mesh(cmd_ctx, renderables.meshes[i]);
            }
        };

        extern const zec::render_graph::PassDesc pass_desc{
            .name = "Forward Pass",
            .command_queue_type = CommandQueueType::GRAPHICS,
            .setup_fn = &forward_pass_setup,
            .execute_fn = &forward_pass_execution,
            .inputs = inputs,
            .outputs = outputs,
        };
    }

}
