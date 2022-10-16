#pragma once
#include <core/zec_types.h>
#include <utils/crc_hash.h>
#include <gfx/render_task_system.h>
#include "renderable_scene.h"

namespace clustered
{
#define MAKE_IDENTIFIER(_VAR_NAME) \
    constexpr zec::render_graph::ResourceIdentifier _VAR_NAME = { .identifier = ctcrc32(#_VAR_NAME) };

    namespace settings
    {
        MAKE_IDENTIFIER(MAIN_PASS_VIEW_CB);
        MAKE_IDENTIFIER(RENDERABLE_SCENE_PTR);
        MAKE_IDENTIFIER(CLUSTER_GRID_SETUP);
        MAKE_IDENTIFIER(BACKGROUND_CUBE_MAP);
        MAKE_IDENTIFIER(FULLSCREEN_QUAD);
        MAKE_IDENTIFIER(EXPOSURE);
    };

    namespace pass_resources
    {
        MAKE_IDENTIFIER(DEPTH_TARGET);
        MAKE_IDENTIFIER(HDR_TARGET);
        MAKE_IDENTIFIER(SDR_TARGET);
        MAKE_IDENTIFIER(SPOT_LIGHT_INDICES);
        MAKE_IDENTIFIER(POINT_LIGHT_INDICES);
    }

    namespace pass_resource_descs
    {
        constexpr zec::render_graph::TextureResourceDesc DEPTH_TARGET;
        constexpr zec::render_graph::TextureResourceDesc HDR_TARGET;
        constexpr zec::render_graph::TextureResourceDesc SDR_TARGET;
        constexpr zec::render_graph::BufferResourceDesc SPOT_LIGHT_INDICES;
        constexpr zec::render_graph::BufferResourceDesc POINT_LIGHT_INDICES;
    }
}
