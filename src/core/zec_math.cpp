#include "pch.h"
#include "zec_math.h"

namespace zec
{
    mat44 perspective_projection(const float aspect_ratio, const float fov, const float z_near, const float z_far)
    {
        const float h = 1.0f / tanf(0.5f * fov);
        const float w = h / aspect_ratio;

        return mat44{
            { w, 0.0f, 0.0f, 0.0f },
            { 0.0f, h, 0.0f, 0.0f },
            { 0.0f, 0.0f, -(z_far) / (z_near - z_far) - 1, -(z_near * z_far) / (z_near - z_far) },
            { 0.0f, 0.0f, -1.0f, 0.0f },
        };
    };

    mat44 quat_to_mat(const quaternion& q)
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
    mat44 operator*(const mat44& m1, const mat44& m2)
    {
        const vec4& src_a0 = m1.rows[0];
        const vec4& src_a1 = m1.rows[1];
        const vec4& src_a2 = m1.rows[2];
        const vec4& src_a3 = m1.rows[3];

        const vec4& src_b0 = m2.rows[0];
        const vec4& src_b1 = m2.rows[1];
        const vec4& src_b2 = m2.rows[2];
        const vec4& src_b3 = m2.rows[3];

        return mat44{
            src_a0 * src_b0[0] + src_a1 * src_b0[1] + src_a2 * src_b0[2] + src_a3 * src_b0[3],
            src_a0 * src_b1[0] + src_a1 * src_b1[1] + src_a2 * src_b1[2] + src_a3 * src_b1[3],
            src_a0 * src_b2[0] + src_a1 * src_b2[1] + src_a2 * src_b2[2] + src_a3 * src_b2[3],
            src_a0 * src_b3[0] + src_a1 * src_b3[1] + src_a2 * src_b3[2] + src_a3 * src_b3[3]
        };
    }
}