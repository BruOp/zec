#include "bounding_meshes.h"

namespace zec::geometry
{
    const float k_cube_positions[] = {
         -0.5f,  0.5f, -0.5f, // +Y (top face)
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         -0.5f,  0.5f,  0.5f,
         -0.5f, -0.5f,  0.5f,  // -Y (bottom face)
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f, -0.5f,
         -0.5f, -0.5f, -0.5f,
     };

    const float k_cube_uvs[] = {
         0.0f,  0.0f, // +Y (top face)
         1.0f,  0.0f,
         1.0f,  1.0f,
         0.0f,  1.0f,
         0.0f,  0.0f,  // -Y (bottom face)
         1.0f,  0.0f,
         1.0f,  1.0f,
         0.0f,  1.0f,
    };

     const u32 k_cube_colors[] = {
         0xff00ff00, // +Y (top face)
         0xff00ffff,
         0xffffffff,
         0xffffff00,
         0xffff0000, // -Y (bottom face)
         0xffff00ff,
         0xff0000ff,
         0xff000000,
     };

     const u16 k_cube_indices[] = {
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

     const u16 k_fullscreen_indices[] = { 0, 1, 2 };

     const float k_fullscreen_positions[9u] = {
         1.0f,  3.0f, 1.0f,
         1.0f, -1.0f, 1.0f,
         -3.0f, -1.0f, 1.0f,
     };

     const float k_fullscreen_uvs[] = {
         1.0f,  -1.0f,
         1.0f,   1.0f,
         -1.0f,   1.0f,
     };

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
