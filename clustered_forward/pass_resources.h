#pragma once
#include "utils/crc_hash.h"
#include "gfx/render_task_system.h"
#include "renderable_scene.h"

namespace clustered
{
    template <typename T, size_t N>
    constexpr zec::SettingsDesc define_setting(const char (&name)[N])
    {
        return zec::SettingsDesc{
            .identifier = ctcrc32(name),
            .name = name,
            .byte_size = sizeof(T)
        };
    }

    namespace Settings
    {
        constexpr zec::SettingsDesc main_pass_view_cb = define_setting<zec::BufferHandle>("Main pass view cb");

        constexpr zec::SettingsDesc renderable_scene_ptr = define_setting<RenderableScene*>("Renderable Scene pointer");

        constexpr zec::SettingsDesc cluster_grid_setup = define_setting <ClusterGridSetup>("Cluster Grid Setup");

        constexpr zec::SettingsDesc background_cube_map = define_setting <zec::TextureHandle>("Background Cube Map");

        constexpr zec::SettingsDesc fullscreen_quad = define_setting <zec::MeshHandle>("Fullscreen Quad Mesh");

        constexpr zec::SettingsDesc exposure = define_setting<float>("Tone Mapping Exposure");
    }

    namespace PassResources
    {
        constexpr zec::TexturePassResourceDesc DEPTH_TARGET = {
            .identifier = ctcrc32("depth"),
            .name = "depth",
            .sizing = zec::Sizing::RELATIVE_TO_SWAP_CHAIN,
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
        constexpr zec::TexturePassResourceDesc HDR_TARGET = {
            .identifier = ctcrc32("hdr"),
            .name = "hdr",
            .sizing = zec::Sizing::RELATIVE_TO_SWAP_CHAIN,
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
        constexpr zec::TexturePassResourceDesc SDR_TARGET = {
            .identifier = ctcrc32("sdr"),
            .name = "sdr",
            .sizing = zec::Sizing::RELATIVE_TO_SWAP_CHAIN,
            .relative_width_factor = 1.0f,
            .relative_height_factor = 1.0f,
            .desc = {
                .num_mips = 1,
                .array_size=1,
                .is_cubemap=false,
                .is_3d=false,
                .format = zec::BufferFormat::R8G8B8A8_UNORM_SRGB,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .initial_state = zec::RESOURCE_USAGE_RENDER_TARGET,
            },
        };

        constexpr zec::BufferPassResourceDesc SPOT_LIGHT_INDICES = {
            .identifier = ctcrc32("spot light indices"),
            .name = "spot light indices",
            .initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
            .desc = {
                .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_SHADER_READABLE,
                .type = zec::BufferType::RAW,
                .byte_size = 0,
                .stride = 4,
            }
        };
        constexpr zec::BufferPassResourceDesc POINT_LIGHT_INDICES = {
            .identifier = ctcrc32("point light indices"),
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
}
