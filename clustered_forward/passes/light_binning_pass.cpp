#include "light_binning_pass.h"

using namespace zec;

namespace clustered
{
    const render_pass_system::IRenderPass::OutputList LightBinningPass::get_output_list() const
    {
        return OutputList{
            .ptr = pass_outputs,
            .count = u32(std::size(pass_outputs)),
        };
    }

    LightBinningPass::LightBinningPass(const ClusterGridSetup grid_setup) : cluster_grid_setup{ grid_setup }
    {
        set_output_dimensions(cluster_grid_setup);
    }

    void LightBinningPass::set_output_dimensions(const ClusterGridSetup cluster_grid_setup) {
        u32 depth = cluster_grid_setup.pre_mid_depth + cluster_grid_setup.post_mid_depth;
        u32 num_clusters = cluster_grid_setup.width * cluster_grid_setup.height * depth;
        pass_outputs[size_t(Outputs::SPOT_LIGHT_INDICES)].buffer_desc.byte_size = (ClusterGridSetup::MAX_LIGHTS_PER_BIN + 1) * num_clusters * sizeof(u32);
        pass_outputs[size_t(Outputs::POINT_LIGHT_INDICES)].buffer_desc.byte_size = (ClusterGridSetup::MAX_LIGHTS_PER_BIN + 1) * num_clusters * sizeof(u32);
    };

    void LightBinningPass::setup()
    {
        ResourceLayoutDesc resource_layout_desc{
            .num_constants = 0,
            .constant_buffers = {
                {.visibility = ShaderVisibility::COMPUTE },
                {.visibility = ShaderVisibility::COMPUTE },
                {.visibility = ShaderVisibility::COMPUTE },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::WRITE },
            },
            .num_resource_tables = 2,
            .num_static_samplers = 0,
        };

        resource_layout = gfx::pipelines::create_resource_layout(resource_layout_desc);

        spot_light_pso = gfx::pipelines::create_pipeline_state_object({
            .resource_layout = resource_layout,
            .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/spot_light_assignment.hlsl",
        });
        gfx::set_debug_name(spot_light_pso, L"Light Binning PSO");

        point_light_pso = gfx::pipelines::create_pipeline_state_object({
            .resource_layout = resource_layout,
            .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/point_light_assignment.hlsl",
        });
        gfx::set_debug_name(point_light_pso, L"Light Binning PSO");
        
        binning_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(BinningConstants),
            .stride = 0 });
    }

    void LightBinningPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
    {
        const BufferHandle spot_light_indices_buffer = resource_map.get_buffer_resource(PassResources::SPOT_LIGHT_INDICES.id);
        const BufferHandle point_light_indices_buffer = resource_map.get_buffer_resource(PassResources::POINT_LIGHT_INDICES.id);

        binning_constants.setup = cluster_grid_setup;
        binning_constants.spot_light_indices_list_idx = gfx::buffers::get_shader_writable_index(spot_light_indices_buffer);
        binning_constants.point_light_indices_list_idx = gfx::buffers::get_shader_writable_index(point_light_indices_buffer);
        gfx::buffers::update(binning_cb, &binning_constants, sizeof(binning_constants));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, spot_light_pso);

        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, binning_cb, u32(Slots::LIGHT_GRID_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(Slots::VIEW_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, scene_constants_buffer, u32(Slots::SCENE_CONSTANTS));

        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::READ_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_BUFFERS_TABLE));

        u32 group_size = 8;
        u32 group_count_x = (binning_constants.setup.width + (group_size - 1)) / group_size;
        u32 group_count_y = (binning_constants.setup.height + (group_size - 1)) / group_size;
        u32 group_count_z = binning_constants.setup.pre_mid_depth + binning_constants.setup.post_mid_depth;

        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);

        gfx::cmd::set_compute_pipeline_state(cmd_ctx, point_light_pso);
        
        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);
    };
}
