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

    struct Entity { u32 idx; };

    class RenderScene
    {
    public:
        Array<PerspectiveCamera> perspective_cameras;
        std::vector<std::string> camera_names;

        // Components

        Array<Entity> parent_idx = {};
        // Transform data
        Array<vec3> positions = {};
        Array<quaternion> rotations;
        Array<vec3> scales = {};
        Array<mat4> local_transforms = {};
        Array<mat4> global_transforms = {};
        Array<mat3> normal_transforms = {};
        // Rendering data
        Array<MeshHandle> mesh_handles = {};
        Array<RawMaterialData> material_data = {};
    private:
    };
}