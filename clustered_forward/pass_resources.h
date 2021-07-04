#pragma once
#include "utils/crc_hash.h"
#include "gfx/render_task_system.h"
#include "renderable_scene.h"

namespace clustered
{
    struct ResourceIdentifier
    {
        u64 id;
        const char* name;
    };

    namespace Settings
    {
        constexpr zec::SettingsDesc main_pass_view_cb = {
            .identifier = ctcrc32("Main pass view cb"),
            .name = "Main pass view cb",
            .byte_size = sizeof(zec::BufferHandle),
        };

        constexpr zec::SettingsDesc renderable_scene_ptr = {
            .identifier = ctcrc32("Renderable Scene pointer"),
            .name = "Renderable Scene pointer",
            .byte_size = sizeof(RenderableScene*),
        };

        constexpr zec::SettingsDesc cluster_grid_setup = {
            .identifier = ctcrc32("Cluster Grid Setup"),
            .name = "Cluster Grid Setup",
            .byte_size = sizeof(ClusterGridSetup),
        };
        
        constexpr zec::SettingsDesc background_cube_map = {
            .identifier = ctcrc32("Background Cube Map"),
            .name = "Background Cube Map",
            .byte_size = sizeof(zec::TextureHandle),
        };
        
        constexpr zec::SettingsDesc fullscreen_quad = {
            .identifier = ctcrc32("Fullscreen Quad Mesh"),
            .name = "Fullscreen Quad Mesh",
            .byte_size = sizeof(zec::MeshHandle),
        };

        constexpr zec::SettingsDesc exposure = {
            .identifier = ctcrc32("Tone Mapping Exposure"),
            .name = "ToneMapping Exposure",
            .byte_size = sizeof(float),
        };
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
