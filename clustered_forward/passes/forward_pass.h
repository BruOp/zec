#pragma once
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../renderable_scene.h"
#include "../pass_resources.h"
#include <iterator>

namespace clustered
{
    class ForwardPass : public zec::render_pass_system::IRenderPass
    {
    public:
        zec::PerspectiveCamera* camera = nullptr;
        Renderables* scene_renderables = nullptr;
        zec::BufferHandle scene_constants_buffer = {};
        zec::BufferHandle view_cb_handle = {};
        ClusterGridSetup cluster_grid_setup = {};

        virtual void setup() final;

        void copy() override final
        {
            // Shrug
        };

        void record(const zec::render_pass_system::ResourceMap& resource_map, zec::CommandContextHandle cmd_ctx) override final;

        void shutdown() override final
        {
            // TODO: Free pipeline and resource layouts
        };

        const char* get_name() const override final
        {
            return pass_name;
        }

        const InputList get_input_list() const override final
        {
            return {
                .ptr = pass_inputs,
                .count = std::size(pass_inputs)
            };
        };

        const OutputList get_output_list() const override final
        {
            return {
                .ptr = pass_outputs,
                .count = std::size(pass_outputs),
            };
        }

        const zec::CommandQueueType get_queue_type() const override final
        {
            return command_queue_type;
        }

        static constexpr zec::CommandQueueType command_queue_type = zec::CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "ForwardPass";
        static constexpr zec::render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::DEPTH_TARGET.id,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
            },
            {
                .id = PassResources::LIGHT_INDICES.id,
                .type = zec::render_pass_system::PassResourceType::BUFFER,
                .usage = zec::RESOURCE_USAGE_SHADER_READABLE,
            },
        };
        static constexpr zec::render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .name = PassResources::HDR_TARGET.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = zec::BufferFormat::R16G16B16A16_FLOAT}
            }
        };

    private:
        enum struct BindingSlots : u32
        {
            PER_INSTANCE_CONSTANTS = 0,
            BUFFERS_DESCRIPTORS,
            VIEW_CONSTANT_BUFFER,
            SCENE_CONSTANT_BUFFER,
            LIGHT_GRID_CONSTANTS,
            RAW_BUFFERS_TABLE,
            TEXTURE_2D_TABLE,
            TEXTURE_CUBE_TABLE,
        };

        struct BinningConstants
        {
            ClusterGridSetup setup;
            u32 indices_list_idx;
        };

        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso = {};
        zec::BufferHandle binning_cb = {};

        BinningConstants binning_constants = {};

    };
}
