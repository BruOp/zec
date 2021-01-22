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

    void generate_frustum_data(Array<vec3>& in_positions, const float near_plane, const float far_plane, const float vertical_fov, const float aspect_ratio);

    const u16* get_cube_indices();
    const float* get_unit_cube_positions();
}