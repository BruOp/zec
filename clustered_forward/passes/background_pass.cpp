//#include "background_pass.h"
//#include "../bounding_meshes.h"
//
//using namespace zec;
//
//namespace clustered
//{
//    void BackgroundPass::setup()
//    {
//        // Create fullscreen quad
//        MeshDesc fullscreen_desc{
//            .index_buffer_desc = {
//                .usage = RESOURCE_USAGE_INDEX,
//                .type = BufferType::DEFAULT,
//                .byte_size = sizeof(geometry::k_fullscreen_indices),
//                .stride = sizeof(geometry::k_fullscreen_indices[0]),
//            },
//            .vertex_buffer_descs = {
//                {
//                    .usage = RESOURCE_USAGE_VERTEX,
//                    .type = BufferType::DEFAULT,
//                    .byte_size = sizeof(geometry::k_fullscreen_positions),
//                    .stride = 3 * sizeof(geometry::k_fullscreen_positions[0]),
//                },
//                {
//                    .usage = RESOURCE_USAGE_VERTEX,
//                    .type = BufferType::DEFAULT,
//                    .byte_size = sizeof(geometry::k_fullscreen_uvs),
//                    .stride = 2 * sizeof(geometry::k_fullscreen_uvs[0]),
//                },
//            },
//            .index_buffer_data = geometry::k_fullscreen_indices,
//            .vertex_buffer_data = {
//                geometry::k_fullscreen_positions,
//                geometry::k_fullscreen_uvs
//            }
//        };
//        CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);
//        fullscreen_mesh = gfx::meshes::create(copy_ctx, fullscreen_desc);
//        gfx::cmd::cpu_wait(gfx::cmd::return_and_execute(&copy_ctx, 1));
//
//        ResourceLayoutDesc layout_desc{
//            .constants = {
//                {.visibility = ShaderVisibility::PIXEL, .num_constants = 2 },
//             },
//            .num_constants = 1,
//            .constant_buffers = {
//                { ShaderVisibility::ALL },
//            },
//            .num_constant_buffers = 1,
//            .tables = {
//                {.usage = ResourceAccess::READ },
//            },
//            .num_resource_tables = 1,
//            .static_samplers = {
//                {
//                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
//                    .wrap_u = SamplerWrapMode::WRAP,
//                    .wrap_v = SamplerWrapMode::WRAP,
//                    .binding_slot = 0,
//                },
//            },
//            .num_static_samplers = 1,
//        };
//
//        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
//
//        // Create the Pipeline State Object
//        PipelineStateObjectDesc pipeline_desc = {
//            .shader_file_path = L"shaders/clustered_forward/background_pass.hlsl"
//        };
//        pipeline_desc.input_assembly_desc = { {
//            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
//            { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
//        } };
//        pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
//        pipeline_desc.depth_buffer_format = BufferFormat::D32;
//        pipeline_desc.resource_layout = resource_layout;
//        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
//        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
//        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::GREATER_OR_EQUAL;
//        pipeline_desc.depth_stencil_state.depth_write = false;
//        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
//
//        pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
//    }
//
//    void BackgroundPass::record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx)
//    {
//        TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.id);
//        TextureHandle hdr_buffer = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
//        const TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
//        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
//        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };
//
//        gfx::cmd::set_graphics_resource_layout(cmd_ctx, resource_layout);
//        gfx::cmd::set_graphics_pipeline_state(cmd_ctx, pso_handle);
//        gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);
//        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
//        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);
//
//        struct Constants
//        {
//            u32 cube_map_idx;
//            float mip_level;
//        };
//        Constants constants = {
//            .cube_map_idx = gfx::textures::get_shader_readable_index(cube_map_buffer),
//            .mip_level = mip_level,
//        };
//        gfx::cmd::bind_graphics_constants(cmd_ctx, &constants, 2, u32(BindingSlots::CONSTANTS));
//        gfx::cmd::bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
//        gfx::cmd::bind_graphics_resource_table(cmd_ctx, u32(BindingSlots::RESOURCE_TABLE));
//
//        gfx::cmd::draw_mesh(cmd_ctx, fullscreen_mesh);
//    }
//}