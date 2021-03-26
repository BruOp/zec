#pragma once
#include "gfx/gfx.h"
#include "gfx/render_system.h"
#include "../renderable_scene.h"
#include "../pass_resources.h"


namespace clustered
{
    class DebugPass : public zec::render_pass_system::IRenderPass
    {
    public:
        RenderableScene* renderable_scene = nullptr;
        zec::BufferHandle view_cb_handle = {};

        virtual ~DebugPass() = default;

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
        static constexpr char pass_name[] = "DebugPass";
        
        static constexpr zec::render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::DEPTH_TARGET.id,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };
        static constexpr zec::render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::SDR_TARGET.id,
                .name = PassResources::SDR_TARGET.name,
                .type = zec::render_pass_system::PassResourceType::TEXTURE,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = zec::BufferFormat::R8G8B8A8_UNORM_SRGB}
            }
        };

    private:
        enum struct BindingSlots : u32
        {
            VIEW_CONSTANT_BUFFER,
            SCENE_CONSTANT_BUFFER,
            RAW_BUFFERS_TABLE,
        };

        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso = {};
        zec::BufferHandle frustum_indices_buffer = {};
    };
}
