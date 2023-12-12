#pragma once
#include "rhi_public_resources.h"

namespace zec
{
    enum struct Samplers : u8
    {
        POINT_WRAP,
        POINT_MIRROR,
        POINT_CLAMP,
        LINEAR_WRAP,
        LINEAR_MIRROR,
        LINEAR_CLAMP,
        ANISOTROPIC_WRAP,
        ANISOTROPIC_MIRROR,
        ANISOTROPIC_CLAMP,
        // TODO: Comparison samplers
        COUNT,
    };

    constexpr rhi::SamplerDesc k_sampler_descs[size_t(Samplers::COUNT)] =
    {
        // POINT_WRAP,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
            .wrap_u = rhi::SamplerWrapMode::WRAP,
            .wrap_v = rhi::SamplerWrapMode::WRAP,
            .wrap_w = rhi::SamplerWrapMode::WRAP,
        },
        // POINT_MIRROR,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
            .wrap_u = rhi::SamplerWrapMode::MIRROR,
            .wrap_v = rhi::SamplerWrapMode::MIRROR,
            .wrap_w = rhi::SamplerWrapMode::MIRROR,
        },
        // POINT_CLAMP,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
            .wrap_u = rhi::SamplerWrapMode::CLAMP,
            .wrap_v = rhi::SamplerWrapMode::CLAMP,
            .wrap_w = rhi::SamplerWrapMode::CLAMP,
        },
        // LINEAR_WRAP,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
            .wrap_u = rhi::SamplerWrapMode::WRAP,
            .wrap_v = rhi::SamplerWrapMode::WRAP,
            .wrap_w = rhi::SamplerWrapMode::WRAP,
        },
        // LINEAR_MIRROR,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
            .wrap_u = rhi::SamplerWrapMode::MIRROR,
            .wrap_v = rhi::SamplerWrapMode::MIRROR,
            .wrap_w = rhi::SamplerWrapMode::MIRROR,
        },
        // LINEAR_CLAMP,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
            .wrap_u = rhi::SamplerWrapMode::CLAMP,
            .wrap_v = rhi::SamplerWrapMode::CLAMP,
            .wrap_w = rhi::SamplerWrapMode::CLAMP,
        },
        // ANISOTROPIC_WRAP,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::ANISOTROPIC,
            .wrap_u = rhi::SamplerWrapMode::WRAP,
            .wrap_v = rhi::SamplerWrapMode::WRAP,
            .wrap_w = rhi::SamplerWrapMode::WRAP,
        },
        // ANISOTROPIC_MIRROR,
        rhi::SamplerDesc{
            .filtering = rhi::SamplerFilterType::ANISOTROPIC,
            .wrap_u = rhi::SamplerWrapMode::MIRROR,
            .wrap_v = rhi::SamplerWrapMode::MIRROR,
            .wrap_w = rhi::SamplerWrapMode::MIRROR,
        },
        // ANISOTROPIC_CLAMP,
        rhi::SamplerDesc{
            .filtering =rhi::SamplerFilterType::ANISOTROPIC,
            .wrap_u = rhi::SamplerWrapMode::CLAMP,
            .wrap_v = rhi::SamplerWrapMode::CLAMP,
            .wrap_w = rhi::SamplerWrapMode::CLAMP,
        },
    };
}