#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"
#include "culling_methods.h"
#include "sat_frustum_culling_ispc.h"

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
        vec3 axes[3] = {};
    };

    struct AABB_SoA
    {
        Array<float> min_x;
        Array<float> min_y;
        Array<float> min_z;
        Array<float> max_x;
        Array<float> max_y;
        Array<float> max_z;
    };

    struct CullingFrustum
    {
        float near_right;
        float near_top;
        float near_plane;
        float far_plane;
    };

    Array<mat4> model_to_view_transforms;

    bool test_using_separating_axis_theorem(const CullingFrustum& frustum, const mat4& vs_transform, const AABB& aabb)
    {
        // Near, far
        float z_near = frustum.near_plane;
        float z_far = frustum.far_plane;
        // half width, half height
        float x_near = frustum.near_right;
        float y_near = frustum.near_top;

        // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
        vec3 corners[] = {
                    {aabb.min.x, aabb.min.y, aabb.min.z},
                    {aabb.max.x, aabb.min.y, aabb.min.z},
                    {aabb.min.x, aabb.max.y, aabb.min.z},
                    {aabb.min.x, aabb.min.y, aabb.max.z},
        };

        // Transform corners
        // This only translates to our OBB if our transform is affine
        for (size_t corner_idx = 0; corner_idx < std::size(corners); corner_idx++) {
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
        obb.extents = vec3{ length(obb.axes[0]), length(obb.axes[1]), length(obb.axes[2]) };
        obb.axes[0] = obb.axes[0] / obb.extents.x;
        obb.axes[1] = obb.axes[1] / obb.extents.y;
        obb.axes[2] = obb.axes[2] / obb.extents.z;
        obb.extents *= 0.5f;

        {
            vec3 M = { 0, 0, 1 };
            float MoX = 0.0f;
            float MoY = 0.0f;
            float MoZ = 1.0f;
            
            // Projected center of our OBB
            float MoC = obb.center.z;
            // Projected size of OBB
            float radius = 0.0f;
            for (size_t i = 0; i < 3; i++) {
                // dot(M, axes[i]) == axes[i].z;
                radius += fabsf(obb.axes[i].z) * obb.extents[i];
            }
            float obb_min = MoC - radius;
            float obb_max = MoC + radius;

            float tau_0 = z_far; // Since z is negative, far is smaller than near
            float tau_1 = z_near;

            if (obb_min > tau_1 || obb_max < tau_0) {
                culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
                return false;
            }
        }

        {
            const vec3 M[] = {
                { z_near, 0.0f, x_near }, // Left Plane
                { -z_near, 0.0f, x_near }, // Right plane
                { 0.0, -z_near, y_near }, // Top plane
                { 0.0, z_near, y_near }, // Bottom plane
            };
            for (size_t m = 0; m < std::size(M); m++) {
                float MoX = fabsf(M[m].x);
                float MoY = fabsf(M[m].y);
                float MoZ = M[m].z;
                float MoC = dot(M[m], obb.center);

                float obb_radius = 0.0f;
                for (size_t i = 0; i < 3; i++) {
                    obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
                }
                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                float p = x_near * MoX + y_near * MoY;

                float tau_0 = z_near * MoZ - p;
                float tau_1 = z_near * MoZ + p;

                if (tau_0 < 0.0f) {
                    tau_0 *= z_far / z_near;
                }
                if (tau_1 > 0.0f) {
                    tau_1 *= z_far / z_near;
                }

                if (obb_min > tau_1 || obb_max < tau_0) {
                    culling_counters[CullingCounter::FRUSTUM_NORMALS]++;
                    return false;
                }
            }
        }

        // OBB Axes
        {
            for (size_t m = 0; m < std::size(obb.axes); m++) {
                const vec3& M = obb.axes[m];
                float MoX = fabsf(M.x);
                float MoY = fabsf(M.y);
                float MoZ = M.z;
                float MoC = dot(M, obb.center);

                float obb_radius = obb.extents[m];

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = x_near * MoX + y_near * MoY;
                float tau_0 = z_near * MoZ - p;
                float tau_1 = z_near * MoZ + p;
                if (tau_0 < 0.0f) {
                    tau_0 *= z_far / z_near;
                }
                if (tau_1 > 0.0f) {
                    tau_1 *= z_far / z_near;
                }

                if (obb_min > tau_1 || obb_max < tau_0) {
                    culling_counters[CullingCounter::OBB_AXES]++;
                    return false;
                }
            }
        }

        // Now let's perform each of the cross products between the edges
        // First R x A_i
        {
            for (size_t m = 0; m < std::size(obb.axes); m++) {
                const vec3 M = { 0.0f, -obb.axes[m].z, obb.axes[m].y };
                float MoX = 0.0f;
                float MoY = fabsf(M.y);
                float MoZ = M.z;
                float MoC = M.y * obb.center.y + M.z * obb.center.z;

                float obb_radius = 0.0f;
                for (size_t i = 0; i < 3; i++) {
                    obb_radius += fabsf(dot(M, obb.axes[i])) * obb.extents[i];
                }

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = x_near * MoX + y_near * MoY;
                float tau_0 = z_near * MoZ - p;
                float tau_1 = z_near * MoZ + p;
                if (tau_0 < 0.0f) {
                    tau_0 *= z_far / z_near;
                }
                if (tau_1 > 0.0f) {
                    tau_1 *= z_far / z_near;
                }

                if (obb_min > tau_1 || obb_max < tau_0) {
                    culling_counters[CullingCounter::R_CROSS_A]++;
                    return false;
                }
            }
        }

        // U x A_i
        {
            for (size_t m = 0; m < std::size(obb.axes); m++) {
                const vec3 M = { obb.axes[m].z, 0.0f, -obb.axes[m].x };
                float MoX = fabsf(M.x);
                float MoY = 0.0f;
                float MoZ = M.z;
                float MoC = M.x * obb.center.x + M.z * obb.center.z;

                float obb_radius = 0.0f;
                for (size_t i = 0; i < 3; i++) {
                    obb_radius += fabsf(dot(M, obb.axes[i])) * obb.extents[i];
                }

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = x_near * MoX + y_near * MoY;
                float tau_0 = z_near * MoZ - p;
                float tau_1 = z_near * MoZ + p;
                if (tau_0 < 0.0f) {
                    tau_0 *= z_far / z_near;
                }
                if (tau_1 > 0.0f) {
                    tau_1 *= z_far / z_near;
                }

                if (obb_min > tau_1 || obb_max < tau_0) {
                    culling_counters[CullingCounter::U_CROSS_A]++;
                    return false;
                }
            }
        }

        // Frustum Edges X Ai
        {
            for (size_t obb_edge_idx = 0; obb_edge_idx < std::size(obb.axes); obb_edge_idx++) {
                const vec3 M[] = {
                    cross({-x_near, 0.0f, z_near}, obb.axes[obb_edge_idx]), // Left Plane
                    cross({ x_near, 0.0f, z_near }, obb.axes[obb_edge_idx]), // Right plane
                    cross({ 0.0f, y_near, z_near }, obb.axes[obb_edge_idx]), // Top plane
                    cross({ 0.0, -y_near, z_near }, obb.axes[obb_edge_idx]) // Bottom plane
                };

                for (size_t m = 0; m < std::size(M); m++) {
                    float MoX = fabsf(M[m].x);
                    float MoY = fabsf(M[m].y);
                    float MoZ = M[m].z;

                    constexpr float epsilon = 1e-4;
                    if (MoX < epsilon && MoY < epsilon && fabsf(MoZ) < epsilon) continue;

                    float MoC = dot(M[m], obb.center);

                    float obb_radius = 0.0f;
                    for (size_t i = 0; i < 3; i++) {
                        obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
                    }

                    float obb_min = MoC - obb_radius;
                    float obb_max = MoC + obb_radius;

                    // Frustum projection
                    float p = x_near * MoX + y_near * MoY;
                    float tau_0 = z_near * MoZ - p;
                    float tau_1 = z_near * MoZ + p;
                    if (tau_0 < 0.0f) {
                        tau_0 *= z_far / z_near;
                    }
                    if (tau_1 > 0.0f) {
                        tau_1 *= z_far / z_near;
                    }

                    if (obb_min > tau_1 || obb_max < tau_0) {
                        culling_counters[CullingCounter::FRUSTUM_EDGE_CROSS_A]++;
                        return false;
                    }
                }
            }
        }

        // No intersections detected
        return true;
    };

    void cull_obbs_sac(const PerspectiveCamera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);

        for (size_t i = 0; i < CullingCounter::NUM_CULLING_COUNTERS; i++) {
            culling_counters[i] = 0;
        }

        float tan_fov = tan(0.5f * camera.vertical_fov);
        CullingFrustum frustum = {
            .near_right = camera.aspect_ratio * camera.near_plane * tan_fov,
            .near_top = camera.near_plane * tan_fov,
            .near_plane = -camera.near_plane,
            .far_plane = -camera.far_plane,
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

    void cull_obbs_sac_ispc(const PerspectiveCamera& camera, const Array<mat4>& transforms, const AABB_SoA& aabb_soa, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);
        if (out_visible_list.capacity < transforms.size) {
            out_visible_list.reserve(transforms.size);
        }
        if (model_to_view_transforms.size < transforms.size) {
            model_to_view_transforms.grow(transforms.size - model_to_view_transforms.size);
        }

        float tan_fov = tan(0.5f * camera.vertical_fov);
        ispc::CullingFrustum frustum = {
            .near_right = camera.aspect_ratio * camera.near_plane * tan_fov,
            .near_top = camera.near_plane * tan_fov,
            .near_plane = -camera.near_plane,
            .far_plane = -camera.far_plane,
        };

        for (size_t i = 0; i < transforms.size; i++) {
            matrix_mul_sse(camera.view, transforms[i], model_to_view_transforms[i]);
        }

        u32 num_visible = 0;
        ispc::cull_obbs_ispc(
            frustum,
            reinterpret_cast<const ispc::mat4*>(model_to_view_transforms.data),
            aabb_soa.min_x.data,
            aabb_soa.min_y.data,
            aabb_soa.min_z.data,
            aabb_soa.max_x.data,
            aabb_soa.max_y.data,
            aabb_soa.max_z.data,
            transforms.size,
            out_visible_list.data,
            &num_visible);
        out_visible_list.size = num_visible;
    }
}
