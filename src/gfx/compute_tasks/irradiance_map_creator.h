#pragma once
#include "gfx/gfx.h"

namespace zec {
    
    struct IrradiancePassConstants
    {
        u32 src_texture_idx = UINT32_MAX;
        u32 out_texture_idx = UINT32_MAX;
        u32 src_img_width = 0;
    };
    
    class IrradianceMapCreator
    {
    public:
        struct RenderPassSharedSettings
        {
            zec::TextureHandle src_texture = {};
            zec::TextureHandle out_texture = {};
        };

        void init();

        void record(zec::CommandContextHandle cmd_ctx, const RenderPassSharedSettings& settings);

    private:

        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso_handle = {};
    };
}
