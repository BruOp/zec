#pragma once

#include "core/array.h"
#include "core/zec_math.h"
#include "gfx/rhi_public_resources.h"
#include "gfx/rhi.h"

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

        struct SceneGraph
        {
            VirtualArray<u32> parent_ids = {};
            VirtualArray<vec3> positions = {};
            VirtualArray<quaternion> rotations = {};
            VirtualArray<vec3> scales = {};
            VirtualArray<mat4> global_transforms = {};
            VirtualArray<mat3> normal_transforms = {};
        };

        struct DrawCall
        {
            u32 draw_idx = UINT32_MAX;
            u32 scene_node_idx = UINT32_MAX;
            u32 material_index = UINT32_MAX;
        };

        struct Context
        {
            SceneGraph scene_graph = {};
            VirtualArray<rhi::TextureHandle> textures = {};
            VirtualArray<rhi::Draw> draws = {};
            VirtualArray<AABB> aabbs = {};
            VirtualArray<MaterialData> materials = {};
            VirtualArray<DrawCall> draw_calls = {};
        };

        void load_gltf_file(const char* gltf_file_path, rhi::CommandContextHandle cmd_ctx, Context& context, const LoaderFlags flags = GLTF_LOADING_FLAG_NONE);
    };
}