#pragma once
#include "pch.h"
#include "utils/memory.h"

/// <summary>
/// Vectors are _column-major_ and we use _post-multiplication_
/// so to transform a vector you would do M * v
/// 
/// Matrices are layed out in row-major form, so data[1][2] refers to the third element
/// in the second row (A_2,3)
/// 
/// To use the matrices produced by this lib with HLSL we'll have to enable row-major storage.
/// </summary>

namespace zec
{
    constexpr float k_pi = M_PI;
    constexpr float k_half_pi = M_PI_2;
    constexpr float k_quarter_pi = M_PI_4;
    constexpr float k_inv_pi = M_1_PI;

    float deg_to_rad(const float degrees)
    {
        constexpr float conversion_factor = k_pi / 180.0f;
        return degrees * conversion_factor;
    }

    float rad_to_deg(const float radians)
    {
        constexpr float conversion_factor = 180.0f * k_inv_pi;
        return radians * conversion_factor;
    }

    struct vec3
    {
        float data[3] = {};

        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    struct vec4
    {
        float data[4] = {};

        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    struct mat44
    {
        float data[4][4] = {};

        mat44() = default;
        mat44(vec4 row1, vec4 row2, vec4 row3, vec4 row4)
        {
            memory::copy(data[0], row1.data, sizeof(row1));
            memory::copy(data[1], row2.data, sizeof(row2));
            memory::copy(data[2], row3.data, sizeof(row3));
            memory::copy(data[3], row4.data, sizeof(row4));
        }

        inline float* operator[](const size_t index)
        {
            return data[index];
        }

        inline const float* operator[](const size_t index) const
        {
            return data[index];
        }
    };

    inline mat44 identity_mat44()
    {
        return mat44{
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
    };

    inline void set_translation(mat44& mat, const vec3& translation)
    {
        mat[0][3] = translation[0];
        mat[1][3] = translation[1];
        mat[2][3] = translation[2];
    };

    //mat44 orthogonal_projection(float left, float right, float near, float far, float top, float bottom)

    // Aspect ratio is in radians, please
    mat44 perspective_projection(float aspect_ratio, float fov, float z_near, float z_far)
    {
        const float h = 1.0f / tanf(0.5f * fov);
        const float w = h / aspect_ratio;

        return mat44{
            {w, 0.0f, 0.0f, 0.0f},
            {0.0f, h, 0.0f, 0.0f},
            {0.0f, 0.0f, -(z_far) / (z_near - z_far) - 1, -(z_near * z_far) / (z_near - z_far)},
            {0.0f, 0.0f, -1.0f, 0.0f }
        };
    };
}