#include "light_binning_pass.h"
#include "../renderable_scene.h"
#include "gfx/gfx.h"
#include "utils/crc_hash.h"

using namespace zec;

namespace clustered
{
    enum struct BindingSlots : u32
    {
        CLUSTER_GRID_CONSTANTS = 0,
        VIEW_CONSTANTS,
        SCENE_CONSTANTS,
        READ_BUFFERS_TABLE,
        WRITE_BUFFERS_TABLE,
    };

    struct LightBinningPassData
    {
        BufferHandle cluster_grid_cb;
    };
    static_assert(sizeof(LightBinningPassData) < sizeof(PerPassData));

    struct ClusterGridConstants
    {
        ClusterGridSetup setup;
        u32 spot_light_indices_list_idx;
        u32 point_light_indices_list_idx;
    };

    constexpr RenderPassResourceUsage outputs[] = {
        {
            .identifier = PassResources::SPOT_LIGHT_INDICES.identifier,
            .type = PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
            .name = PassResources::SPOT_LIGHT_INDICES.name,
        },
        {
            .identifier = PassResources::POINT_LIGHT_INDICES.identifier,
            .type = PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
            .name = PassResources::POINT_LIGHT_INDICES.name,
        }
    };

    constexpr RenderPassResourceLayoutDesc resource_layout_descs[] = { {
        .identifier = ctcrc32("light binning layout"),
        .resource_layout_desc = {
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
        }
    } };

    const static RenderPassPipelineStateObjectDesc pipeline_descs[] = {
        {
            .identifier = ctcrc32("spot light assighment pipeline state"),
            .resource_layout_id = resource_layout_descs[0].identifier,
            .shader_compilation_desc = {
                .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
                .shader_file_path = L"shaders/clustered_forward/spot_light_assignment.hlsl",
            },
            .pipeline_desc = {},
        },
        {
            .identifier = ctcrc32("point light assighment pipeline state"),
            .resource_layout_id = resource_layout_descs[0].identifier,
            .shader_compilation_desc = {
                .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
                .shader_file_path = L"shaders/clustered_forward/point_light_assignment.hlsl",
            },
            .pipeline_desc = {},
        }
    };

    enum struct SettingsIDs : u32
    {
        MAIN_PASS_VIEW_CB = Settings::main_pass_view_cb.identifier,
        RENDERABLE_SCENE_PTR = Settings::renderable_scene_ptr.identifier,
        CLUSTER_GRID_SETUP = Settings::cluster_grid_setup.identifier,
    };

    constexpr SettingsDesc settings[] = {
        Settings::main_pass_view_cb,
        Settings::renderable_scene_ptr,
        Settings::cluster_grid_setup,
    };

    static void setup(PerPassData* per_pass_data)
    {
        LightBinningPassData* pass_data = reinterpret_cast<LightBinningPassData*>(per_pass_data);
        pass_data->cluster_grid_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ClusterGridConstants),
            .stride = 0 });
        gfx::set_debug_name(pass_data->cluster_grid_cb, L"Cluster Grid Constants");
    }

    void light_binning_execution(const RenderPassContext* context)
    {
        const CommandContextHandle cmd_ctx = context->cmd_context;
        const ResourceMap& resource_map = *context->resource_map;
        const LightBinningPassData pass_data = *reinterpret_cast<LightBinningPassData*>(context->per_pass_data);
        const ResourceLayoutHandle resource_layout = context->resource_layouts->at(resource_layout_descs[0].identifier);
        const PipelineStateHandle spot_light_pso = context->pipeline_states->at(pipeline_descs[0].identifier);
        const PipelineStateHandle point_light_pso = context->pipeline_states->at(pipeline_descs[1].identifier);

        const BufferHandle spot_light_indices_buffer = resource_map.get_buffer_resource(PassResources::SPOT_LIGHT_INDICES.identifier);
        const BufferHandle point_light_indices_buffer = resource_map.get_buffer_resource(PassResources::POINT_LIGHT_INDICES.identifier);

        const ClusterGridSetup cluster_grid_setup = context->settings->get_settings<ClusterGridSetup>(u32(SettingsIDs::CLUSTER_GRID_SETUP));
        ClusterGridConstants binning_constants = {
            .setup = cluster_grid_setup,
            .spot_light_indices_list_idx = gfx::buffers::get_shader_writable_index(spot_light_indices_buffer),
            .point_light_indices_list_idx = gfx::buffers::get_shader_writable_index(point_light_indices_buffer),
        };
        gfx::buffers::update(pass_data.cluster_grid_cb, &binning_constants, sizeof(binning_constants));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, spot_light_pso);

        const BufferHandle view_cb_handle = context->settings->get_settings<BufferHandle>(u32(SettingsIDs::MAIN_PASS_VIEW_CB));
        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANTS));

        gfx::cmd::bind_compute_constant_buffer(cmd_ctx, pass_data.cluster_grid_cb, u32(BindingSlots::CLUSTER_GRID_CONSTANTS));

        const RenderableScene* renderable_scene = context->settings->get_settings<RenderableScene*>(u32(SettingsIDs::RENDERABLE_SCENE_PTR));
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

    extern const RenderPassTaskDesc light_binning_desc = {
        .name = "Light Binning Pass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .setup_fn = &setup,
        .execute_fn = &light_binning_execution,
        .resource_layout_descs = resource_layout_descs,
        .pipeline_descs = pipeline_descs,
        .settings = settings,
        .inputs = {},
        .outputs = outputs,
    };
}
