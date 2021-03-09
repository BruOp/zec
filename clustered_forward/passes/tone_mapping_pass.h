#pragma once
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../pass_resources.h"

namespace clustered
{
    struct TonemapPassConstants
    {
        u32 src_texture;
        float exposure;
    };

    class ToneMappingPass : public zec::render_pass_system::IRenderPass
    {
    public:
        // Settings
        float exposure = 1.0f;

        void setup() override final;

        void copy() override final { };

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

        zec::MeshHandle fullscreen_mesh;
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso = {};

        static constexpr zec::CommandQueueType command_queue_type = zec::CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "Tone Mapping Pass";
        static constexpr zec::render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_SHADER_READABLE,
            },
        };
        static constexpr zec::render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::SDR_TARGET.id,
                .name = PassResources::SDR_TARGET.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = zec::BufferFormat::R8G8B8A8_UNORM_SRGB}
            },
        };
    };
}
