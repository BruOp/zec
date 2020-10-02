#include "pch.h"
#include "zec_math.h"

namespace zec
{
    vec3 k_up = { 0.0f, 1.0f, 0.0f };

    mat4 perspective_projection(const float aspect_ratio, const float fov, const float z_near, const float z_far)
    {
        const float h = 1.0f / tanf(0.5f * fov);
        const float w = h / aspect_ratio;

        return mat4{
            { w, 0.0f, 0.0f, 0.0f },
            { 0.0f, h, 0.0f, 0.0f },
            { 0.0f, 0.0f, -(z_far) / (z_far - z_near), -(z_near * z_far) / (z_far - z_near) },
            { 0.0f, 0.0f, -1.0f, 0.0f },
        };
    };

    mat4 quat_to_mat(const quaternion& q)
    {
        quaternion nq = normalize(q);
        float qx2 = nq.x * nq.x;
        float qy2 = nq.y * nq.y;
        float qz2 = nq.z * nq.z;
        float qxy = nq.x * nq.y;
        float qxz = nq.x * nq.z;
        float qxw = nq.x * nq.w;
        float qyz = nq.y * nq.z;
        float qyw = nq.y * nq.w;
        float qzw = nq.z * nq.w;

        return {
            { 1 - 2 * (qy2 + qz2), 2 * (qxy - qzw), 2 * (qxz + qyw), 0 },
            { 2 * (qxy + qzw), 1 - 2 * (qx2, qz2), 2 * (qyz - qxw), 0 },
            { 2 * (qxz - qyw), 2 * (qyz + qxw), 1 - 2 * (qx2 + qy2), 0 },
            { 0,  0, 0, 1 },
        };
    }

    float dot(const vec3& a, const vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    vec3 cross(const vec3& a, const vec3& b)
    {
        float a1b2 = a[0] * b[1];
        float a1b3 = a[0] * b[2];
        float a2b1 = a[1] * b[0];
        float a2b3 = a[1] * b[2];
        float a3b1 = a[2] * b[0];
        float a3b2 = a[2] * b[1];

        return { a2b3 - a3b2, a3b1 - a1b3, a1b2 - a2b1 };
    }

    mat3 transpose(const mat3& m)
    {
        return {
            m.column(0),
            m.column(1),
            m.column(2)
        };
    }

    vec3 operator*(const mat3& m, const vec3& v)
    {
        vec3 res{};
        for (size_t i = 0; i < 3; i++) {
            for (size_t j = 0; j < 3; j++) {
                res[i] = m[i][j] * v[j];
            }
        }
        return res;
    }

    bool operator==(const mat4& m1, const mat4& m2)
    {
        constexpr size_t N = _countof(m1.rows);

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                if (m1[i][j] != m2[i][j]) return false;
            }
        }
        return true;
    }

    mat4 operator*(const mat4& m1, const mat4& m2)
    {
        constexpr size_t N = _countof(m1.rows);

        mat4 res{};
        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                for (size_t k = 0; k < N; k++) {
                    res[i][j] += m1[i][k] * m2[k][j];
                }
            }
        }
        return res;
    }

    mat4& operator/=(mat4& m, const float s)
    {
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                m.data[i][j] *= s;
            }
        }
        return m;
    }

    vec4 operator*(const mat4& m, const vec4& v)
    {
        vec4 res{};
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                res[i] += m[i][j] * v[j];
            }
        }
        return res;
    }

    vec3 get_right(const mat4& m)
    {
        return { m[0][0], m[0][1], m[0][2] };
    }

    vec3 get_up(const mat4& m)
    {
        return { m[1][0], m[1][1], m[1][2] };
    }

    vec3 get_dir(const mat4& m)
    {
        return { m[2][0], m[2][1], m[2][2] };
    }

    vec3 get_translation(const mat4& m)
    {
        return { m[0][3], m[1][3], m[2][3] };
    }

    mat4 look_at(const vec3& pos, const vec3& target, const vec3& world_up)
    {
        vec3 dir = normalize(pos - target);
        vec3 right = normalize(cross(world_up, dir));
        vec3 up = cross(dir, right);

        return {
            vec4{ right, -dot(right, pos) },
            vec4{ up,    -dot(up, pos) },
            vec4{ dir,   -dot(dir, pos) },
            vec4{ 0.0f, 0.0f, 0.0f, 1.0f },
        };
    }

    mat4 invert(const mat4& m)
    {
        // Code based on inversion code in GLM
        float s00 = m[2][2] * m[3][3] - m[2][3] * m[3][2];
        float s01 = m[1][2] * m[3][3] - m[1][3] * m[3][2];
        float s02 = m[1][2] * m[2][3] - m[1][3] * m[2][2];
        float s03 = m[0][2] * m[3][3] - m[0][3] * m[3][2];
        float s04 = m[0][2] * m[2][3] - m[0][3] * m[2][2];
        float s05 = m[0][2] * m[1][3] - m[0][3] * m[1][2];
        float s06 = m[2][1] * m[3][3] - m[2][3] * m[3][1];
        float s07 = m[1][1] * m[3][3] - m[1][3] * m[3][1];
        float s08 = m[1][1] * m[2][3] - m[1][3] * m[2][1];
        float s09 = m[0][1] * m[3][3] - m[0][3] * m[3][1];
        float s10 = m[0][1] * m[2][3] - m[0][3] * m[2][1];
        float s11 = m[0][1] * m[1][3] - m[0][3] * m[1][1];
        float s12 = m[2][1] * m[3][2] - m[2][2] * m[3][1];
        float s13 = m[1][1] * m[3][2] - m[1][2] * m[3][1];
        float s14 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
        float s15 = m[0][1] * m[3][2] - m[0][2] * m[3][1];
        float s16 = m[0][1] * m[2][2] - m[0][2] * m[2][1];
        float s17 = m[0][1] * m[1][2] - m[0][2] * m[1][1];

        mat4 inv{};
        inv[0][0] = +(m[1][1] * s00 - m[2][1] * s01 + m[3][1] * s02);
        inv[0][1] = -(m[0][1] * s00 - m[2][1] * s03 + m[3][1] * s04);
        inv[0][2] = +(m[0][1] * s01 - m[1][1] * s03 + m[3][1] * s05);
        inv[0][3] = -(m[0][1] * s02 - m[1][1] * s04 + m[2][1] * s05);

        inv[1][0] = +(m[1][0] * s00 - m[2][0] * s01 + m[3][0] * s02);
        inv[1][1] = -(m[0][0] * s00 - m[2][0] * s03 + m[3][0] * s04);
        inv[1][2] = +(m[0][0] * s01 - m[1][0] * s03 + m[3][0] * s05);
        inv[1][3] = -(m[0][0] * s02 - m[1][0] * s04 + m[2][0] * s05);

        inv[2][0] = +(m[1][0] * s06 - m[2][0] * s07 + m[3][0] * s08);
        inv[2][1] = -(m[0][0] * s06 - m[2][0] * s09 + m[3][0] * s10);
        inv[2][2] = +(m[0][0] * s07 - m[1][0] * s09 + m[3][0] * s11);
        inv[2][3] = -(m[0][0] * s08 - m[1][0] * s10 + m[2][0] * s11);

        inv[3][0] = +(m[1][0] * s12 - m[2][0] * s13 + m[3][0] * s14);
        inv[3][1] = -(m[0][0] * s12 - m[2][0] * s15 + m[3][0] * s16);
        inv[3][2] = +(m[0][0] * s13 - m[1][0] * s15 + m[3][0] * s17);
        inv[3][3] = -(m[0][0] * s14 - m[1][0] * s16 + m[2][0] * s17);

        float det = +m[0][0] * inv[0][0]
            + m[1][0] * inv[1][0]
            + m[2][0] * inv[2][0]
            + m[3][0] * inv[3][0];

        inv /= det;
        return inv;
    }

    mat4 transpose(const mat4& m)
    {
        return {
            m.column(0),
            m.column(1),
            m.column(2),
            m.column(3)
        };
    }
    mat3 to_mat3(const mat4& m)
    {
        return {
            vec3{ m[0][0], m[0][1], m[0][2] },
            vec3{ m[1][0], m[1][1], m[1][2] },
            vec3{ m[2][0], m[2][1], m[2][2] },
        };
    }
}