#pragma once
#include "gfx/gfx.h"

namespace clustered
{
    class BRDFLutCreator
    {
    public:
        struct Settings
        {
            zec::TextureHandle out_texture = {};
        };

        void init();

        void record(zec::CommandContextHandle cmd_ctx, const Settings& settings);

    private:
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso_handle = {};
    };
}
