#include "forward_pass.h"

namespace clustered
{
    using namespace zec;

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
    static_assert(sizeof(ForwardPassData) < sizeof(PerPassData));

    static constexpr RenderPassResourceUsage inputs[] = {
        {
            .identifier = PassResources::DEPTH_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            .name = PassResources::DEPTH_TARGET.name,
        },
        {
            .identifier = PassResources::SPOT_LIGHT_INDICES.identifier,
            .type = PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_SHADER_READABLE,
            .name = PassResources::SPOT_LIGHT_INDICES.name,
        },
        {
            .identifier = PassResources::POINT_LIGHT_INDICES.identifier,
            .type = PassResourceType::BUFFER,
            .usage = RESOURCE_USAGE_SHADER_READABLE,
            .name = PassResources::POINT_LIGHT_INDICES.name,
        }
    };

    static constexpr RenderPassResourceUsage outputs[] = {
        {
            .identifier = PassResources::HDR_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = PassResources::HDR_TARGET.name,
        },
    };

    static constexpr RenderPassResourceLayoutDesc resource_layout_descs[] = { {
        .identifier = ctcrc32("Forward pass resource layout"),
        .resource_layout_desc = {
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
                {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { ShaderVisibility::ALL },
                { ShaderVisibility::PIXEL },
                { ShaderVisibility::PIXEL },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 3,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::ANISOTROPIC,
                    .wrap_u = SamplerWrapMode::WRAP,
                    .wrap_v = SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
                {
                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_POINT,
                    .wrap_u = SamplerWrapMode::CLAMP,
                    .wrap_v = SamplerWrapMode::CLAMP,
                    .binding_slot = 1,
                },
            },
            .num_static_samplers = 2,
        },
    } };

    static constexpr RenderPassPipelineStateObjectDesc pipeline_descs[] = { {
        .identifier = ctcrc32("Forward pass opaque pipeline desc"),
        .resource_layout_id = resource_layout_descs[0].identifier,
        .shader_compilation_desc = {
            .used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL,
            .shader_file_path = L"shaders/clustered_forward/mesh_forward.hlsl",
        },
        .pipeline_desc = {
            .input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = ComparisonFunc::EQUAL,
                .depth_write = true,
            },
            .raster_state_desc = {
                .cull_mode = CullMode::BACK_CCW,
                .flags = DEPTH_CLIP_ENABLED,
            },
            .rtv_formats = { BufferFormat::R16G16B16A16_FLOAT },
            .depth_buffer_format = BufferFormat::D32,
        }
    } };

    static constexpr SettingsDesc settings[] = {
        Settings::main_pass_view_cb,
        Settings::renderable_scene_ptr,
        Settings::cluster_grid_setup,
    };

    static void forward_pass_setup(PerPassData* per_pass_data)
    {
        ForwardPassData* pass_data = reinterpret_cast<ForwardPassData*>(per_pass_data);
        pass_data->cluster_grid_cb = gfx::buffers::create({
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ClusterGridConstants),
            .stride = 0 });
        gfx::set_debug_name(pass_data->cluster_grid_cb, L"Forward Pass Binning Constants");
    };

    static void forward_pass_execution(const RenderPassContext* context)
    {
        const ResourceMap& resource_map = *context->resource_map;
        const ForwardPassData pass_data = *reinterpret_cast<ForwardPassData*>(context->per_pass_data);

        const BufferHandle spot_light_indices_buffer = resource_map.get_buffer_resource(PassResources::SPOT_LIGHT_INDICES.identifier);
        const BufferHandle point_light_indices_buffer = resource_map.get_buffer_resource(PassResources::POINT_LIGHT_INDICES.identifier);

        // Hmmm, shouldn't this be in copy?
        const CommandContextHandle cmd_ctx = context->cmd_context;

        const ClusterGridSetup cluster_grid_setup = context->settings->get_settings<ClusterGridSetup>(Settings::cluster_grid_setup.identifier);

        ClusterGridConstants binning_constants = {
            .setup = cluster_grid_setup,
            .spot_light_indices_list_idx = gfx::buffers::get_shader_readable_index(spot_light_indices_buffer),
            .point_light_indices_list_idx = gfx::buffers::get_shader_readable_index(point_light_indices_buffer),
        };
        gfx::buffers::update(pass_data.cluster_grid_cb, &binning_constants, sizeof(binning_constants));

        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.identifier);
        TextureHandle render_target = resource_map.get_texture_resource(PassResources::HDR_TARGET.identifier);
        const TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        const ResourceLayoutHandle resource_layout = context->resource_layouts->at(resource_layout_descs[0].identifier);
        const PipelineStateHandle pso = context->pipeline_states->at(pipeline_descs[0].identifier);

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);


        const RenderableScene* scene_data = context->settings->get_settings<RenderableScene*>(Settings::renderable_scene_ptr.identifier);
        const BufferHandle view_cb_handle = context->settings->get_settings<BufferHandle>(Settings::main_pass_view_cb.identifier);
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, scene_data->scene_constants, u32(BindingSlots::SCENE_CONSTANT_BUFFER));
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, pass_data.cluster_grid_cb, u32(BindingSlots::CLUSTER_GRID_CONSTANTS));

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

    extern const zec::RenderPassTaskDesc forward_pass_desc{
        .name = "Forward Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .setup_fn = &forward_pass_setup,
        .execute_fn = &forward_pass_execution,
        .resource_layout_descs = resource_layout_descs,
        .pipeline_descs = pipeline_descs,
        .settings = settings,
        .inputs = inputs,
        .outputs = outputs,
    };
}
