#pragma once
#include "core/array.h"
#include "core/zec_math.h"
#include "gfx/gfx.h"
#include "camera.h"

namespace zec
{
    struct Scene
    {
        Array<PerspectiveCamera> perspective_cameras;
        std::vector<std::string> camera_names;

    };
}