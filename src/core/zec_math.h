#pragma once
#include "utils/memory.h"
#include <math.h>

/// <summary>
/// Vectors are _column-major_ and we use _post-multiplication_
/// so to transform a vector you would do M * v
///
/// Matrices are laid out in row-major form, so data[1][2] refers to the third element
/// in the second row (A_2,3)
///
/// To use the matrices produced by this lib with HLSL we'll have to enable row-major storage.
/// </summary>

namespace zec
{
    constexpr float k_pi = 3.14159265358979323846f;
    constexpr float k_half_pi = 1.57079632679489661923f;
    constexpr float k_quarter_pi = 0.785398163397448309616f;
    constexpr float k_inv_pi = 0.318309886183790671538f;
    constexpr float k_2_pi = k_pi * 2.0f;

    constexpr float deg_to_rad(const float degrees)
    {
        constexpr float conversion_factor = k_pi / 180.0f;
        return degrees * conversion_factor;
    }

    constexpr float rad_to_deg(const float radians)
    {
        constexpr float conversion_factor = 180.0f * k_inv_pi;
        return radians * conversion_factor;
    }

    template<typename T>
    inline T lerp(const T& x, const T& y, float s)
    {
        return x + (y - x) * s;
    }

    template<typename T>
    inline T min(T a, T b)
    {
        return a < b ? a : b;
    }

    template<typename T>
    inline T max(T a, T b)
    {
        return a < b ? b : a;
    }

    // ---------- vec2 ----------
    template<typename T>
    struct Vector2
    {
        union
        {
            T data[2] = {};
            struct { T x, y; };
            struct { T r, g; };
        };

        inline T& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const T& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    using uvec2 = Vector2<u32>;
    using vec2 = Vector2<float>;

    template<typename T>
    inline Vector2<T> operator+(const Vector2<T>& v1, const T s)
    {
        return { v1.x + s, v1.y + s };
    }
    template<typename T>
    inline Vector2<T> operator+(const Vector2<T>& v1, const Vector2<T>& v2)
    {
        return { v1.x + v2.x, v1.y + v2.y };
    }
    template<typename T>
    inline Vector2<T> operator-(const Vector2<T>& v1)
    {
        return { -v1.x, -v1.y };
    }
    template<typename T>
    inline Vector2<T> operator-(const Vector2<T>& v1, const Vector2<T>& v2)
    {
        return { v1.x - v2.x, v1.y - v2.y };
    }
    template<typename T>
    inline Vector2<T> operator-(const Vector2<T>& v, const T s)
    {
        return { v.x - s, v.y - s };
    }
    template<typename T>
    inline Vector2<T> operator-(const T s, const Vector2<T>& v)
    {
        return v - s;
    }
    template<typename T>
    inline Vector2<T> operator*(const Vector2<T>& v1, const T s)
    {
        return { v1.x * s, v1.y * s };
    }
    template<typename T>
    inline Vector2<T> operator*(const T s, const Vector2<T>& v1)
    {
        return v1 * s;
    }
    template<typename T>
    inline Vector2<T> operator*(const Vector2<T>& v1, const Vector2<T>& v2)
    {
        return { v1.x * v2.x, v1.y * v2.y };
    }
    template<typename T>
    inline Vector2<T> operator/(const Vector2<T>& v1, const T s)
    {
        float inv_s = 1.0f / s;
        return { v1.x * inv_s, v1.y * inv_s };
    }
    template<typename T>
    inline Vector2<T> operator/(const float s, const Vector2<T>& v)
    {
        return { s / v.x, s / v.y };
    }

    template<typename T>
    inline Vector2<T>& operator+=(Vector2<T>& v1, const Vector2<T>& v2)
    {
        v1.x += v2.x;
        v1.y += v2.y;
        return v1;
    }
    template<typename T>
    inline Vector2<T>& operator-=(Vector2<T>& v1, const Vector2<T>& v2)
    {
        v1.x -= v2.x;
        v1.y -= v2.y;
        return v1;
    }
    template<typename T>
    inline Vector2<T>& operator*=(Vector2<T>& v1, const T s)
    {
        v1.x *= s;
        v1.y *= s;
        return v1;
    }

    template<typename T>
    inline T length_squared(Vector2<T> v)
    {
        return v.x * v.x + v.y * v.y;
    }

    template<typename T>
    inline T length(Vector2<T> v)
    {
        return sqrtf(length_squared(v));
    }

    template<typename T>
    inline Vector2<T> normalize(Vector2<T> v)
    {
        return v / length(v);
    }

    template<typename T>
    T dot(const Vector2<T>& a, const Vector2<T>& b)
    {
        return a.x * b.x + a.y * b.y;
    };

    template<typename T>
    Vector2<T> min(const Vector2<T>& a, const Vector2<T>& b)
    {
        return Vector2<T>{ min(a.x, b.x), min(a.y, b.y) };
    }

    template<typename T>
    Vector2<T> max(const Vector2<T>& a, const Vector2<T>& b)
    {
        return Vector2<T>{ max(a.x, b.x), max(a.y, b.y) };
    }

    // ---------- vec3 ----------
    template<typename T>
    struct Vector3
    {
        union
        {
            T data[3] = {};
            struct { T x, y, z; };
            struct { T r, g, b; };
        };

        inline T& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const T& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    using uvec3 = Vector3<u32>;
    template <> struct Vector3<float>
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
    using vec3 = Vector3<float>;


    extern vec3 k_up;

    template<typename T>
    inline Vector3<T> operator+(const Vector3<T>& v1, const T s)
    {
        return { v1.x + s, v1.y + s, v1.z + s };
    }
    template<typename T>
    inline Vector3<T> operator+(const Vector3<T>& v1, const Vector3<T>& v2)
    {
        return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
    }
    template<typename T>
    inline Vector3<T> operator-(const Vector3<T>& v1)
    {
        return { -v1.x, -v1.y, -v1.z };
    }
    template<typename T>
    inline Vector3<T> operator-(const Vector3<T>& v1, const Vector3<T>& v2)
    {
        return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
    }
    template<typename T>
    inline Vector3<T> operator-(const Vector3<T>& v, const T s)
    {
        return { v.x - s, v.y - s, v.z - s };
    }
    template<typename T>
    inline Vector3<T> operator-(const T s, const Vector3<T>& v)
    {
        return v - s;
    }
    template<typename T>
    inline Vector3<T> operator*(const Vector3<T>& v1, const T s)
    {
        return { v1.x * s, v1.y * s, v1.z * s };
    }
    template<typename T>
    inline Vector3<T> operator*(const T s, const Vector3<T>& v1)
    {
        return { v1.x * s, v1.y * s, v1.z * s };
    }
    template<typename T>
    inline Vector3<T> operator*(const Vector3<T>& v1, const Vector3<T>& v2)
    {
        return { v1.x * v2.x, v1.y * v2.y, v1.z * v2.z };
    }
    template<typename T>
    inline Vector3<T> operator/(const Vector3<T>& v1, const T s)
    {
        float inv_s = 1.0f / s;
        return { v1.x * inv_s, v1.y * inv_s, v1.z * inv_s };
    }
    template<typename T>
    inline Vector3<T> operator/(const float s, const Vector3<T>& v)
    {
        return { s / v.x, s / v.y, s / v.z };
    }

    template<typename T>
    inline Vector3<T>& operator+=(Vector3<T>& v1, const Vector3<T>& v2)
    {
        v1.x += v2.x;
        v1.y += v2.y;
        v1.z += v2.z;
        return v1;
    }
    template<typename T>
    inline Vector3<T>& operator-=(Vector3<T>& v1, const Vector3<T>& v2)
    {
        v1.x -= v2.x;
        v1.y -= v2.y;
        v1.z -= v2.z;
        return v1;
    }
    template<typename T>
    inline Vector3<T>& operator*=(Vector3<T>& v1, const T s)
    {
        v1.x *= s;
        v1.y *= s;
        v1.z *= s;
        return v1;
    }

    template<typename T>
    inline T length_squared(Vector3<T> v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    template<typename T>
    inline T length(Vector3<T> v)
    {
        return sqrtf(length_squared(v));
    }

    template<typename T>
    inline Vector3<T> normalize(Vector3<T> v)
    {
        return v / length(v);
    }

    template<typename T>
    T dot(const Vector3<T>& a, const Vector3<T>& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    template<typename T>
    Vector3<T> cross(const Vector3<T>& a, const Vector3<T>& b)
    {
        T a1b2 = a[0] * b[1];
        T a1b3 = a[0] * b[2];
        T a2b1 = a[1] * b[0];
        T a2b3 = a[1] * b[2];
        T a3b1 = a[2] * b[0];
        T a3b2 = a[2] * b[1];

        return { a2b3 - a3b2, a3b1 - a1b3, a1b2 - a2b1 };
    };

    template<typename T>
    Vector3<T> min(const Vector3<T>& a, const Vector3<T>& b)
    {
        return Vector3<T>{ min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) };
    }

    template<typename T>
    Vector3<T> max(const Vector3<T>& a, const Vector3<T>& b)
    {
        return Vector3<T>{ max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) };
    }

    // ---------- vec4 ----------

    __declspec(align(16)) struct vec4
    {
        union
        {
            float data[4] = {};
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            struct { vec3 xyz; float s; };
            struct { vec3 rgb; float a; };
        };
        vec4() : data{ 0.0f, 0.0f, 0.0f, 0.0f } { };
        vec4(const float x, const float y, const float z, const float w) : x(x), y(y), z(z), w(w) { };
        vec4(const vec3 xyz, const float w = 1.0f) : xyz(xyz), s(w) { };

        inline float& operator[](const size_t idx)
        {
            return data[idx];
        }

        inline const float& operator[](const size_t idx) const
        {
            return data[idx];
        }
    };

    inline bool operator==(const vec4& v1, const vec4& v2)
    {
        return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w == v2.w;
    };

    inline vec4 operator+(const vec4& v1, const vec4& v2)
    {
        return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w };
    }
    inline vec4 operator-(const vec4& v1, const vec4& v2)
    {
        return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w };
    }
    inline vec4 operator*(const vec4& v1, const float s)
    {
        return { v1.x * s, v1.y * s, v1.z * s, v1.w * s };
    }
    inline vec4 operator*(const float s, const vec4& v1)
    {
        return v1 * s;
    }
    inline vec4 operator*(const vec4& v1, const vec4& v2)
    {
        return { v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w };
    }
    inline vec4 operator/(const vec4& v1, const float s)
    {
        float inv_s = 1.0f / s;
        return { v1.x * inv_s, v1.y * inv_s, v1.z * inv_s, v1.w * inv_s };
    }
    inline vec4 operator/(const float s, const vec4& v1)
    {
        return { s / v1.x, s / v1.y, s / v1.z, s / v1.w };
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

    //

    __declspec(align(16)) struct mat3
    {
        union
        {
            // We need each row to be 16 bytes, since that's how it's packed in HLSL
            vec4 rows[3] = {};
            float data[3][4];
        };

        mat3() : rows{ {}, {}, {} } { };
        mat3(const vec3& r1, const vec3& r2, const vec3& r3) : mat3()
        {
            memory::copy(&rows[0], &r1, sizeof(r1));
            memory::copy(&rows[1], &r2, sizeof(r1));
            memory::copy(&rows[2], &r3, sizeof(r1));
        }

        inline vec3 column(const size_t col_idx) const
        {
            return { rows[0][col_idx], rows[1][col_idx], rows[2][col_idx] };
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

    inline mat3 identity_mat3()
    {
        return mat3{
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
        };
    };

    mat3 transpose(const mat3& m);

    bool operator==(const mat3& m1, const mat3& m2);

    vec3 operator*(const mat3& m, const vec3& v);
    mat3 operator*(const mat3& m1, const mat3& m2);


    // ---------- mat34 ----------

    __declspec(align(16)) struct mat34
    {
        union
        {
            vec4 rows[3] = {};
            float data[3][4];
            float linear_data[12];
        };

        mat34() : rows{ {}, {}, {} } { };
        mat34(const vec4& r1, const vec4& r2, const vec4& r3)
        {
            memory::copy(&rows[0], &r1, sizeof(r1));
            memory::copy(&rows[1], &r2, sizeof(r1));
            memory::copy(&rows[2], &r3, sizeof(r1));
        }
        mat34(const vec4 in_rows[3])
        {
            memory::copy(linear_data, in_rows, sizeof(linear_data));
        }
        mat34(const float in_data[12])
        {
            memory::copy(linear_data, in_data, sizeof(linear_data));
        }
        mat34(const mat3& rotation, const vec3& translation)
        {
            memory::copy(&rows[0], &rotation.rows[0], sizeof(vec3));
            memory::copy(&rows[1], &rotation.rows[1], sizeof(vec3));
            memory::copy(&rows[2], &rotation.rows[2], sizeof(vec3));
            data[0][3] = translation.x;
            data[1][3] = translation.y;
            data[2][3] = translation.z;
        }

        inline vec3 column(const size_t col_idx) const
        {
            return { rows[0][col_idx], rows[1][col_idx], rows[2][col_idx] };
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

    bool operator==(const mat34& m1, const mat34& m2);
    mat34 operator*(const mat34& m1, const mat34& m2);
    mat34& operator/=(mat34& m, const float s);

    vec4 operator*(const mat34& m, const vec4& v);

    inline mat34 identity_mat34()
    {
        return mat34{
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f }
        };
    };

    vec3 get_right(const mat34& m);
    vec3 get_up(const mat34& m);
    vec3 get_dir(const mat34& m);
    vec3 get_translation(const mat34& m);

    mat34 look_at3x4(const vec3& pos, const vec3& target);

    mat34 invert(const mat34& m);

    mat34 transpose(const mat34& m);

    mat3 to_mat3(const mat34& m);


    // ---------- mat4 ----------

    struct mat4
    {
        union
        {
            vec4 rows[4] = {};
            float data[4][4];
            float linear_data[16];
        };

        mat4() : rows{ {}, {}, {}, {} } { };
        mat4(const vec4& r1, const vec4& r2, const vec4& r3, const vec4& r4)
        {
            memory::copy(&rows[0], &r1, sizeof(r1));
            memory::copy(&rows[1], &r2, sizeof(r1));
            memory::copy(&rows[2], &r3, sizeof(r1));
            memory::copy(&rows[3], &r4, sizeof(r1));
        }
        mat4(const vec4 in_rows[4])
        {
            memory::copy(data, in_rows, sizeof(data));
        }
        mat4(const float in_data[16])
        {
            memory::copy(linear_data, in_data, sizeof(linear_data));
        }
        mat4(const mat3& rotation, const vec3& translation)
        {
            memory::copy(&rows[0], &rotation.rows[0], sizeof(vec3));
            memory::copy(&rows[1], &rotation.rows[1], sizeof(vec3));
            memory::copy(&rows[2], &rotation.rows[2], sizeof(vec3));
            rows[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
            data[0][3] = translation.x;
            data[1][3] = translation.y;
            data[2][3] = translation.z;
        }

        inline vec4 column(const size_t col_idx) const
        {
            return { rows[0][col_idx], rows[1][col_idx], rows[2][col_idx], rows[3][col_idx] };
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

    bool operator==(const mat4& m1, const mat4& m2);
    mat4 operator*(const mat4& m1, const mat4& m2);
    mat4& operator/=(mat4& m, const float s);

    vec4 operator*(const mat4& m, const vec4& v);

    inline mat4 identity_mat4()
    {
        return mat4{
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        };
    };

    vec3 get_right(const mat4& m);
    vec3 get_up(const mat4& m);
    vec3 get_dir(const mat4& m);
    vec3 get_translation(const mat4& m);

    mat4 look_at4x4(const vec3& pos, const vec3& target);

    mat4 invert(const mat4& m);

    mat4 transpose(const mat4& m);

    mat3 to_mat3(const mat4& m);

    mat4 to_mat4(const mat34& m);
    mat34 to_mat34(const mat4& m);
    // ---------- quaternion ----------

    struct quaternion
    {
        union
        {
            float data[4] = {};
            struct { float x, y, z, w; };
            struct { vec3 v; float u; };
        };

        quaternion() = default;
        quaternion(const float x, const float y, const float z, const float w) :
            x(x), y(y), z(z), w(w)
        { };
        quaternion(vec3 v, float u) : v{ v }, u{ u } { };


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


    mat3 quat_to_mat3(const quaternion& q);
    mat34 quat_to_mat34(const quaternion& q);
    mat4 quat_to_mat4(const quaternion& q);

    // ---------- Transformation helpers ----------

    inline void rotate(mat4& mat, const quaternion& q)
    {
        mat = quat_to_mat4(q) * mat;
    }

    inline void set_translation(mat34& mat, const vec3& translation)
    {
        mat[0][3] = translation.x;
        mat[1][3] = translation.y;
        mat[2][3] = translation.z;
    };
    inline void set_translation(mat4& mat, const vec3& translation)
    {
        mat[0][3] = translation.x;
        mat[1][3] = translation.y;
        mat[2][3] = translation.z;
    };

    inline void set_scale(mat3& mat, const vec3& scale)
    {
        mat[0][0] = scale.x;
        mat[1][1] = scale.y;
        mat[2][2] = scale.z;
    }
    inline void set_scale(mat34& mat, const vec3& scale)
    {
        mat[0][0] = scale.x;
        mat[1][1] = scale.y;
        mat[2][2] = scale.z;
    }
    inline void set_scale(mat4& mat, const vec3& scale)
    {
        mat[0][0] = scale.x;
        mat[1][1] = scale.y;
        mat[2][2] = scale.z;
    };
    //mat44 orthogonal_projection(float left, float right, float near, float far, float top, float bottom)

    // Aspect ratio is in radians, please
    mat4 perspective_projection(const float aspect_ratio, const float fov, const float z_near, const float z_far);

    struct AABB
    {
        // Two different corners of the AABB bounding box
        vec3 min;
        vec3 max;
    };

    struct OBB
    {
        vec3 center = {};
        vec3 extents = {};
        // Orthonormal basis
        vec3 axes[3] = {};
    };

    struct Plane
    {
        union
        {
            vec4 normal_d = {};
            struct
            {
                vec3 normal;
                // distance from original along normal
                float d;
            };
        };
    };

    inline bool within(float min_val, float x, float max_val)
    {
        return min_val <= x && x <= max_val;
    };
}