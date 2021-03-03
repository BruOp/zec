#pragma once
#include "camera.h"
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../renderable_scene.h"
#include "../pass_resources.h"

namespace clustered
{
    class LightBinningPass : public zec::render_pass_system::IRenderPass
    {
    public:

        // Settings
        zec::PerspectiveCamera* camera = nullptr;
        zec::BufferHandle scene_constants_buffer = {};
        zec::BufferHandle view_cb_handle = {};
        ClusterGridSetup cluster_grid_setup = {};

        LightBinningPass(const ClusterGridSetup cluster_grid_setup);

        void set_output_dimensions(const ClusterGridSetup cluster_grid_setup);

        void setup() override final;

        void copy() override final
        {

        };

        void record(const zec::render_pass_system::ResourceMap& resource_map, zec::CommandContextHandle cmd_ctx) override final;

        // TODO!
        void shutdown() override final { }


        const char* get_name() const override final
        {
            return pass_name;
        }

        const zec::CommandQueueType get_queue_type() const override final
        {
            return command_queue_type;
        }

        const InputList get_input_list() const override final
        {
            return {
                .ptr = nullptr,
                .count = 0
            };
        };

        const OutputList get_output_list() const override final;

    private:
        enum struct Slots : u32
        {
            LIGHT_GRID_CONSTANTS = 0,
            VIEW_CONSTANTS,
            SCENE_CONSTANTS,
            READ_BUFFERS_TABLE,
            WRITE_BUFFERS_TABLE,
            WRITE_3D_TEXTURES_TABLE,
        };

        enum struct Outputs : u32
        {
            GLOBAL_COUNT,
            LIGHT_INDICES,
            CLUSTER_OFFSETS
        };

        struct BinningConstants
        {
            ClusterGridSetup setup;
            u32 indices_list_idx;
            u32 cluster_offsets_idx;
            u32 global_count_idx;
        };

        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso = {};
        zec::ResourceLayoutHandle clearing_resource_layout = {};
        zec::PipelineStateHandle clearing_pso = {};
        zec::BufferHandle binning_cb = {};

        BinningConstants binning_constants = {};

        static constexpr zec::CommandQueueType command_queue_type = zec::CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "Light Binning Pass";
        
        zec::render_pass_system::PassOutputDesc pass_outputs[3] = {
            {
                .id = PassResources::COUNT_BUFFER.id,
                    .name = PassResources::COUNT_BUFFER.name,
                    .type = zec::render_pass_system::PassResourceType::BUFFER,
                    .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
                    .buffer_desc = {
                        .type = zec::BufferType::RAW,
                        .byte_size = 4,
                        .stride = 4,
                }
            },
            {
                .id = PassResources::LIGHT_INDICES.id,
                .name = PassResources::LIGHT_INDICES.name,
                .type = zec::render_pass_system::PassResourceType::BUFFER,
                .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
                .buffer_desc = {
                    .type = zec::BufferType::RAW,
                    .byte_size = 0,
                    .stride = 4,
                    }
            },
            {
                .id = PassResources::CLUSTER_OFFSETS.id,
                .name = PassResources::CLUSTER_OFFSETS.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
                .texture_desc = {
                    .size_class = zec::render_pass_system::SizeClass::ABSOLUTE,
                    .format = zec::BufferFormat::UINT32_2,
                    .width = 0,
                    .height = 0,
                    .depth = 0,
                    .num_mips = 1,
                    .array_size = 1,
                    .is_cubemap = false,
                    .is_3d = true,
                },
            },
        };
    };
}
