#pragma once
#include "core/array.h"
#include "core/zec_math.h"
#include "gfx/gfx.h"
#include "camera.h"

namespace zec
{
    struct RawMaterialData
    {
        float data[64] = {};
    };

    struct Scene
    {
        Array<PerspectiveCamera> perspective_cameras;
        std::vector<std::string> camera_names;

        Array<mat4> local_transforms;
        Array<mat4> global_transforms;
        Array<RawMaterialData> material_data;
    };
}