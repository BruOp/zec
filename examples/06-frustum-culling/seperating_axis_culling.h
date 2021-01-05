#include "pch.h"
#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"
#include "culling_methods.h"

namespace zec
{
    enum CullingCounter : u8
    {
        FRUSTUM_NORMALS = 0,
        OBB_AXES,
        R_CROSS_A,
        U_CROSS_A,
        FRUSTUM_EDGE_CROSS_A,
        NUM_CULLING_COUNTERS
    };

    static constexpr wchar const* culling_counter_names[] = {
        L"FRUSTUM_NORMALS",
        L"OBB_AXES",
        L"R_CROSS_A",
        L"U_CROSS_A",
        L"FRUSTUM_EDGE_CROSS_A",
    };
    static u32 culling_counters[NUM_CULLING_COUNTERS] = { 0 };

    struct OBBv2
    {
        vec3 center = {};
        vec3 extents = {};
        // Orthonormal basis
        union
        {
            vec3 axes[3] = {};
            struct
            {
                vec3 a0;
                vec3 a1;
                vec3 a2;
            };
        };
    };

    struct CullingFrustum
    {
        // Excludes forward direction, since that's just (0, 0, -1)
        vec3 left_bottom_near = {};
        vec3 right_top_far = {};
    };

    bool test_using_separating_axis_theorem(const CullingFrustum& frustum, const mat4& vs_transform, const AABB& aabb)
    {
        // Near, far
        float n = frustum.left_bottom_near.z;
        float f = frustum.right_top_far.z;
        // half width, half height
        float r = -frustum.left_bottom_near.x;
        float t = -frustum.left_bottom_near.y;

        // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
        vec3 corners[] = {
                    {aabb.min.x, aabb.min.y, aabb.min.z},
                    {aabb.max.x, aabb.min.y, aabb.min.z},
                    {aabb.min.x, aabb.max.y, aabb.min.z},
                    {aabb.min.x, aabb.min.y, aabb.max.z},
        };

        // Transform corners
        // This only translates to our OBB if our transform is affine
        for (size_t corner_idx = 0; corner_idx < ARRAY_SIZE(corners); corner_idx++) {
            corners[corner_idx] = (vs_transform * corners[corner_idx]).xyz;
        }

        OBBv2 obb = {
            .axes = {
                corners[1] - corners[0],
                corners[2] - corners[0],
                corners[3] - corners[0]
            },
        };
        obb.center = corners[0] + 0.5f * (obb.axes[0] + obb.axes[1] + obb.axes[2]);
        obb.extents = vec3{ length(obb.a0), length(obb.a1), length(obb.a2) };
        obb.axes[0] = obb.axes[0] / obb.extents.x;
        obb.axes[1] = obb.axes[1] / obb.extents.y;
        obb.axes[2] = obb.axes[2] / obb.extents.z;
        obb.extents *= 0.5f;

        // M = (0, 0, 1)
        {
            float MoR = 0.0f;
            float MoU = 0.0f;
            float MoD = 1.0f;
            float MoC = obb.center.z;
            // Projected size of OBB
            float radius = 0.0f;
            for (size_t i = 0; i < 3; i++) {
                // dot(M, axes[i]) == axes[i].z;
                radius += fabsf(obb.axes[i].z) * obb.extents[i];
            }
            float obb_min = MoC - radius;
            float obb_max = MoC + radius;

            float m0 = f; // Since z is negative, far is smaller than near
            float m1 = n;

            if (obb_min > m1 || obb_max < m0) {
                culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
                return false;
            }
        }

        {
            const vec3 M[] = {
                { n, 0.0f, r }, // Left Plane
                { -n, 0.0f, r }, // Right plane
                { 0.0, -n, t }, // Top plane
                { 0.0, n, t }, // Bottom plane
            };
            for (size_t m = 0; m < ARRAY_SIZE(M); m++) {
                float MoR = fabsf(M[m].x);
                float MoU = fabsf(M[m].y);
                float MoD = M[m].z;
                float MoC = dot(M[m], obb.center);

                float obb_radius = 0.0f;
                for (size_t i = 0; i < 3; i++) {
                    obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
                }
                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                float p = r * MoR + t * MoU;

                float m0 = n * MoD - p;
                float m1 = n * MoD + p;

                if (m0 < 0.0f) {
                    m0 *= f / n;
                }
                if (m1 > 0.0f) {
                    m1 *= f / n;
                }

                if (obb_min > m1 || obb_max < m0) {
                    culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
                    return false;
                }
            }
        }

        // OBB Extents
        {
            for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
                const vec3& M = obb.axes[m];
                float MoR = fabsf(M.x);
                float MoU = fabsf(M.y);
                float MoD = M.z;
                float MoC = dot(M, obb.center);

                float obb_radius = obb.extents[m];

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = r * MoR + t * MoU;
                float m0 = n * MoD - p;
                float m1 = n * MoD + p;
                if (m0 < 0.0f) {
                    m0 *= f / n;
                }
                if (m1 > 0.0f) {
                    m1 *= f / n;
                }

                if (obb_min > m1 || obb_max < m0) {
                    culling_counters[CullingCounter::OBB_AXES]++;
                    return false;
                }
            }
        }

        // Now let's perform each of the cross products between the edges
        // First R x A_i
        {
            for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
                const vec3 M = { 0.0f, -obb.axes[m].z, obb.axes[m].y };
                float MoR = 0.0f;
                float MoU = fabsf(M.y);
                float MoD = M.z;
                float MoC = M.y * obb.center.y + M.z * obb.center.z;

                float obb_radius = obb.extents[m];

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = r * MoR + t * MoU;
                float m0 = n * MoD - p;
                float m1 = n * MoD + p;
                if (m0 < 0.0f) {
                    m0 *= f / n;
                }
                if (m1 > 0.0f) {
                    m1 *= f / n;
                }

                if (obb_min > m1 || obb_max < m0) {
                    culling_counters[CullingCounter::R_CROSS_A]++;
                    return false;
                }
            }
        }

        // U x A_i
        {
            for (size_t m = 0; m < ARRAY_SIZE(obb.axes); m++) {
                const vec3 M = { obb.axes[m].z, 0.0f, -obb.axes[m].x };
                float MoR = fabsf(M.x);
                float MoU = 0.0f;
                float MoD = M.z;
                float MoC = M.y * obb.center.y + M.z * obb.center.z;

                float obb_radius = obb.extents[m];

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = r * MoR + t * MoU;
                float m0 = n * MoD - p;
                float m1 = n * MoD + p;
                if (m0 < 0.0f) {
                    m0 *= f / n;
                }
                if (m1 > 0.0f) {
                    m1 *= f / n;
                }

                if (obb_min > m1 || obb_max < m0) {
                    culling_counters[CullingCounter::U_CROSS_A]++;
                    return false;
                }
            }
        }

        // Frustum Edges X Ai
        {
            for (size_t i = 0; i < ARRAY_SIZE(obb.axes); i++) {
                const vec3 M[] = {
                    cross({-r, 0.0f, n}, obb.a0), // Left Plane
                    cross({ r, 0.0f, n }, obb.a0), // Right plane
                    cross({ 0.0f, t, n }, obb.a0), // Top plane
                    cross({ 0.0, -t, n }, obb.a0) // Bottom plane
                };

                for (size_t m = 0; m < ARRAY_SIZE(M); m++) {
                    float MoR = fabsf(M[m].x);
                    float MoU = fabsf(M[m].y);
                    float MoD = M[m].z;
                    float MoC = dot(M[m], obb.center);

                    float obb_radius = obb.extents[m];

                    float obb_min = MoC - obb_radius;
                    float obb_max = MoC + obb_radius;

                    // Frustum projection
                    float p = r * MoR + t * MoU;
                    float m0 = n * MoD - p;
                    float m1 = n * MoD + p;
                    if (m0 < 0.0f) {
                        m0 *= f / n;
                    }
                    if (m1 > 0.0f) {
                        m1 *= f / n;
                    }

                    if (obb_min > m1 || obb_max < m0) {
                        culling_counters[CullingCounter::FRUSTUM_EDGE_CROSS_A]++;
                        return false;
                    }
                }
            }
        }

        // No intersections detected
        return true;
    };

    void cull_obbs_sac(const Camera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);

        for (size_t i = 0; i < CullingCounter::NUM_CULLING_COUNTERS; i++) {
            culling_counters[i] = 0;
        }

        float tan_fov = tan(0.5f * camera.vertical_fov);
        CullingFrustum frustum = {
            .left_bottom_near = vec3{ -camera.aspect_ratio * camera.near_plane * tan_fov, -camera.near_plane * tan_fov, -camera.near_plane },
            .right_top_far = vec3{ camera.aspect_ratio * camera.far_plane * tan_fov, camera.far_plane * tan_fov, -camera.far_plane },
        };
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform;
            matrix_mul_sse(camera.view, transforms[i], transform);

            const AABB& aabb = aabb_list[i];
            if (test_using_separating_axis_theorem(frustum, transform, aabb)) {
                out_visible_list.push_back(i);
            }
        }

        for (size_t i = 0; i < CullingCounter::NUM_CULLING_COUNTERS; i++) {
            PIXReportCounter(culling_counter_names[i], float(culling_counters[i]));
        }
    };
}
