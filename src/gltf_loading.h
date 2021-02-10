#pragma once
#include "core/array.h"
#include "core/zec_math.h"
#include "gfx/public_resources.h"
#include "gfx/gfx.h"

namespace zec
{
    namespace gltf
    {
        enum LoaderFlags : u32
        {
            GLTF_LOADING_FLAG_NONE = 0,
            // We're loading a glb
            GLTF_LOADING_BINARY_FORMAT = (1 << 0),
        };

        struct MaterialData
        {
            vec4 base_color_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
            vec3 emissive_factor = { 0.0f, 0.0f, 0.0f };
            float metallic_factor = 1.0f;
            float roughness_factor = 1.0f;
            u32 base_color_texture_idx = UINT32_MAX;
            u32 metallic_roughness_texture_idx = UINT32_MAX;
            u32 normal_texture_idx = UINT32_MAX;
            u32 occlusion_texture_idx = UINT32_MAX;
            u32 emissive_texture_idx = UINT32_MAX;
        };

        struct AABB
        {
            vec3 max = {};
            vec3 min = {};
        };

        struct SceneGraph
        {
            Array<u32> parent_ids = {};
            Array<vec3> positions = {};
            Array<quaternion> rotations = {};
            Array<vec3> scales = {};
            Array<mat4> global_transforms = {};
            Array<mat3> normal_transforms = {};
        };

        struct DrawCall
        {
            MeshHandle mesh = INVALID_HANDLE;
            u32 scene_node_idx = UINT32_MAX;
            u32 material_index = UINT32_MAX;
        };

        struct Context
        {
            SceneGraph scene_graph = {};
            Array<TextureHandle> textures = {};
            Array<MeshHandle> meshes = {};
            Array<AABB> aabbs = {};
            Array<MaterialData> materials = {};
            Array<DrawCall> draw_calls = {};
        };

        void load_gltf_file(const std::string& gltf_file, CommandContextHandle cmd_ctx, Context& context, const LoaderFlags flags);
    };
}