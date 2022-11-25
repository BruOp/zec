#pragma once
#include <core/zec_types.h>
#include <utils/crc_hash.h>
#include <gfx/render_task_system.h>
#include "renderable_scene.h"

namespace clustered
{
#define MAKE_IDENTIFIER(_VAR_NAME) \
    constexpr zec::render_graph::ResourceIdentifier _VAR_NAME = { .identifier = ctcrc32(#_VAR_NAME) };

    namespace settings
    {
        MAKE_IDENTIFIER(MAIN_PASS_VIEW_CB);
        MAKE_IDENTIFIER(RENDERABLE_SCENE_PTR);
        MAKE_IDENTIFIER(CLUSTER_GRID_SETUP);
        MAKE_IDENTIFIER(BACKGROUND_CUBE_MAP);
        MAKE_IDENTIFIER(FULLSCREEN_QUAD);
        MAKE_IDENTIFIER(EXPOSURE);
    };

    namespace pass_resources
    {
        MAKE_IDENTIFIER(DEPTH_TARGET);
        MAKE_IDENTIFIER(HDR_TARGET);
        MAKE_IDENTIFIER(SDR_TARGET);
        MAKE_IDENTIFIER(SPOT_LIGHT_INDICES);
        MAKE_IDENTIFIER(POINT_LIGHT_INDICES);
    }

    enum EResourceLayoutIds : u32
    {
        DEPTH_PASS_RESOURCE_LAYOUT = 0,
        BACKGROUND_PASS_RESOURCE_LAYOUT,
        FORWARD_PASS_RESOURCE_LAYOUT,
        TONE_MAPPING_PASS_RESOURCE_LAYOUT,
        LIGHT_BINNING_PASS_RESOURCE_LAYOUT,
        CLUSTERED_DEBUG_PASS_RESOURCE_LAYOUT,
        RESOURCE_LAYOUT_COUNT
    };


    enum EShaderIds : u32
    {
        DEPTH_PASS_SHADER = 0,
        BACKGROUND_PASS_SHADER,
        FORWARD_PASS_SHADER,
        TONE_MAPPING_PASS_SHADER,
        LIGHT_BINNING_PASS_SPOT_LIGHT_SHADER,
        LIGHT_BINNING_PASS_POINT_LIGHT_SHADER,
        CLUSTERED_DEBUG_PASS_SHADER,
        SHADERS_COUNT
    };


    enum EPipelineIds : u32
    {
        DEPTH_PASS_PIPELINE = 0,
        BACKGROUND_PASS_PIPELINE,
        FORWARD_PASS_PIPELINE,
        TONE_MAPPING_PASS_PIPELINE,
        LIGHT_BINNING_PASS_POINT_LIGHT_PIPELINE,
        LIGHT_BINNING_PASS_SPOT_LIGHT_PIPELINE,
        CLUSTERED_DEBUG_PASS_PIPELINE,
        PIPELINES_COUNT,
    };

    zec::ResourceLayoutDesc get_resource_layout_desc(const EResourceLayoutIds& layouts_enum);
    zec::ShaderCompilationDesc get_shader_compilation_desc(const EShaderIds& shaders_enum);
    zec::PipelineStateObjectDesc get_pipeline_desc(const EPipelineIds& pipelines_enum);

    namespace pass_resource_descs
    {
        extern zec::render_graph::TextureResourceDesc DEPTH_TARGET;
        extern zec::render_graph::TextureResourceDesc HDR_TARGET;
        extern zec::render_graph::TextureResourceDesc SDR_TARGET;
        extern zec::render_graph::BufferResourceDesc SPOT_LIGHT_INDICES;
        extern zec::render_graph::BufferResourceDesc POINT_LIGHT_INDICES;
    }
}
