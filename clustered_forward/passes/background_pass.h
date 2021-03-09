#pragma once
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../pass_resources.h"

namespace clustered
{
    class BackgroundPass : public zec::render_pass_system::IRenderPass
    {
    public:
        zec::BufferHandle view_cb_handle = {};
        zec::TextureHandle cube_map_buffer = {};
        float mip_level = 1.0f;

        void setup() override final;

        void copy() override final { };

        void record(const zec::render_pass_system::ResourceMap& resource_map, zec::CommandContextHandle cmd_ctx) override final;

        void shutdown() override final
        {
            // TODO
        };

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


    private:
        enum struct BindingSlots : u32
        {
            CONSTANTS = 0,
            VIEW_CONSTANT_BUFFER,
            RESOURCE_TABLE
        };

        zec::MeshHandle fullscreen_mesh = {};
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso_handle = {};

        static constexpr zec::CommandQueueType command_queue_type = zec::CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "BackgroundPass";
        static constexpr zec::render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::DEPTH_TARGET.id,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };
        static constexpr zec::render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .name = PassResources::HDR_TARGET.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = zec::BufferFormat::R16G16B16A16_FLOAT}
            },
        };
    };
}
