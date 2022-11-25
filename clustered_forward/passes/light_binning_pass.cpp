#include "light_binning_pass.h"
#include "../renderable_scene.h"
#include "gfx/gfx.h"
#include "utils/crc_hash.h"
#include "../pass_resources.h"

using namespace zec;
using namespace zec::render_graph;

namespace clustered::light_binning_pass
{
    namespace
    {
        enum struct BindingSlots : u32
        {
            CLUSTER_GRID_CONSTANTS = 0,
            VIEW_CONSTANTS,
            SCENE_CONSTANTS,
            READ_BUFFERS_TABLE,
            WRITE_BUFFERS_TABLE,
        };

        struct ClusterGridConstants
        {
            ClusterGridSetup setup;
            u32 spot_light_indices_list_idx;
            u32 point_light_indices_list_idx;
        };

        struct LightBinningPassData
        {
            BufferHandle cluster_grid_cb;
        };
        MAKE_IDENTIFIER(LIGHT_BINNING_PASS_DATA);

    }


    constexpr render_graph::PassResourceUsage outputs[] = {
        {
            .identifier = pass_resources::SPOT_LIGHT_INDICES,
            .type = render_graph::PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE
        },
        {
            .identifier = pass_resources::POINT_LIGHT_INDICES,
            .type = render_graph::PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE
        }
    };

    static void setup(const SettingsStore* settings_context, PerPassDataStore* per_pass_data_store)
    {
        BufferHandle cluster_grid_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ClusterGridConstants),
            .stride = 0 });
        gfx::set_debug_name(cluster_grid_cb, L"Cluster Grid Constants");
        per_pass_data_store->emplace(LIGHT_BINNING_PASS_DATA, LightBinningPassData{
            .cluster_grid_cb = cluster_grid_cb
        });
    }

    void light_binning_execution(const PassExecutionContext* context)
    {
        const CommandContextHandle cmd_ctx = context->cmd_context;
        const ResourceContext& resource_context = *context->resource_context;
        const PipelineStore& pipeline_context = *context->pipeline_context;
        const SettingsStore& settings_context = *context->settings_context;
        const PerPassDataStore* per_pass_data_store = context->per_pass_data_store;

        const LightBinningPassData& pass_data = per_pass_data_store->get<LightBinningPassData>(LIGHT_BINNING_PASS_DATA);
        const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout({ LIGHT_BINNING_PASS_RESOURCE_LAYOUT });
        const PipelineStateHandle spot_light_pso = pipeline_context.get_pipeline({ LIGHT_BINNING_PASS_SPOT_LIGHT_PIPELINE });
        const PipelineStateHandle point_light_pso = pipeline_context.get_pipeline({ LIGHT_BINNING_PASS_POINT_LIGHT_PIPELINE });

        const BufferHandle spot_light_indices_buffer = resource_context.get_buffer(pass_resources::SPOT_LIGHT_INDICES);
        const BufferHandle point_light_indices_buffer = resource_context.get_buffer(pass_resources::POINT_LIGHT_INDICES);

        const ClusterGridSetup cluster_grid_setup = settings_context.get<ClusterGridSetup>(settings::CLUSTER_GRID_SETUP);
        ClusterGridConstants binning_constants = {
            .setup = cluster_grid_setup,
            .spot_light_indices_list_idx = gfx::buffers::get_shader_writable_index(spot_light_indices_buffer),
            .point_light_indices_list_idx = gfx::buffers::get_shader_writable_index(point_light_indices_buffer),
        };
        gfx::buffers::update(pass_data.cluster_grid_cb, &binning_constants, sizeof(binning_constants));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, spot_light_pso);

        const BufferHandle view_cb_handle = settings_context.get<BufferHandle>(settings::MAIN_PASS_VIEW_CB);
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANTS));

        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, pass_data.cluster_grid_cb, u32(BindingSlots::CLUSTER_GRID_CONSTANTS));

        const RenderableScene* renderable_scene = settings_context.get<RenderableScene*>(settings::RENDERABLE_SCENE_PTR);
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, renderable_scene->scene_constants, u32(BindingSlots::SCENE_CONSTANTS));

        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(BindingSlots::READ_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(BindingSlots::WRITE_BUFFERS_TABLE));

        u32 group_size = 8;
        u32 group_count_x = (binning_constants.setup.width + (group_size - 1)) / group_size;
        u32 group_count_y = (binning_constants.setup.height + (group_size - 1)) / group_size;
        u32 group_count_z = binning_constants.setup.pre_mid_depth + binning_constants.setup.post_mid_depth;

        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);

        gfx::cmd::set_compute_pipeline_state(cmd_ctx, point_light_pso);

        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);
    };

    extern const render_graph::PassDesc pass_desc = {
        .name = "Light Binning Pass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .setup_fn = &setup,
        .execute_fn = &light_binning_execution,
        .inputs = {},
        .outputs = outputs,
    };
}
