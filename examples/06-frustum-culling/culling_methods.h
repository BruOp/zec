#pragma once

#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"
#include <xmmintrin.h>
#include <ftl/task_scheduler.h>
#include <ftl/task_counter.h>

#define SPLAT(v, c) _mm_permute_ps(v, _MM_SHUFFLE(c, c, c, c))

namespace zec
{

    constexpr size_t NUM_ITEMS_PER_LIST = 1024;

    struct ViewFrustumCullTaskData
    {
        mat4 const* VP = nullptr;
        AABB const* aabbs = nullptr;
        mat4 const* model_transforms = nullptr;
        u32* visibility_list;
        u32 offset = 0;
        u32 num_to_process = 0;
        u32 size = 0;
    };

    static Array<ftl::Task> tasks = {};
    static Array<ViewFrustumCullTaskData> task_data = {};
    static Array<u32> temp_visibility_list = {};

    // ---------- SSE Methods ----------

    void matrix_mul_sse(const mat4& lhs, const mat4& rhs, mat4& dest)
    {
        for (size_t i = 0; i < std::size(lhs.rows); i++) {
            __m128 v_x = _mm_broadcast_ss(static_cast<const float*>(&lhs.rows[i].x));
            __m128 v_y = _mm_broadcast_ss(static_cast<const float*>(&lhs.rows[i].y));
            __m128 v_z = _mm_broadcast_ss(static_cast<const float*>(&lhs.rows[i].z));
            __m128 v_w = _mm_broadcast_ss(static_cast<const float*>(&lhs.rows[i].w));

            v_x = _mm_mul_ps(v_x, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[0])));
            v_x = _mm_fmadd_ps(v_y, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[1])), v_x);
            v_x = _mm_fmadd_ps(v_z, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[2])), v_x);
            v_x = _mm_fmadd_ps(v_w, _mm_load_ps(reinterpret_cast<const float*>(&rhs.rows[3])), v_x);
            _mm_store_ps(reinterpret_cast<float*>(&dest.rows[i]), v_x);
        }
    };

    void transform_points_4(__m128 dest[4], const __m128 x, const __m128 y, const __m128 z, const mat4& transform)
    {
        for (size_t i = 0; i < 4; ++i) {
            // This first one is because our 4 points would all have w == 1.0f, so we can just skip the multiply
            __m128 res = _mm_broadcast_ss(&transform.rows[i].w);
            res = _mm_fmadd_ps(_mm_broadcast_ss(&transform.rows[i].x), x, res);
            res = _mm_fmadd_ps(_mm_broadcast_ss(&transform.rows[i].y), y, res);
            res = _mm_fmadd_ps(_mm_broadcast_ss(&transform.rows[i].z), z, res);
            dest[i] = res;
        }
    }

    void transform_points_8(__m256 dest[4], const __m256 x, const __m256 y, const __m256 z, const mat4& transform)
    {
        for (size_t i = 0; i < 4; ++i) {
            __m256 res = _mm256_broadcast_ss(&transform.rows[i].w);
            res = _mm256_fmadd_ps(_mm256_broadcast_ss(&transform.rows[i].x), x, res);
            res = _mm256_fmadd_ps(_mm256_broadcast_ss(&transform.rows[i].y), y, res);
            res = _mm256_fmadd_ps(_mm256_broadcast_ss(&transform.rows[i].z), z, res);
            dest[i] = res;
        }
    }

    bool test_aabb_visiblity_against_frustum_128(mat4& transform, const AABB& aabb)
    {
        vec4 min{ aabb.min, 1.0f };
        vec4 max{ aabb.max, 1.0f };
        const __m128 aabb_min = _mm_load_ps(&min.x);
        const __m128 aabb_max = _mm_load_ps(&max.x);

        __m128 x = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(0, 0, 0, 0));
        x = _mm_permute_ps(x, _MM_SHUFFLE(2, 0, 2, 0)); // x X x X
        const __m128 y = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(1, 1, 1, 1)); // y y Y Y
        const __m128 z_min = SPLAT(aabb_min, 3); // z z z z
        const __m128 z_max = SPLAT(aabb_max, 3);  // Z Z Z Z

        // corners[0] = { x, x, x, x} ... corners[4] = {w, w, w, w};
        __m128 min_corners[4];
        __m128 max_corners[4];

        transform_points_4(min_corners, x, y, z_min, transform);
        transform_points_4(max_corners, x, y, z_max, transform);

        const __m128 min_neg_ws = _mm_sub_ps(_mm_setzero_ps(), min_corners[3]);
        const __m128 max_neg_ws = _mm_sub_ps(_mm_setzero_ps(), max_corners[3]);

        // Test whether -w <= x <= w
        __m128 min_inside = _mm_and_ps(
            _mm_cmple_ps(min_neg_ws, min_corners[0]),
            _mm_cmple_ps(min_corners[0], min_corners[3])
        );
        // inside && (-w <= y <= w)
        min_inside = _mm_and_ps(
            min_inside,
            _mm_and_ps(
                _mm_cmple_ps(min_neg_ws, min_corners[1]),
                _mm_cmple_ps(min_corners[1], min_corners[3])
            )
        );
        // inside && 0 < z < w
        min_inside = _mm_and_ps(
            min_inside,
            _mm_and_ps(
                _mm_cmple_ps(_mm_setzero_ps(), min_corners[2]),
                _mm_cmple_ps(min_corners[2], min_corners[3])
            )
        );

        // Test whether -w < x < w
        __m128 max_inside = _mm_and_ps(
            _mm_cmple_ps(max_neg_ws, max_corners[0]),
            _mm_cmple_ps(max_corners[0], max_corners[3])
        );
        // inside && -w < y < w
        max_inside = _mm_and_ps(
            max_inside,
            _mm_and_ps(
                _mm_cmple_ps(max_neg_ws, max_corners[1]),
                _mm_cmple_ps(max_corners[1], max_corners[3])
            )
        );
        // inside && 0 < z < w
        max_inside = _mm_and_ps(
            max_inside,
            _mm_and_ps(
                _mm_cmple_ps(_mm_setzero_ps(), max_corners[2]),
                _mm_cmple_ps(max_corners[2], max_corners[3])
            )
        );

        // Perform ORs to reduce our lanes, since we want to know if _any_ point was "inside"
        __m128 inside = _mm_or_ps(min_inside, max_inside);
        // The following is equivalent to
        // { inside[0] || inside[2], inside[1] || inside[3], inside[2] || inside[2], inside[3] || inside[3] }
        inside = _mm_or_ps(inside, _mm_permute_ps(inside, _MM_SHUFFLE(2, 3, 2, 3)));
        // Then we perform another OR to fill the lowest lane with the final reduction
        // { (inside[0] || inside[2]) || (inside[1] || inside[3]), ... }
        inside = _mm_or_ps(inside, _mm_permute_ps(inside, _MM_SHUFFLE(1, 1, 1, 1)));
        // Store our reduction
        u32 res = 0u;
        _mm_store_ss(reinterpret_cast<float*>(&res), inside);
        return res != 0;
    }

    bool test_aabb_visiblity_against_frustum_256(mat4& transform, const AABB& aabb)
    {
        vec4 min{ aabb.min, 1.0f };
        vec4 max{ aabb.max, 1.0f };
        const __m128 aabb_min = _mm_load_ps(&min.x);
        const __m128 aabb_max = _mm_load_ps(&max.x);

        __m128 x_minmax = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(0, 0, 0, 0));
        x_minmax = _mm_permute_ps(x_minmax, _MM_SHUFFLE(2, 0, 2, 0)); // x X x X
        const __m128 y_minmax = _mm_shuffle_ps(aabb_min, aabb_max, _MM_SHUFFLE(1, 1, 1, 1)); // y y Y Y
        const __m128 z_min = SPLAT(aabb_min, 2); // z z z z
        const __m128 z_max = SPLAT(aabb_max, 2);  // Z Z Z Z

        // Each __m256 represents a single component of 8 vertices
        const __m256 x = _mm256_set_m128(x_minmax, x_minmax);
        const __m256 y = _mm256_set_m128(y_minmax, y_minmax);
        const __m256 z = _mm256_set_m128(z_min, z_max);

        // corner_comps[0] = { x, x, x, x, ... } ... corners[4] = {w, w, w, w ...};
        __m256 corner_comps[4];

        transform_points_8(corner_comps, x, y, z, transform);

        const __m256 neg_ws = _mm256_sub_ps(_mm256_setzero_ps(), corner_comps[3]);

        // Test whether -w < x < w
        __m256 inside = _mm256_and_ps(
            _mm256_cmp_ps(neg_ws, corner_comps[0], _CMP_LE_OQ),
            _mm256_cmp_ps(corner_comps[0], corner_comps[3], _CMP_LE_OQ)
        );
        // inside && -w < y < w
        inside = _mm256_and_ps(
            inside,
            _mm256_and_ps(
                _mm256_cmp_ps(neg_ws, corner_comps[1], _CMP_LE_OQ),
                _mm256_cmp_ps(corner_comps[1], corner_comps[3], _CMP_LE_OQ)
            )
        );
        // inside && 0 < z < w
        inside = _mm256_and_ps(
            inside,
            _mm256_and_ps(
                _mm256_cmp_ps(neg_ws, corner_comps[2], _CMP_LE_OQ),
                _mm256_cmp_ps(corner_comps[2], corner_comps[3], _CMP_LE_OQ)
            )
        );

        __m128 reduction = _mm_or_ps(_mm256_extractf128_ps(inside, 0), _mm256_extractf128_ps(inside, 1));
        // The following fills the first two lanes such that
        // reduction[0] = reduction[0] || reduction[2]
        // reduction[1] = reduction[1] || reduction[3]
        reduction = _mm_or_ps(reduction, _mm_permute_ps(reduction, _MM_SHUFFLE(3, 2, 3, 2)));
        // Then we perform another OR to fill the lowest lane with the final reduction
        // reduction[0] = (reduction[0] || reduction[2]) || (reduction[1] || reduction[3])
        reduction = _mm_or_ps(reduction, _mm_permute_ps(reduction, _MM_SHUFFLE(1, 1, 1, 1)));
        // Store our reduction
        u32 res = 0u;
        _mm_store_ss(reinterpret_cast<float*>(&res), reduction);
        return res != 0;
    }

    void cull_obbs_sse(const PerspectiveCamera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);

        mat4 VP;
        matrix_mul_sse(camera.projection, camera.view, VP);
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform;
            matrix_mul_sse(VP, transforms[i], transform);

            const AABB& aabb = aabb_list[i];
            if (test_aabb_visiblity_against_frustum_256(transform, aabb)) {
                out_visible_list.push_back(i);
            }
        }
    }

    // ---------- Scalar Methods ----------

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
        for (size_t corner_idx = 0; corner_idx < std::size(ms_corners); corner_idx++) {
            obb.corners[corner_idx] = transform * ms_corners[corner_idx];
        }

        return obb_in_frustum(obb);
    }

    void cull_obbs(const PerspectiveCamera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);
        mat4 VP = camera.projection * camera.view;
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform = VP * transforms[i];

            const AABB& aabb = aabb_list[i];
            if (test_aabb_visiblity_against_frustum(transform, aabb)) {
                out_visible_list.push_back(i);
            }
        }
    }

    // ---------- Multi-threaded Methods ----------

    void cull_obbs_task(ftl::TaskScheduler* task_scheduler, void* arg)
    {
        (void)task_scheduler;
        ViewFrustumCullTaskData* task_data = static_cast<ViewFrustumCullTaskData*>(arg);

        for (size_t i = 0; i < task_data->num_to_process; i++) {
            mat4 transform;
            matrix_mul_sse(*task_data->VP, task_data->model_transforms[i], transform);

            const AABB& aabb = task_data->aabbs[i];
            if (test_aabb_visiblity_against_frustum_256(transform, aabb)) {
                task_data->visibility_list[task_data->size++] = task_data->offset + i;
            }
        }
    };

    void cull_obbs_mt(ftl::TaskScheduler& task_scheduler, const PerspectiveCamera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);

        mat4 VP;
        matrix_mul_sse(camera.projection, camera.view, VP);

        size_t num_tasks = (aabb_list.size + NUM_ITEMS_PER_LIST - 1) / NUM_ITEMS_PER_LIST;

        if (temp_visibility_list.size < aabb_list.size) {
            temp_visibility_list.grow(aabb_list.size - temp_visibility_list.size);
        }

        if (tasks.size < num_tasks) {
            tasks.grow(num_tasks - tasks.size);
            task_data.grow(num_tasks - task_data.size);
        }

        // Fork
        for (size_t task_idx = 0; task_idx < num_tasks; task_idx++) {
            size_t offset = task_idx * num_tasks;
            task_data[task_idx].VP = &VP;
            task_data[task_idx].aabbs = &aabb_list[offset];
            task_data[task_idx].model_transforms = &transforms[offset];
            task_data[task_idx].visibility_list = &temp_visibility_list[offset];
            task_data[task_idx].offset = u32(offset);
            task_data[task_idx].num_to_process = u32(min(NUM_ITEMS_PER_LIST, aabb_list.size - offset));
            task_data[task_idx].size = 0;
            tasks[task_idx].Function = &cull_obbs_task;
            tasks[task_idx].ArgData = &task_data[task_idx];
        }

        ftl::TaskCounter counter(&task_scheduler);
        task_scheduler.AddTasks(num_tasks, tasks.data, ftl::TaskPriority::High, &counter);

        if (out_visible_list.capacity < aabb_list.size) {
            out_visible_list.reserve(aabb_list.size);
        }

        task_scheduler.WaitForCounter(&counter, 0);

        // Reduce
        for (size_t task_idx = 0; task_idx < num_tasks; task_idx++) {
            auto& task_datum = task_data[task_idx];

            u32* dest = out_visible_list.data + out_visible_list.size;
            memory::copy(dest, task_datum.visibility_list, task_datum.size * sizeof(u32));
            out_visible_list.size += task_datum.size;
        }
    };
};

#undef SPLAT