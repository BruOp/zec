#pragma once
#include "gfx/gfx.h"

namespace zec
{
    class BRDFLutCreator
    {
    public:
        struct RenderPassSharedSettings
        {
            zec::TextureHandle out_texture = {};
        };

        void init();

        void record(zec::CommandContextHandle cmd_ctx, const RenderPassSharedSettings& settings);

    private:
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso_handle = {};
    };
}
