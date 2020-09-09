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

    inline float deg_to_rad(const float degrees)
    {
        constexpr float conversion_factor = k_pi / 180.0f;
        return degrees * conversion_factor;
    }

    inline float rad_to_deg(const float radians)
    {
        constexpr float conversion_factor = 180.0f * k_inv_pi;
        return radians * conversion_factor;
    }

    // ---------- vec3 ----------

    struct vec3
    {
        union
        {
            float data[3] = {};
            struct { float x, y, z; };
            struct { float r, g, b; };
        };

        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };


    inline vec3 operator+(const vec3& v1, const vec3& v2)
    {
        return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
    }
    inline vec3 operator*(const vec3& v1, const float s)
    {
        return { v1.x * s, v1.y * s, v1.z * s };
    }
    inline vec3 operator*(const float s, const vec3& v1)
    {
        return { v1.x * s, v1.y * s, v1.z * s };
    }
    inline vec3 operator/(const vec3& v1, const float s)
    {
        float inv_s = 1.0f / s;
        return { v1.x * inv_s, v1.y * inv_s, v1.z * inv_s };
    }

    inline float length_squared(vec3 v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }
    inline float length(vec3 v)
    {
        return sqrtf(length_squared(v));
    }
    inline vec3 normalize(vec3 v)
    {
        return v / length(v);
    }

    // ---------- vec4 ----------

    struct vec4
    {
        union
        {
            float data[4] = {};
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            vec3 xyz;
            vec3 rgb;
        };

        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    inline vec4 operator+(const vec4& v1, const vec4& v2)
    {
        return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w };
    }
    inline vec4 operator*(const vec4& v1, const float s)
    {
        return { v1.x * s, v1.y * s, v1.z * s, v1.w * s };
    }
    inline vec4 operator/(const vec4& v1, const float s)
    {
        float inv_s = 1.0f / s;
        return { v1.x * inv_s, v1.y * inv_s, v1.z * inv_s, v1.w * inv_s };
    }


    inline float length_squared(vec4 v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    }
    inline float length(vec4 v)
    {
        return sqrtf(length_squared(v));
    }
    inline vec4 normalize(vec4 v)
    {
        return v / length(v);
    }

    // ---------- mat44 ----------

    struct mat44
    {
        union
        {
            vec4 rows[4] = {};
            float data[4][4];
        };

        mat44() : rows{ {}, {}, {}, {} } { };
        mat44(const vec4& r1, const vec4& r2, const vec4& r3, const vec4& r4)
        {
            memory::copy(&rows[0], &r1, sizeof(r1));
            memory::copy(&rows[1], &r2, sizeof(r1));
            memory::copy(&rows[2], &r3, sizeof(r1));
            memory::copy(&rows[3], &r4, sizeof(r1));
        }
        mat44(const vec4 in_rows[4])
        {
            memory::copy(data, in_rows, sizeof(data));
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

    mat44 operator*(const mat44& m1, const mat44& m2);

    inline mat44 identity_mat44()
    {
        return mat44{
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
    };

    // ---------- quaternion ----------

    struct quaternion
    {
        union
        {
            float data[4] = {};
            struct { float x, y, z, w; };
            struct { vec3 v; float w; };
        };

        quaternion() = default;
        quaternion(const float x, const float y, const float z, const float w) :
            x(x), y(y), z(z), w(w)
        { };
        quaternion(vec3 v, float w) : v(v), w(w) { };


        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    inline quaternion operator*(const quaternion& q1, const float s)
    {
        return { q1.x * s, q1.y * s, q1.z * s, q1.w * s };
    }
    inline quaternion operator/(const quaternion& q1, const float s)
    {
        float inv_s = 1.0f / s;
        return { q1.x * inv_s, q1.y * inv_s, q1.z * inv_s, q1.w * inv_s };
    }

    inline quaternion from_axis_angle(const vec3& axis, const float angle)
    {
        return { sinf(0.5f * angle) * normalize(axis), cosf(0.5f * angle) };
    }

    inline float length_squared(quaternion v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    }
    inline float length(quaternion v)
    {
        return sqrtf(length_squared(v));
    }
    inline quaternion normalize(quaternion v)
    {
        return v / length(v);
    }

    mat44 quat_to_mat(const quaternion& q);

    // ---------- Transformation helpers ----------

    inline void rotate(mat44& mat, const quaternion& q)
    {
        mat = quat_to_mat(q) * mat;
    }

    inline void set_translation(mat44& mat, const vec3& translation)
    {
        mat[0][3] = translation.x;
        mat[1][3] = translation.y;
        mat[2][3] = translation.z;
    };
    //mat44 orthogonal_projection(float left, float right, float near, float far, float top, float bottom)

    // Aspect ratio is in radians, please
    mat44 perspective_projection(const float aspect_ratio, const float fov, const float z_near, const float z_far);
}