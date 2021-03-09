#include "bounding_meshes.h"

namespace zec::geometry
{
    void generate_frustum_data(Array<vec3>& in_positions, const float near_plane, const float far_plane, const float vertical_fov, const float aspect_ratio)
    {
        in_positions.reserve(8u);

        const float tan_fov = tanf(0.5f * vertical_fov);

        // These represent two different corners of our full frustum
        vec3 cl_left_bot_near = vec3{
            -aspect_ratio * tan_fov * near_plane,
            -tan_fov * near_plane,
            -near_plane,
        };
        vec3 cl_right_top_far = {
            aspect_ratio * tan_fov * far_plane,
            tan_fov * far_plane,
            -far_plane,
        };

        //-0.5f,  0.5f, -0.5f, // +Y (top face)
        // 0.5f,  0.5f, -0.5f,
        // 0.5f,  0.5f,  0.5f,
        //-0.5f,  0.5f,  0.5f,
        //-0.5f, -0.5f,  0.5f,  // -Y (bottom face)
        // 0.5f, -0.5f,  0.5f,
        // 0.5f, -0.5f, -0.5f,
        //-0.5f, -0.5f, -0.5f,


        // Top and bottom planes
        in_positions.push_back({ -cl_right_top_far.x, cl_right_top_far.y, cl_right_top_far.z });
        in_positions.push_back({ cl_right_top_far.x, cl_right_top_far.y, cl_right_top_far.z });
        in_positions.push_back({ -cl_left_bot_near.x, -cl_left_bot_near.y, cl_left_bot_near.z });
        in_positions.push_back({ cl_left_bot_near.x, -cl_left_bot_near.y, cl_left_bot_near.z });

        in_positions.push_back({ cl_left_bot_near.x, cl_left_bot_near.y, cl_left_bot_near.z });
        in_positions.push_back({ -cl_left_bot_near.x, cl_left_bot_near.y, cl_left_bot_near.z });
        in_positions.push_back({ cl_right_top_far.x, -cl_right_top_far.y, cl_right_top_far.z });
        in_positions.push_back({ -cl_right_top_far.x, -cl_right_top_far.y, cl_right_top_far.z });
    }
    const u16* get_cube_indices()
    {
        return k_cube_indices;
    }

    const float* get_unit_cube_positions()
    {
        return k_cube_positions;
    }
}
