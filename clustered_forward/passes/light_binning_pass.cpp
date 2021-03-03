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
        pass_outputs[size_t(Outputs::LIGHT_INDICES)].buffer_desc.byte_size = ClusterGridSetup::MAX_LIGHTS_PER_BIN * num_clusters * sizeof(u32);

        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.width = float(cluster_grid_setup.width);
        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.height = float(cluster_grid_setup.height);
        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.depth = depth;
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
                {.usage = ResourceAccess::WRITE },
            },
            .num_resource_tables = 3,
            .num_static_samplers = 0,
        };

        resource_layout = gfx::pipelines::create_resource_layout(resource_layout_desc);

        PipelineStateObjectDesc pipeline_desc = {
            .resource_layout = resource_layout,
            .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/light_assignment.hlsl",
        };
        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::set_debug_name(pso, L"Light Binning PSO");

        ResourceLayoutDesc clearing_resource_layout_desc{
            .constants = {
                { .visibility=ShaderVisibility::COMPUTE, .num_constants = 3 },
            },
            .num_constants = 1,
            .constant_buffers = { },
            .num_constant_buffers = 0,
            .tables = {
                {.usage = ResourceAccess::WRITE },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        };
        clearing_resource_layout = gfx::pipelines::create_resource_layout(clearing_resource_layout_desc);

        clearing_pso = gfx::pipelines::create_pipeline_state_object({
            .resource_layout = clearing_resource_layout,
            .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/clear_buffer.hlsl",
            });
        gfx::set_debug_name(pso, L"Light count clearing PSO");

        binning_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(BinningConstants),
            .stride = 0 });
    }

    void LightBinningPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
    {
        const BufferHandle indices_buffer = resource_map.get_buffer_resource(PassResources::LIGHT_INDICES.id);
        const BufferHandle count_buffer = resource_map.get_buffer_resource(PassResources::COUNT_BUFFER.id);
        const TextureHandle cluster_offsets = resource_map.get_texture_resource(PassResources::CLUSTER_OFFSETS.id);

        // Hmmm, shouldn't this be in copy?
        binning_constants.setup = cluster_grid_setup;
        binning_constants.indices_list_idx = gfx::buffers::get_shader_writable_index(indices_buffer);
        binning_constants.cluster_offsets_idx = gfx::textures::get_shader_writable_index(cluster_offsets);
        binning_constants.global_count_idx = gfx::buffers::get_shader_writable_index(count_buffer);
        gfx::buffers::update(binning_cb, &binning_constants, sizeof(binning_constants));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, clearing_resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, clearing_pso);

        u32 constants[] = {
            gfx::buffers::get_shader_writable_index(count_buffer),
            4,
            0
        };
        gfx::cmd::bind_compute_constants(cmd_ctx, constants, std::size(constants), 0);
        gfx::cmd::bind_compute_resource_table(cmd_ctx, 1);
        gfx::cmd::dispatch(cmd_ctx, 1, 1, 1);

        gfx::cmd::compute_write_barrier(cmd_ctx, count_buffer);

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso);

        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, binning_cb, u32(Slots::LIGHT_GRID_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(Slots::VIEW_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, scene_constants_buffer, u32(Slots::SCENE_CONSTANTS));

        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::READ_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_3D_TEXTURES_TABLE));

        u32 group_size = 8;
        u32 group_count_x = (binning_constants.setup.width + (group_size - 1)) / group_size;
        u32 group_count_y = (binning_constants.setup.height + (group_size - 1)) / group_size;
        u32 group_count_z = binning_constants.setup.pre_mid_depth + binning_constants.setup.post_mid_depth;

        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);
    };
}
