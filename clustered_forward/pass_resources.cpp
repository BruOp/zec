#include "pass_resources.h"

namespace clustered::PassResourceDescs
{
    constexpr zec::render_graph::TextureResourceDesc DEPTH_TARGET = {
        .identifier = pass_resources::DEPTH_TARGET,
        .name = "depth",
        .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
        .relative_width_factor = 1.0f,
        .relative_height_factor = 1.0f,
        .desc = {
            .num_mips = 1,
            .array_size = 1,
            .is_cubemap = false,
            .is_3d = false,
            .format = zec::BufferFormat::D32,
            .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
            .initial_state = zec::RESOURCE_USAGE_DEPTH_STENCIL,
        },
    };
    constexpr zec::render_graph::TextureResourceDesc HDR_TARGET = {
        .identifier = pass_resources::HDR_TARGET,
        .name = "hdr",
        .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
        .relative_width_factor = 1.0f,
        .relative_height_factor = 1.0f,
        .desc = {
            .num_mips = 1,
            .array_size = 1,
            .is_cubemap = false,
            .is_3d = false,
            .format = zec::BufferFormat::R16G16B16A16_FLOAT,
            .usage = zec::RESOURCE_USAGE_RENDER_TARGET | zec::RESOURCE_USAGE_SHADER_READABLE,
            .initial_state = zec::RESOURCE_USAGE_RENDER_TARGET,
        },
    };
    constexpr zec::render_graph::TextureResourceDesc SDR_TARGET = {
        .identifier = pass_resources::SDR_TARGET,
        .name = "sdr",
        .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
        .relative_width_factor = 1.0f,
        .relative_height_factor = 1.0f,
        .desc = {
            .num_mips = 1,
            .array_size = 1,
            .is_cubemap = false,
            .is_3d = false,
            .format = zec::BufferFormat::R8G8B8A8_UNORM_SRGB,
            .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
            .initial_state = zec::RESOURCE_USAGE_RENDER_TARGET,
        },
    };

    constexpr zec::render_graph::BufferResourceDesc SPOT_LIGHT_INDICES = {
        .identifier = pass_resources::SPOT_LIGHT_INDICES,
        .name = "spot light indices",
        .initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
        .desc = {
            .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_SHADER_READABLE,
            .type = zec::BufferType::RAW,
            .byte_size = 0,
            .stride = 4,
        }
    };
    constexpr zec::render_graph::BufferResourceDesc POINT_LIGHT_INDICES = {
        .identifier = pass_resources::POINT_LIGHT_INDICES,
        .name = "point light indices",
        .initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
        .desc = {
            .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_SHADER_READABLE,
            .type = zec::BufferType::RAW,
            .byte_size = 0,
            .stride = 4,
        }
    };
}