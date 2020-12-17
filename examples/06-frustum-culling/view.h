#pragma once
#include "pch.h"
#include "core/array.h"
#include "core/zec_math.h"
#include "camera.h"

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

    void transform_aabb_to_clip_space(const Camera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<OBB>& out_obb_list)
    {
        ASSERT(transforms.size == aabb_list.size);
        if (out_obb_list.size < aabb_list.size) {
            out_obb_list.grow(aabb_list.size - out_obb_list.size);
        }

        mat4 VP = camera.projection * camera.view;
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform = VP * transforms[i];
            const AABB& aabb = aabb_list[i];

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
            for (size_t corner_idx = 0; corner_idx < ARRAY_SIZE(ms_corners); corner_idx++) {
                out_obb_list[i].corners[corner_idx] = transform * ms_corners[corner_idx];
            }
        }
    }

    void cull_obbs(const Array<OBB>& clip_space_obb_list, Array<u32>& out_visible_list)
    {
        ASSERT(out_visible_list.size == 0);
        const u32 num_obbs = u32(clip_space_obb_list.size);
        for (u32 i = 0; i < num_obbs; i++) {
            const OBB& obb = clip_space_obb_list[i];
            if (obb_in_frustum(obb)) {
                out_visible_list.push_back(i);
            }
        }
    };

};