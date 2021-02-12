#include "core/array.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"

namespace zec::geometry
{
    extern constexpr float k_cube_positions[] = {
        -0.5f,  0.5f, -0.5f, // +Y (top face)
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,  // -Y (bottom face)
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
    };

    constexpr float k_cube_uvs[] = {
         0.0f,  0.0f, // +Y (top face)
         1.0f,  0.0f,
         1.0f,  1.0f,
         0.0f,  1.0f,
         0.0f,  0.0f,  // -Y (bottom face)
         1.0f,  0.0f,
         1.0f,  1.0f,
         0.0f,  1.0f,
    };

    constexpr u32 k_cube_colors[] = {
        0xff00ff00, // +Y (top face)
        0xff00ffff,
        0xffffffff,
        0xffffff00,
        0xffff0000, // -Y (bottom face)
        0xffff00ff,
        0xff0000ff,
        0xff000000,
    };

    extern constexpr u16 k_cube_indices[] = {
        2, 1, 0,
        3, 2, 0,
        5, 1, 2,
        5, 6, 1,
        4, 3, 0,
        7, 4, 0,
        1, 7, 0,
        6, 7, 1,
        4, 2, 3,
        4, 5, 2,
        7, 5, 4,
        7, 6, 5
    };

    extern constexpr u16 k_fullscreen_indices[] = { 0, 1, 2 };

    extern constexpr float k_fullscreen_positions[] = {
         1.0f,  3.0f, 1.0f,
         1.0f, -1.0f, 1.0f,
        -3.0f, -1.0f, 1.0f,
    };

    extern constexpr float k_fullscreen_uvs[] = {
         1.0f,  -1.0f,
         1.0f,   1.0f,
        -1.0f,   1.0f,
    };

    void generate_frustum_data(Array<vec3>& in_positions, const float near_plane, const float far_plane, const float vertical_fov, const float aspect_ratio);
}