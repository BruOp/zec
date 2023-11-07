#include "utils/utils.h"
#include "zec_math.h"

namespace zec
{
    vec3 k_up = { 0.0f, 1.0f, 0.0f };
    vec3 k_right = { 1.0f, 0.0f, 0.0f };
    vec3 k_forward = { 0.0f, 0.0f, -1.0f };

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
    }

    mat3 quat_to_mat3(const quaternion& q)
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
            { 1 - 2 * (qy2 + qz2),  2 * (qxy - qzw),        2 * (qxz + qyw) },
            { 2 * (qxy + qzw),      1 - 2 * (qx2 + qz2),     2 * (qyz - qxw) },
            { 2 * (qxz - qyw),      2 * (qyz + qxw),        1 - 2 * (qx2 + qy2) },
        };
    }
    mat34 quat_to_mat34(const quaternion& q)
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
            { 1 - 2 * (qy2 + qz2),  2 * (qxy - qzw),        2 * (qxz + qyw),        0 },
            { 2 * (qxy + qzw),      1 - 2 * (qx2 + qz2),     2 * (qyz - qxw),        0 },
            { 2 * (qxz - qyw),      2 * (qyz + qxw),        1 - 2 * (qx2 + qy2),    0 },
        };
    }
    mat4 quat_to_mat4(const quaternion& q)
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
            { 1 - 2 * (qy2 + qz2),  2 * (qxy - qzw),        2 * (qxz + qyw),        0 },
            { 2 * (qxy + qzw),      1 - 2 * (qx2 + qz2),     2 * (qyz - qxw),        0 },
            { 2 * (qxz - qyw),      2 * (qyz + qxw),        1 - 2 * (qx2 + qy2),    0 },
            { 0,                    0,                      0,                      1 },
        };
    }

    mat3 transpose(const mat3& m)
    {
        return {
            m.column(0),
            m.column(1),
            m.column(2)
        };
    }

    bool operator==(const mat3& m1, const mat3& m2)
    {
        for (size_t i = 0; i < 3; i++) {
            for (size_t j = 0; j < 3; j++) {
                if (m1[i][j] != m2[i][j]) {
                    return false;
                }
            }
        }
        return true;
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

    mat3 operator*(const mat3& m1, const mat3& m2)
    {
        mat3 res{};
        for (size_t i = 0; i < 3; i++) {
            for (size_t j = 0; j < 3; j++) {
                for (size_t k = 0; k < 3; k++) {
                    res[i][j] += m1[i][k] * m2[k][j];
                }
            }
        }
        return res;
    }

    bool operator==(const mat34& m1, const mat34& m2)
    {
        constexpr size_t N = _countof(m1.rows);

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < 4; j++) {
                if (m1[i][j] != m2[i][j]) return false;
            }
        }
        return true;
    }

    mat34 operator*(const mat34& m1, const mat34& m2)
    {
        constexpr size_t N = _countof(m1.rows);
        mat34 res{};
        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < 4; j++) {
                for (size_t k = 0; k < N; k++) {
                    res[i][j] += m1[i][k] * m2[k][j];
                }
                // Make up for our missing last row in the second matrix, which would only have a 1 in [i][3]
                res[i][j] += m1[i][3];
            }
        }
        return res;
    }

    mat34& operator/=(mat34& m, const float s)
    {
        for (size_t i = 0; i < _countof(m.rows); i++) {
            for (size_t j = 0; j < 4; j++) {
                m.data[i][j] *= s;
            }
        }
        return m;
    }

    vec4 operator*(const mat34& m, const vec4& v)
    {
        vec4 res{};
        for (size_t i = 0; i < _countof(m.rows); i++) {
            for (size_t j = 0; j < 4; j++) {
                res[i] += m[i][j] * v[j];
            }
        }
        return res;
    }

    vec3 get_right(const mat34& m)
    {
        return { m[0][0], m[0][1], m[0][2] };
    }

    vec3 get_up(const mat34& m)
    {
        return { m[1][0], m[1][1], m[1][2] };
    }

    vec3 get_dir(const mat34& m)
    {
        return { m[2][0], m[2][1], m[2][2] };
    }

    vec3 get_translation(const mat34& m)
    {
        return { m[0][3], m[1][3], m[2][3] };
    }

    bool operator==(const mat4& m1, const mat4& m2)
    {
        constexpr size_t N = _countof(m1.rows);

        for (size_t i = 0; i < N; i++) {
            for (size_t j = 0; j < N; j++) {
                if (m1[i][j] != m2[i][j]) {
                    return false;
                }
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

    mat34 look_at3x4(const vec3& pos, const vec3& target)
    {
        vec3 dir = normalize(pos - target);
        const vec3& world_up = dir == k_up ? k_right : k_up;
        vec3 right = normalize(cross(world_up, dir));
        vec3 up = cross(dir, right);

        return {
            vec4{ right, -dot(right, pos) },
            vec4{ up,    -dot(up, pos) },
            vec4{ dir,   -dot(dir, pos) }
        };
    }

    mat4 look_at4x4(const vec3& pos, const vec3& target)
    {
        vec3 dir = normalize(pos - target);
        const vec3& world_up = dir == k_up ? k_right : k_up;
        vec3 right = normalize(cross(world_up, dir));
        vec3 up = cross(dir, right);

        return {
            vec4{ right, -dot(right, pos) },
            vec4{ up,    -dot(up, pos) },
            vec4{ dir,   -dot(dir, pos) },
            vec4{ 0, 0, 0, 1 }
        };
    }

    mat3 to_mat3(const mat34& m)
    {
        return {
            vec3{ m[0][0], m[0][1], m[0][2] },
            vec3{ m[1][0], m[1][1], m[1][2] },
            vec3{ m[2][0], m[2][1], m[2][2] },
        };
    }

    mat4 to_mat4(const mat34& m)
    {
        return {
            vec4{ m[0][0], m[0][1], m[0][2], m[0][3] },
            vec4{ m[1][0], m[1][1], m[1][2], m[1][3] },
            vec4{ m[2][0], m[2][1], m[2][2], m[2][3] },
            vec4{ 0, 0, 0, 1},
        };
    }

    mat4 invert(const mat4& mat)
    {
        float inv[16];
        float det;
        const float* m = mat.linear_data;

        inv[0] = m[5] * m[10] * m[15] -
            m[5] * m[11] * m[14] -
            m[9] * m[6] * m[15] +
            m[9] * m[7] * m[14] +
            m[13] * m[6] * m[11] -
            m[13] * m[7] * m[10];

        inv[4] = -m[4] * m[10] * m[15] +
            m[4] * m[11] * m[14] +
            m[8] * m[6] * m[15] -
            m[8] * m[7] * m[14] -
            m[12] * m[6] * m[11] +
            m[12] * m[7] * m[10];

        inv[8] = m[4] * m[9] * m[15] -
            m[4] * m[11] * m[13] -
            m[8] * m[5] * m[15] +
            m[8] * m[7] * m[13] +
            m[12] * m[5] * m[11] -
            m[12] * m[7] * m[9];

        inv[12] = -m[4] * m[9] * m[14] +
            m[4] * m[10] * m[13] +
            m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] -
            m[12] * m[5] * m[10] +
            m[12] * m[6] * m[9];

        inv[1] = -m[1] * m[10] * m[15] +
            m[1] * m[11] * m[14] +
            m[9] * m[2] * m[15] -
            m[9] * m[3] * m[14] -
            m[13] * m[2] * m[11] +
            m[13] * m[3] * m[10];

        inv[5] = m[0] * m[10] * m[15] -
            m[0] * m[11] * m[14] -
            m[8] * m[2] * m[15] +
            m[8] * m[3] * m[14] +
            m[12] * m[2] * m[11] -
            m[12] * m[3] * m[10];

        inv[9] = -m[0] * m[9] * m[15] +
            m[0] * m[11] * m[13] +
            m[8] * m[1] * m[15] -
            m[8] * m[3] * m[13] -
            m[12] * m[1] * m[11] +
            m[12] * m[3] * m[9];

        inv[13] = m[0] * m[9] * m[14] -
            m[0] * m[10] * m[13] -
            m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] -
            m[12] * m[2] * m[9];

        inv[2] = m[1] * m[6] * m[15] -
            m[1] * m[7] * m[14] -
            m[5] * m[2] * m[15] +
            m[5] * m[3] * m[14] +
            m[13] * m[2] * m[7] -
            m[13] * m[3] * m[6];

        inv[6] = -m[0] * m[6] * m[15] +
            m[0] * m[7] * m[14] +
            m[4] * m[2] * m[15] -
            m[4] * m[3] * m[14] -
            m[12] * m[2] * m[7] +
            m[12] * m[3] * m[6];

        inv[10] = m[0] * m[5] * m[15] -
            m[0] * m[7] * m[13] -
            m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] -
            m[12] * m[3] * m[5];

        inv[14] = -m[0] * m[5] * m[14] +
            m[0] * m[6] * m[13] +
            m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] +
            m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] +
            m[1] * m[7] * m[10] +
            m[5] * m[2] * m[11] -
            m[5] * m[3] * m[10] -
            m[9] * m[2] * m[7] +
            m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] -
            m[0] * m[7] * m[10] -
            m[4] * m[2] * m[11] +
            m[4] * m[3] * m[10] +
            m[8] * m[2] * m[7] -
            m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] +
            m[0] * m[7] * m[9] +
            m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] +
            m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] -
            m[0] * m[6] * m[9] -
            m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] -
            m[8] * m[2] * m[5];

        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (det == 0)
            throw std::runtime_error("Cannot calculate inverse of this matrix");

        det = 1.0f / det;

        for (size_t i = 0; i < 16; i++)
            inv[i] = inv[i] * det;

        return mat4{ inv };
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