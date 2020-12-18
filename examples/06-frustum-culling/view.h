#pragma once
#include "pch.h"
#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"
#include <xmmintrin.h>

#define SPLAT(v, c) _mm_permute_ps(v, _MM_SHUFFLE(c, c, c, c))
#define SPLAT256(v, c) _mm256_permute_ps(v, _MM_SHUFFLE(c, c, c,c))

namespace zec
{
    //struct AABBSetSSE
    //{
    //    float x[8];
    //    float y[8];
    //    float z[8];
    //};

    struct Frustum
    {
        Plane planes[6];
    };

    enum struct CameraType : u8
    {
        PERSPECTIVE = 0,
        ORTHOGRAPHIC
    };

    struct View
    {
        //CameraType camera_type = CameraType::PERSPECTIVE;
        Camera* camera;
        Array<AABB>* aabb_list;
        Array<OBB>* clip_space_obb_list;
    };

    void matrix_mul_sse(const mat4& lhs, const mat4& rhs, mat4& dest)
    {
        for (size_t i = 0; i < ARRAY_SIZE(lhs.rows); i++) {
            __m128 v_x = _mm_broadcast_ss(reinterpret_cast<const float*>(&lhs.rows[i].x));
            __m128 v_y = _mm_broadcast_ss(reinterpret_cast<const float*>(&lhs.rows[i].y));
            __m128 v_z = _mm_broadcast_ss(reinterpret_cast<const float*>(&lhs.rows[i].z));
            __m128 v_w = _mm_broadcast_ss(reinterpret_cast<const float*>(&lhs.rows[i].w));

            v_x = _mm_mul_ps(v_x, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[0])));
            v_x = _mm_fmadd_ps(v_y, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[1])), v_x);
            v_x = _mm_fmadd_ps(v_z, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[2])), v_x);
            v_x = _mm_fmadd_ps(v_w, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[3])), v_x);
            _mm_store_ps(reinterpret_cast<float*>(&dest.rows[i]), v_x);
        }
    };


    void transform_points_4(__m128 dest[4], const __m128 x, const __m128 y, const __m128 z, const mat4& transform)
    {

        // This first one is because our 4 points would all have w == 1.0f, so we can just skip the multiply
        __m128 res = SPLAT(_mm_load_ps(&transform.rows[0].x), 3);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[0].x), 0), x, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[0].x), 1), y, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[0].x), 2), z, res);
        dest[0] = res;

        res = SPLAT(_mm_load_ps(&transform.rows[1].x), 3);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[1].x), 0), x, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[1].x), 1), y, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[1].x), 2), z, res);
        dest[1] = res;

        res = SPLAT(_mm_load_ps(&transform.rows[2].x), 3);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[2].x), 0), x, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[2].x), 1), y, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[2].x), 2), z, res);
        dest[2] = res;

        res = SPLAT(_mm_load_ps(&transform.rows[3].x), 3);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[3].x), 0), x, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[3].x), 1), y, res);
        res = _mm_fmadd_ps(SPLAT(_mm_load_ps(&transform.rows[3].x), 2), z, res);
        dest[3] = res;
    }

    void transform_points_8(__m256 dest[4], const __m256 x, const __m256 y, const __m256 z, const mat4& transform)
    {
        // This first one is because our 4 points would all have w == 1.0f, so we can just skip the multiply
        __m256 res = SPLAT256(_mm256_load_ps(&transform.rows[0].x), 3);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[0].x), 0), x, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[0].x), 1), y, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[0].x), 2), z, res);
        dest[0] = res;

        res = SPLAT256(_mm256_load_ps(&transform.rows[1].x), 3);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[1].x), 0), x, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[1].x), 1), y, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[1].x), 2), z, res);
        dest[1] = res;

        res = SPLAT256(_mm256_load_ps(&transform.rows[2].x), 3);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[2].x), 0), x, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[2].x), 1), y, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[2].x), 2), z, res);
        dest[2] = res;

        res = SPLAT256(_mm256_load_ps(&transform.rows[3].x), 3);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[3].x), 0), x, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[3].x), 1), y, res);
        res = _mm256_fmadd_ps(SPLAT256(_mm256_load_ps(&transform.rows[3].x), 2), z, res);
        dest[3] = res;
    }

    bool test_aabb_visiblity_against_frustum_sse(mat4& transform, const AABB& aabb)
    {
        vec4 min{ aabb.min, 1.0f };
        vec4 max{ aabb.max, 1.0f };
        const __m128 aabb_min = _mm_load_ps(&min.x);
        const __m128 aabb_max = _mm_load_ps(&max.x);

        __m128 minmax_x = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(0, 0, 0, 0));
        minmax_x = _mm_shuffle_ps(minmax_x, minmax_x, _MM_SHUFFLE(2, 0, 2, 0)); // x X x X
        const __m128 minmax_y = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(1, 1, 1, 1)); // y y Y Y
        const __m128 z_near = _mm_permute_ps(aabb_min, _MM_SHUFFLE(2, 2, 2, 2)); // z z z z
        const __m128 z_far = _mm_permute_ps(aabb_max, _MM_SHUFFLE(2, 2, 2, 2));  // Z Z Z Z

        // corners[0] = { x, x, x, x} ... corners[4] = {w, w, w, w};
        __m128 near_corners[4];
        __m128 far_corners[4];

        transform_points_4(near_corners, minmax_x, minmax_y, z_near, transform);
        transform_points_4(far_corners, minmax_x, minmax_y, z_far, transform);

        const __m128 near_neg_ws = _mm_sub_ps(_mm_setzero_ps(), near_corners[3]);
        const __m128 far_neg_ws = _mm_sub_ps(_mm_setzero_ps(), far_corners[3]);

        // Test whether -w < x < w
        __m128 near_inside = _mm_cmplt_ps(near_neg_ws, near_corners[0]);
        near_inside = _mm_and_ps(near_inside, _mm_cmplt_ps(near_corners[0], near_corners[3]));
        // inside && -w < y < w
        near_inside = _mm_and_ps(near_inside, _mm_and_ps(_mm_cmplt_ps(near_neg_ws, near_corners[1]), _mm_cmplt_ps(near_corners[1], near_corners[3])));
        // inside && 0 < z < w
        near_inside = _mm_and_ps(near_inside, _mm_and_ps(_mm_cmplt_ps(_mm_setzero_ps(), near_corners[2]), _mm_cmplt_ps(near_corners[2], near_corners[3])));

        // Test whether -w < x < w
        __m128 far_inside = _mm_and_ps(_mm_cmplt_ps(far_neg_ws, far_corners[0]), _mm_cmplt_ps(far_corners[0], far_corners[3]));
        // inside && -w < y < w
        far_inside = _mm_and_ps(far_inside, _mm_and_ps(_mm_cmplt_ps(far_neg_ws, far_corners[1]), _mm_cmplt_ps(far_corners[1], far_corners[3])));
        // inside && 0 < z < w
        far_inside = _mm_and_ps(far_inside, _mm_and_ps(_mm_cmplt_ps(_mm_setzero_ps(), far_corners[2]), _mm_cmplt_ps(far_corners[2], far_corners[3])));

        __m128 inside = _mm_or_ps(near_inside, far_inside);
        // This will fill { inside[0] + inside[1], inside[2] + inside[3], inside[0] + inside[1], inside[2] + inside[3] }
        inside = _mm_hadd_ps(inside, inside);
        // This will fill { inside[0] + inside[1] + inside[2] + inside[3], ... }
        inside = _mm_hadd_ps(inside, inside);
        u32 res = 0u;
        _mm_store_ss(reinterpret_cast<float*>(&res), inside);
        return res != 0;
    }

    bool test_aabb_visiblity_against_frustum256(mat4& transform, const AABB& aabb)
    {
        vec4 min{ aabb.min, 1.0f };
        vec4 max{ aabb.max, 1.0f };
        const __m128 aabb_min = _mm_load_ps(&min.x);
        const __m128 aabb_max = _mm_load_ps(&max.x);

        __m128 minmax_x = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(0, 0, 0, 0));
        minmax_x = _mm_shuffle_ps(minmax_x, minmax_x, _MM_SHUFFLE(2, 0, 2, 0)); // x X x X
        const __m128 minmax_y = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(1, 1, 1, 1)); // y y Y Y
        const __m128 minmax_z[] = {
            SPLAT(aabb_min, 2), // z z z z
            SPLAT(aabb_max, 2)  // Z Z Z Z
        };

        const __m256 x = _mm256_broadcast_ps(&minmax_x);
        const __m256 y = _mm256_broadcast_ps(&minmax_y);
        const __m256 z = _mm256_load_ps(reinterpret_cast<const float*>(minmax_z));

        // corners[0] = { x, x, x, x, ... } ... corners[4] = {w, w, w, w ...};
        __m256 corners[4];

        transform_points_8(corners, x, y, z, transform);

        const __m256 neg_ws = _mm256_sub_ps(_mm256_setzero_ps(), corners[3]);

        // Test whether -w < x < w
        __m256 inside = _mm256_cmp_ps(neg_ws, corners[0], _CMP_LT_OQ);
        inside = _mm256_and_ps(inside, _mm256_cmp_ps(corners[0], corners[3], _CMP_LT_OQ));
        // inside && -w < y < w
        inside = _mm256_and_ps(inside, _mm256_and_ps(_mm256_cmp_ps(neg_ws, corners[1], _CMP_LT_OQ), _mm256_cmp_ps(corners[1], corners[3], _CMP_LT_OQ)));
        // inside && 0 < z < w
        inside = _mm256_and_ps(inside, _mm256_and_ps(_mm256_cmp_ps(neg_ws, corners[2], _CMP_LT_OQ), _mm256_cmp_ps(corners[2], corners[3], _CMP_LT_OQ)));

        u32 res[8];
        _mm256_store_ps(reinterpret_cast<float*>(res), inside);
        for (size_t i = 1; i < ARRAY_SIZE(res); i++) {
            res[0] |= res[i];
        }
        return res[0] != 0;
    }

    void cull_obbs_sse(const Camera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        mat4 VP;
        matrix_mul_sse(camera.projection, camera.view, VP);
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform;
            matrix_mul_sse(VP, transforms[i], transform);

            const AABB& aabb = aabb_list[i];
            if (test_aabb_visiblity_against_frustum_sse(transform, aabb)) {
                out_visible_list.push_back(i);
            }
        }
    }

    bool test_aabb_visiblity_against_frustum(mat4& transform, const AABB& aabb)
    {

        vec4 ms_corners[8] = {
            {aabb.min.x, aabb.min.y, aabb.min.z, 1.0},
            {aabb.max.x, aabb.min.y, aabb.min.z, 1.0},
            {aabb.min.x, aabb.max.y, aabb.min.z, 1.0},
            {aabb.max.x, aabb.max.y, aabb.min.z, 1.0},

            {aabb.min.x, aabb.min.y, aabb.max.z, 1.0},
            {aabb.max.x, aabb.min.y, aabb.max.z, 1.0},
            {aabb.min.x, aabb.max.y, aabb.max.z, 1.0},
            {aabb.max.x, aabb.max.y, aabb.max.z, 1.0},
        };
        OBB obb;

        // Transform corners
        for (size_t corner_idx = 0; corner_idx < ARRAY_SIZE(ms_corners); corner_idx++) {
            obb.corners[corner_idx] = transform * ms_corners[corner_idx];
        }

        return obb_in_frustum(obb);
    }

    void cull_obbs(const Camera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        mat4 VP = camera.projection * camera.view;
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform = VP * transforms[i];

            const AABB& aabb = aabb_list[i];
            if (test_aabb_visiblity_against_frustum(transform, aabb)) {
                out_visible_list.push_back(i);
            }
        }
    }
};

#undef SPLAT