//#pragma once
//#include "camera.h"
//#include "gfx/gfx.h"
//#include "gfx/render_system.h"
//#include "../renderable_scene.h"
//#include "../pass_resources.h"
//
//namespace clustered {    
//    class LightBinningPass : public zec::render_pass_system::IRenderPass
//    {
//    public:
//
//        // Settings
//        zec::PerspectiveCamera* camera = nullptr;
//        zec::BufferHandle scene_constants_buffer = {};
//        zec::BufferHandle view_cb_handle = {};
//
//        void setup() override final;
//
//        void copy() override final
//        {
//
//        };
//
//        void record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx) override final
//        {
//            const BufferHandle indices_buffer = resource_map.get_buffer_resource(PassResources::LIGHT_INDICES.id);
//            const BufferHandle count_buffer = resource_map.get_buffer_resource(PassResources::COUNT_BUFFER.id);
//            const TextureHandle cluster_offsets = resource_map.get_texture_resource(PassResources::CLUSTER_OFFSETS.id);
//
//            // Hmmm, shouldn't this be in copy?
//            BinningConstants binning_constants{
//                .grid_bins_x = CLUSTER_SETUP.width,
//                .grid_bins_y = CLUSTER_SETUP.height,
//                .grid_bins_z = CLUSTER_SETUP.depth,
//                .x_near = camera->aspect_ratio * tanf(0.5f * camera->vertical_fov) * camera->near_plane,
//                .y_near = tanf(0.5f * camera->vertical_fov) * camera->near_plane,
//                .z_near = -camera->near_plane,
//                .z_far = -camera->far_plane,
//                .indices_list_idx = gfx::buffers::get_shader_writable_index(indices_buffer),
//                .cluster_offsets_idx = gfx::textures::get_shader_writable_index(cluster_offsets),
//                .global_count_idx = gfx::buffers::get_shader_writable_index(count_buffer),
//            };
//            gfx::buffers::update(binning_cb, &binning_constants, sizeof(binning_constants));
//
//            gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
//            gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso);
//
//            gfx::cmd::bind_compute_constant_buffer(cmd_ctx, binning_cb, u32(Slots::LIGHT_GRID_CONSTANTS));
//            gfx::cmd::bind_compute_constant_buffer(cmd_ctx, view_cb_handle, u32(Slots::VIEW_CONSTANTS));
//            gfx::cmd::bind_compute_constant_buffer(cmd_ctx, scene_constants_buffer, u32(Slots::SCENE_CONSTANTS));
//
//            gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::READ_BUFFERS_TABLE));
//            gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_BUFFERS_TABLE));
//            gfx::cmd::bind_compute_resource_table(cmd_ctx, u32(Slots::WRITE_3D_TEXTURES_TABLE));
//
//            u32 group_size = 8;
//            u32 group_count_x = (CLUSTER_SETUP.width + (group_size - 1)) / group_size;
//            u32 group_count_y = (CLUSTER_SETUP.height + (group_size - 1)) / group_size;
//            u32 group_count_z = CLUSTER_SETUP.depth;
//
//            gfx::cmd::dispatch(cmd_ctx, group_count_x, group_count_y, group_count_z);
//        };
//
//        // TODO!
//        void shutdown() override final { }
//
//
//        const char* get_name() const override final
//        {
//            return pass_name;
//        }
//
//        const CommandQueueType get_queue_type() const override final
//        {
//            return command_queue_type;
//        }
//
//        const InputList get_input_list() const override final
//        {
//            return {
//                .ptr = nullptr,
//                .count = 0
//            };
//        };
//
//        const OutputList get_output_list() const override final
//        {
//            return {
//                .ptr = pass_outputs,
//                .count = std::size(pass_outputs),
//            };
//        }
//
//
//    private:
//
//        ResourceLayoutHandle resource_layout = {};
//        PipelineStateHandle pso = {};
//        BufferHandle binning_cb = {};
//
//        enum struct Slots : u32
//        {
//            LIGHT_GRID_CONSTANTS = 0,
//            VIEW_CONSTANTS,
//            SCENE_CONSTANTS,
//            READ_BUFFERS_TABLE,
//            WRITE_BUFFERS_TABLE,
//            WRITE_3D_TEXTURES_TABLE,
//        };
//        struct BinningConstants
//        {
//            u32 grid_bins_x = CLUSTER_SETUP.width;
//            u32 grid_bins_y = CLUSTER_SETUP.height;
//            u32 grid_bins_z = CLUSTER_SETUP.depth;
//
//            // Top right corner of the near plane
//            float x_near = 0.0f;
//            float y_near = 0.0f;
//            float z_near = 0.0f;
//            // The z-value for the far plane
//            float z_far = 0.0f;
//
//            u32 indices_list_idx;
//            u32 cluster_offsets_idx;
//            u32 global_count_idx;
//        };
//
//        static constexpr CommandQueueType command_queue_type = CommandQueueType::ASYNC_COMPUTE;
//        static constexpr char pass_name[] = "Light Binning Pass";
//        //static constexpr render_pass_system::PassInputDesc pass_inputs[] = {};
//        static const render_pass_system::PassOutputDesc pass_outputs[3];
//    };
//}
