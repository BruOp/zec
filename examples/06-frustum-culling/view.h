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
        Array<u32>* identifiers;
        Array<AABB>* aabb_list;
        Array<OBB>* clip_space_obb_list;
    };

    void transform_aabb_to_clip_space(const Camera& camera, const Array<mat4>& transforms, const Array<AABB>& aabb_list, Array<OBB>& out_obb_list)
    {
        ASSERT(transforms.size == aabb_list.size);
        out_obb_list.reserve(aabb_list.size);
        mat4 VP = camera.view * camera.projection;
        for (size_t i = 0; i < aabb_list.size; i++) {
            mat4 transform = VP * transforms[i];
            const AABB& aabb = aabb_list[i];
            out_obb_list[i] = {
                .min = transform * aabb.min,
                .max = transform * aabb.max,
            };
        }
    }

    void cull_aabbs(const View& view, Array<u32>& out_visible_list)
    {
        ASSERT(view.identifiers->size == view.clip_space_obb_list->size);
        for (size_t i = 0; i < view.identifiers->size; i++) {
            const OBB& obb = (*view.clip_space_obb_list)[i];
            if (obb_in_frustum(obb)) {
                out_visible_list.push_back((*view.identifiers)[i]);
            }
        }
    };

};