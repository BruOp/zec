//#include "debug_pass.h"
//
//using namespace zec;
//
//namespace clustered
//{
//void DebugPass::setup()
//    {
//        ResourceLayoutDesc layout_desc{
//            .constants = { },
//            .num_constants = 0,
//            .constant_buffers = {
//                { ShaderVisibility::VERTEX },
//                { ShaderVisibility::VERTEX },
//            },
//            .num_constant_buffers = 2,
//            .tables = {
//                {.usage = ResourceAccess::READ },
//            },
//            .num_resource_tables = 1,
//            .num_static_samplers = 0,
//        };
//
//        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
//        gfx::set_debug_name(resource_layout, L"Debug Layout");
//
//        // Create the Pipeline State Object
//        PipelineStateObjectDesc pipeline_desc = {
//            .resource_layout = resource_layout,
//            .input_assembly_desc = { { } },
//            .depth_stencil_state = {
//                .depth_cull_mode = ComparisonFunc::GREATER,
//                .depth_write = false,
//            },
//            .raster_state_desc = {
//                .fill_mode = FillMode::WIREFRAME,
//                .cull_mode = CullMode::NONE,
//                .flags = DEPTH_CLIP_ENABLED,
//            },
//            .depth_buffer_format = BufferFormat::D32,
//            .used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL,
//            .shader_file_path = L"shaders/clustered_forward/spot_light_debug.hlsl",
//        };
//        pipeline_desc.rtv_formats[0] = pass_outputs[0].texture_desc.format;
//
//        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
//        gfx::set_debug_name(pso, L"Debug Pipeline");
//
//
//        constexpr u16 frustum_indices[]{
//            0, 1, 2,
//            0, 3, 1,
//            0, 4, 3,
//            0, 2, 4,
//            1, 3, 2,
//            3, 4, 2
//        };
//        frustum_indices_buffer = gfx::buffers::create(BufferDesc{
//            .usage = RESOURCE_USAGE_INDEX,
//            .type = BufferType::DEFAULT,
//            .byte_size = sizeof(frustum_indices),
//            .stride = sizeof(frustum_indices[0]) });
//
//        CommandContextHandle cmd_ctx = gfx::cmd::provision(CommandQueueType::GRAPHICS);
//        gfx::buffers::set_data(cmd_ctx, frustum_indices_buffer, frustum_indices, sizeof(frustum_indices));
//
//        CmdReceipt receipt = gfx::cmd::return_and_execute(&cmd_ctx, 1);
//        gfx::cmd::gpu_wait(CommandQueueType::GRAPHICS, receipt);
//    };
//
//    void DebugPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
//    {
//        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
//
//        TextureHandle depth_target = resource_map.get_texture_resource(pass_inputs[0].id);
//        TextureHandle render_target = resource_map.get_texture_resource(pass_outputs[0].id);
//        const TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
//        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
//        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };
//
//        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
//        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso);
//        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
//        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);
//
//        gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);
//
//        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
//        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, renderable_scene->scene_constants, u32(BindingSlots::SCENE_CONSTANT_BUFFER));
//        
//        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));
//
//        // Render spot light bounding frustums
//        gfx::cmd::draw_mesh(cmd_ctx, frustum_indices_buffer, renderable_scene->scene_constant_data.num_spot_lights);
//    };
//}
