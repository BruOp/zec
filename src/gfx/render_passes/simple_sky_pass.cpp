#include "simple_sky_pass.h"
#include "../../geometry_helpers.h"

using namespace zec;

namespace zec
{
    enum struct BindingSlots : u32
    {
        CONSTANTS = 0,
        VIEW_CONSTANT_BUFFER,
        RESOURCE_TABLE
    };

    static constexpr RenderPassResourceUsage inputs[] = {
        {
            .identifier = PassResources::DEPTH_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            .name = PassResources::DEPTH_TARGET.name,
        },
    };

    constexpr auto TARGET = PassResources::HDR_TARGET;

    static constexpr RenderPassResourceUsage outputs[] = {
        {
            .identifier = TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
            .name = TARGET.name,
        },
    };

    static constexpr RenderPassResourceLayoutDesc resource_layout_descs[] = { {
        .identifier = ctcrc32("simple sky layout"),
        .resource_layout_desc = {
            .num_constants = 0,
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

    const static RenderPassPipelineStateObjectDesc pipeline_descs[] = { {
        .identifier = ctcrc32("simple sky pipeline state"),
        .resource_layout_id = resource_layout_descs[0].identifier,
        .pipeline_desc = {
            .input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = ComparisonFunc::GREATER_OR_EQUAL,
                .depth_write = false,
            },
            .raster_state_desc = {
                .cull_mode = CullMode::NONE,
                .flags = DEPTH_CLIP_ENABLED,
            },
            .rtv_formats = { { TARGET.desc.format } },
            .depth_buffer_format = BufferFormat::D32,
            .used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL,
            .shader_file_path = L"shaders/sky/simple_sky.hlsl"
        }
    } };


    enum struct SettingsIDs : u32
    {
        MAIN_PASS_VIEW_CB = RenderPassSharedSettings::main_pass_view_cb.identifier,
        FULLSCREEN_QUAD = RenderPassSharedSettings::fullscreen_quad.identifier,
    };


    static constexpr SettingsDesc settings[] = {
        RenderPassSharedSettings::main_pass_view_cb,
        RenderPassSharedSettings::fullscreen_quad,
    };

    static void simple_sky_pass_execution(const RenderPassContext* context)
    {
        const ResourceMap& resource_map = *context->resource_map;

        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.identifier);
        TextureHandle hdr_buffer = resource_map.get_texture_resource(TARGET.identifier);
        const TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        const CommandContextHandle cmd_ctx = context->cmd_context;
        const ResourceLayoutHandle resource_layout = context->resource_layouts->at(resource_layout_descs[0].identifier);
        const PipelineStateHandle pso = context->pipeline_states->at(pipeline_descs[0].identifier);

        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
        gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        const BufferHandle view_cb_handle = context->settings->get_settings<BufferHandle>(u32(SettingsIDs::MAIN_PASS_VIEW_CB));
        const MeshHandle fullscreen_mesh = context->settings->get_settings<MeshHandle>(u32(SettingsIDs::FULLSCREEN_QUAD));

        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RESOURCE_TABLE));

        gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
    };

    extern const RenderPassTaskDesc simple_sky_pass_desc = {
        .name = "Simple Sky Pass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .setup_fn = nullptr,
        .execute_fn = &simple_sky_pass_execution,
        .resource_layout_descs = resource_layout_descs,
        .pipeline_descs = pipeline_descs,
        .settings = settings,
        .inputs = inputs,
        .outputs = outputs,
    };
}
