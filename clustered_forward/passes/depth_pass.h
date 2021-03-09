#pragma once
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../renderable_scene.h"
#include "../pass_resources.h"

namespace clustered
{
    class DepthPass : public zec::render_pass_system::IRenderPass
    {
    public:
        Renderables* scene_renderables = nullptr;
        zec::BufferHandle view_cb_handle = {};

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
                .ptr = nullptr,
                .count = 0
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
        static constexpr char pass_name[] = "Depth Prepass";
        static constexpr zec::render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::DEPTH_TARGET.id,
                .name = PassResources::DEPTH_TARGET.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
                .texture_desc = zec::render_pass_system::PassTextureResource{
                    .format = zec::BufferFormat::D32,
                    .clear_depth = 0.0f,
                    .clear_stencil = 0,
                }
            },
        };

    private:
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso = {};

        enum struct BindingSlots : u32
        {
            PER_INSTANCE_CONSTANTS = 0,
            BUFFERS_DESCRIPTORS,
            VIEW_CONSTANT_BUFFER,
            RAW_BUFFERS_TABLE,
        };
    };
}
