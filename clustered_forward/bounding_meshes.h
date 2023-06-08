#pragma once
#include "core/array.h"
#include "core/zec_math.h"

namespace zec::geometry
{
    extern const float k_cube_positions[24u];

    extern const float k_cube_uvs[16u];

    extern const u32 k_cube_colors[8u];

    extern const u16 k_cube_indices[36u];

    extern const u16 k_fullscreen_indices[3u];

    extern const float k_fullscreen_positions[9u];

    extern const float k_fullscreen_uvs[6u];

    void generate_frustum_data(Array<vec3>& in_positions, const float near_plane, const float far_plane, const float vertical_fov, const float aspect_ratio);
}