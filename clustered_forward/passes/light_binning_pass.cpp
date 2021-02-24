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
    
        u32 num_clusters = binning_constants.grid_bins_x * binning_constants.grid_bins_y * binning_constants.grid_bins_z;
        pass_outputs[size_t(Outputs::LIGHT_INDICES)].buffer_desc.byte_size = ClusterGridSetup::MAX_LIGHTS_PER_BIN * num_clusters;

        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.width = float(binning_constants.grid_bins_x);
        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.height = float(binning_constants.grid_bins_y);
        pass_outputs[size_t(Outputs::CLUSTER_OFFSETS)].texture_desc.depth = binning_constants.grid_bins_z;
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
        binning_constants.grid_bins_x = cluster_grid_setup.width;
        binning_constants.grid_bins_y = cluster_grid_setup.height;
        binning_constants.grid_bins_z = cluster_grid_setup.depth;
        binning_constants.x_near = camera->aspect_ratio * tanf(0.5f * camera->vertical_fov) * camera->near_plane,
        binning_constants.y_near = tanf(0.5f * camera->vertical_fov) * camera->near_plane,
        binning_constants.z_near = -camera->near_plane,
        binning_constants.z_far = -camera->far_plane,
        binning_constants.indices_list_idx = gfx::buffers::get_shader_writable_index(indices_buffer);
        binning_constants.cluster_offsets_idx = gfx::textures::get_shader_writable_index(cluster_offsets);
        binning_constants.global_count_idx = gfx::buffers::get_shader_writable_index(count_buffer);
        gfx::buffers::update(binning_cb, &binning_constants, sizeof(binning_constants));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso);

        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, binning_cb, u32(Slots::LIGHT_GRID_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(Slots::VIEW_CONSTANTS));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, scene_constants_buffer, u32(Slots::SCENE_CONSTANTS));

        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::READ_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_BUFFERS_TABLE));
        gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_3D_TEXTURES_TABLE));

        u32 group_size = 8;
        u32 group_count_x = (binning_constants.grid_bins_x + (group_size - 1)) / group_size;
        u32 group_count_y = (binning_constants.grid_bins_y + (group_size - 1)) / group_size;
        u32 group_count_z = binning_constants.grid_bins_z;

        gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);
    };
}
