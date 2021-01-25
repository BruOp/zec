#pragma once
#include "camera.h"
#include "gfx/render_system.h"

#include "scene.h"
#include "./views.h"
#include "bounding_meshes.h"
#include "resource_names.h"

namespace clustered::DebugPass
{
    struct ClusterSetup
    {
        // The number of bins in each axis
        u32 width;
        u32 height;
        u32 depth;
    };

    struct Context
    {
        Context() = default;

        // Not owned
        bool active = true;
        zec::SceneViewHandle debug_view_idx;

        // Owned
        zec::MeshHandle debug_frustum_mesh = {};
        zec::BufferHandle debug_frustum_cb_handle = {};
        zec::ResourceLayoutHandle resource_layout = {};
        zec::PipelineStateHandle pso_handle = {};
    };

    struct FrustumDrawData
    {
        zec::mat4 inv_view;
        zec::vec4 color;
    };

    zec::RenderSystem::RenderPassDesc generate_desc(Context* context);

    void setup(void* context);

    void copy(void* context);

    void record(zec::RenderSystem::RenderList& render_list, zec::CommandContextHandle cmd_ctx, void* context);

    void destroy(void* context);
}

namespace clustered::UIPass
{
    struct Context
    {
        zec::RenderConfigState* render_config_state;
        zec::RenderScene* scene;
        size_t* active_camera_idx;
    };

    zec::RenderSystem::RenderPassDesc generate_desc(Context* context);

    void setup(void* context);

    void copy(void* context);

    void record(zec::RenderSystem::RenderList& render_list, zec::CommandContextHandle cmd_ctx, void* context);

    void destroy(void* context);
}
