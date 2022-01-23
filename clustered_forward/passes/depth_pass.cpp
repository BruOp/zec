#include "depth_pass.h"
#include "utils/crc_hash.h"

using namespace zec;

namespace clustered
{
    enum struct BindingSlots : u32
    {
        PER_INSTANCE_CONSTANTS = 0,
        BUFFERS_DESCRIPTORS,
        VIEW_CONSTANT_BUFFER,
        RAW_BUFFERS_TABLE,
    };

    constexpr RenderPassResourceUsage outputs[] = {
        {
            .identifier = PassResources::DEPTH_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            .name = PassResources::DEPTH_TARGET.name,
        },
    };

    constexpr RenderPassResourceLayoutDesc resource_layout_descs[] = { {
        .identifier = ctcrc32("depth resource layout"),
        .resource_layout_desc = {
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 1,
            .tables = {
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        }
    } };

    const static RenderPassPipelineStateObjectDesc pipeline_descs[] = {
        {
            .identifier = ctcrc32("depth pipeline state object"),
            .resource_layout_id = resource_layout_descs[0].identifier,
            .shader_compilation_desc = {
                .used_stages = PIPELINE_STAGE_VERTEX,
                .shader_file_path = L"shaders/clustered_forward/depth_pass.hlsl",
            },
            .pipeline_desc = {
                .input_assembly_desc = { {
                    { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                } },
                .depth_stencil_state = {
                    .depth_cull_mode = ComparisonFunc::GREATER,
                    .depth_write = true,
                },
                .raster_state_desc = {
                    .cull_mode = CullMode::BACK_CCW,
                    .flags = DEPTH_CLIP_ENABLED,
                },
                .depth_buffer_format = BufferFormat::D32,
            }
        }
    };

    enum struct SettingsIDs : u32
    {
        MAIN_PASS_VIEW_CB = Settings::main_pass_view_cb.identifier,
        RENDERABLE_SCENE_PTR = Settings::renderable_scene_ptr.identifier,
    };

    constexpr SettingsDesc settings[] = {
        Settings::main_pass_view_cb,
        Settings::renderable_scene_ptr
    };

    void depth_pass_execution_fn(const RenderPassContext* context)
    {
        const CommandContextHandle cmd_ctx = context->cmd_context;
        const ResourceMap& resource_map = *context->resource_map;
        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.identifier);

        const TextureInfo& texture_info = gfx::textures::get_texture_info(depth_target);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        const ResourceLayoutHandle resource_layout = context->resource_layouts->at(resource_layout_descs[0].identifier);
        const PipelineStateHandle pso = context->pipeline_states->at(pipeline_descs[0].identifier);

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 0.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, nullptr, 0, depth_target);

        const BufferHandle view_cb_handle = context->settings->get_settings<BufferHandle>(u32(SettingsIDs::MAIN_PASS_VIEW_CB));
        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));

        const RenderableScene* scene_data = context->settings->get_settings<RenderableScene*>(u32(SettingsIDs::RENDERABLE_SCENE_PTR));
        const Renderables& renderables = scene_data->renderables;
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));

        u32 buffer_descriptor = gfx::buffers::get_shader_readable_index(renderables.vs_buffer);
        gfx::cmd::bind_graphics_constants(cmd_ctx, &buffer_descriptor, 1, u32(BindingSlots::BUFFERS_DESCRIPTORS));

        for (u32 i = 0; i < renderables.num_entities; i++) {
            gfx::cmd::bind_graphics_constants(cmd_ctx, &i, 1, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
            gfx::cmd::draw_mesh(cmd_ctx, renderables.meshes[i]);
        }
    };

    extern const RenderPassTaskDesc depth_pass_desc = {
        .name = "Depth Prepass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .execute_fn = &depth_pass_execution_fn,
        .resource_layout_descs = resource_layout_descs,
        .pipeline_descs = pipeline_descs,
        .settings = settings,
        .inputs = {},
        .outputs = outputs,
    };
}