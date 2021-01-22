#include "pch.h"
#include "passes.h"
#include "gfx/ui.h"

using namespace zec;

namespace clustered::DebugPass
{
    RenderSystem::RenderPassDesc generate_desc(Context* context)
    {
        RenderSystem::RenderPassDesc desc = {
            .queue_type = CommandQueueType::GRAPHICS,
            .inputs = {},
            .outputs = {
                {
                    .name = ResourceNames::SDR_BUFFER,
                    .type = RenderSystem::PassResourceType::TEXTURE,
                    .usage = RESOURCE_USAGE_RENDER_TARGET,
                    .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}
                }
            },
            .context = context,
            .setup = &setup,
            .execute = &record,
            .destroy = &destroy,
        };
        strcpy(desc.name, PassNames::Debug);
        return desc;
    }

    void setup(void* context)
    {
        //Context* pass_context = reinterpret_cast<Context*>(context);

        //pass_context->debug_frustum_cb_handle = gfx::buffers::create({
        //    .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
        //    .type = BufferType::DEFAULT,
        //    .byte_size = sizeof(FrustumDrawData),
        //    .stride = 0,
        //    });

        //pass_context->debug_view_cb_handle = gfx::buffers::create({
        //    .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
        //    .type = BufferType::DEFAULT,
        //    .byte_size = sizeof(ViewConstantData),
        //    .stride = 0,
        //    });

        //const ClusterSetup& cl_setup = pass_context->cluster_setup;
        //// Each dimension is sliced width/height/depth times, plus 1 for the final boundary we want to represent
        //// Each slice gets four positions
        //u32 num_slices = (cl_setup.width + cl_setup.height + cl_setup.depth + 3);
        //Array<vec3> frustum_positions;

        //const PerspectiveCamera* camera = pass_context->camera;
        //geometry::generate_frustum_data(frustum_positions, camera->near_plane, camera->far_plane, camera->vertical_fov, camera->aspect_ratio);

        //// Each slice requires 6 indices
        //MeshDesc mesh_desc{
        //    .index_buffer_desc = {
        //        .usage = RESOURCE_USAGE_INDEX,
        //        .type = BufferType::DEFAULT,
        //        .byte_size = sizeof(geometry::k_cube_indices),
        //        .stride = sizeof(geometry::k_cube_indices[0]),
        //        .data = (void*)(geometry::k_cube_indices),
        //     },
        //     .vertex_buffer_descs = {{
        //        .usage = RESOURCE_USAGE_VERTEX,
        //        .type = BufferType::DEFAULT,
        //        .byte_size = u32(sizeof(frustum_positions[0]) * frustum_positions.size),
        //        .stride = sizeof(frustum_positions[0]),
        //        .data = frustum_positions.data
        //     }}
        //};
        //pass_context->debug_frustum_mesh = gfx::meshes::create(mesh_desc);

        //ResourceLayoutDesc layout_desc{
        //   .num_constants = 0,
        //   .constant_buffers = {
        //       { ShaderVisibility::ALL },
        //       { ShaderVisibility::ALL },
        //   },
        //   .num_constant_buffers = 2,
        //   .num_resource_tables = 0,
        //   .num_static_samplers = 0,
        //};

        //pass_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        //// Create the Pipeline State Object
        //PipelineStateObjectDesc pipeline_desc = {};
        //pipeline_desc.input_assembly_desc = { {
        //    { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
        //} };
        //pipeline_desc.shader_file_path = L"shaders/clustered_forward/debug_shader.hlsl";
        //pipeline_desc.rtv_formats[0] = BufferFormat::R8G8B8A8_UNORM_SRGB;
        //pipeline_desc.resource_layout = pass_context->resource_layout;
        //pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        //pipeline_desc.raster_state_desc.fill_mode = FillMode::WIREFRAME;
        //pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::OFF;
        //pipeline_desc.depth_stencil_state.depth_write = false;
        //pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;
        //pipeline_desc.topology_type = TopologyType::TRIANGLE;
        //pass_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        //gfx::pipelines::set_debug_name(pass_context->pso_handle, L"Debug Pipeline");
    }

    void copy(void* context)
    {
        //Context* pass_context = reinterpret_cast<Context*>(context);
        //ViewConstantData debug_view_data{
        //    .VP = pass_context->debug_camera->projection * pass_context->debug_camera->view ,
        //    .invVP = invert(pass_context->debug_camera->view * mat4(to_mat3(pass_context->debug_camera->view), {})),
        //    .camera_position = pass_context->debug_camera->position,
        //    .time = 1.0f,
        //};
        //gfx::buffers::update(pass_context->debug_view_cb_handle, &debug_view_data, sizeof(debug_view_data));

        //FrustumDrawData frustum_draw_data = {
        //    .inv_view = pass_context->camera->invView,
        //    .color = vec4(0.5f, 0.5f, 1.0f, 1.0f),
        //};
        //gfx::buffers::update(pass_context->debug_frustum_cb_handle, &frustum_draw_data, sizeof(frustum_draw_data));
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Context* pass_context = reinterpret_cast<Context*>(context);

        //TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle sdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, sdr_buffer, clear_color);
        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_frustum_cb_handle, 0);
        //gfx::cmd::bind_constant_buffer(cmd_ctx, pass_context->debug_view_cb_handle, 1);

        gfx::cmd::draw_mesh(cmd_ctx, pass_context->debug_frustum_mesh);
    };

    void destroy(void* context)
    {

    }
}


namespace clustered::UIPass
{
    zec::RenderSystem::RenderPassDesc generate_desc(Context* context)
    {
        zec::RenderSystem::RenderPassDesc desc{
            .queue_type = CommandQueueType::GRAPHICS,
            .inputs = {},
            .outputs = {
                {
                    .name = ResourceNames::SDR_BUFFER,
                    .type = RenderSystem::PassResourceType::TEXTURE,
                    .usage = RESOURCE_USAGE_RENDER_TARGET,
                    .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}
                }
            },
            .context = context,
            .setup = &setup,
            .execute = &record,
            .destroy = &destroy,
        };
        strcpy(desc.name, PassNames::UI);
        return desc;
    }

    void setup(void* context)
    { }

    void copy(void* context)
    { }

    void record(zec::RenderSystem::RenderList& render_list, zec::CommandContextHandle cmd_ctx, void* context)
    {
        Context* pass_context = reinterpret_cast<Context*>(context);
        TextureHandle sdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);

        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        gfx::cmd::clear_render_target(cmd_ctx, sdr_buffer, clear_color);
        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        ui::begin_frame();

        ImGui::ShowDemoWindow();
        ImGui::ShowMetricsWindow();


        ImGui::Begin("Camera");

        auto& camera_names = pass_context->scene->camera_names;
        static int item_current = int(*pass_context->active_camera_idx);

        ImGui::Text("Active Camera");
        ImVec2 child_size{
            ImGui::GetContentRegionAvailWidth(),
            100
        };


        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 100));
        ImGui::BeginChild("Cameras", child_size, true, ImGuiWindowFlags_None);

        for (size_t i = 0; i < camera_names.size(); i++) {
            if (ImGui::Selectable(camera_names[i].c_str(), *pass_context->active_camera_idx == i)) {
                // Handle selection
                *pass_context->active_camera_idx = i;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::End();

        ui::end_frame(cmd_ctx);
    }

    void destroy(void* context)
    { }
}
