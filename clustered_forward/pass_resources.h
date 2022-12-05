#pragma once
#include <core/zec_types.h>
#include <utils/crc_hash.h>
#include <gfx/render_task_system.h>
#include "renderable_scene.h"

namespace clustered
{
#define MAKE_IDENTIFIER(_VAR_NAME) \
    constexpr zec::render_graph::ResourceIdentifier _VAR_NAME = { .identifier = ctcrc32(#_VAR_NAME) };

    template<typename Enum>
    constexpr zec::render_graph::ResourceIdentifier to_rid(const Enum value)
    {
        return { static_cast<u32>(value) };
    }

    enum struct ESettingsIds : u32
    {
        MAIN_PASS_VIEW_CB = 0,
        RENDERABLE_SCENE_PTR,
        CLUSTER_GRID_SETUP,
        BACKGROUND_CUBE_MAP,
        FULLSCREEN_QUAD,
        EXPOSURE,

        COUNT
    };

    enum struct EResourceLayoutIds : u32
    {
        DEPTH_PASS_RESOURCE_LAYOUT = 0,
        BACKGROUND_PASS_RESOURCE_LAYOUT,
        FORWARD_PASS_RESOURCE_LAYOUT,
        TONE_MAPPING_PASS_RESOURCE_LAYOUT,
        LIGHT_BINNING_PASS_RESOURCE_LAYOUT,
        CLUSTERED_DEBUG_PASS_RESOURCE_LAYOUT,

        COUNT
    };


    enum struct EShaderIds : u32
    {
        DEPTH_PASS_SHADER = 0,
        BACKGROUND_PASS_SHADER,
        FORWARD_PASS_SHADER,
        TONE_MAPPING_PASS_SHADER,
        LIGHT_BINNING_PASS_SPOT_LIGHT_SHADER,
        LIGHT_BINNING_PASS_POINT_LIGHT_SHADER,
        CLUSTERED_DEBUG_PASS_SHADER,

        COUNT
    };


    enum struct EPipelineIds : u32
    {
        DEPTH_PASS_PIPELINE = 0,
        BACKGROUND_PASS_PIPELINE,
        FORWARD_PASS_PIPELINE,
        TONE_MAPPING_PASS_PIPELINE,
        LIGHT_BINNING_PASS_POINT_LIGHT_PIPELINE,
        LIGHT_BINNING_PASS_SPOT_LIGHT_PIPELINE,
        CLUSTERED_DEBUG_PASS_PIPELINE,

        COUNT,
    };

    enum struct EResourceIds : u32
    {
        DEPTH_TARGET = 0,
        HDR_TARGET,
        SDR_TARGET,
        SPOT_LIGHT_INDICES,
        POINT_LIGHT_INDICES,

        COUNT
    };

    const zec::ResourceLayoutDesc& get_resource_layout_desc(const EResourceLayoutIds layouts_enum);
    const zec::ShaderCompilationDesc& get_shader_compilation_desc(const EShaderIds shaders_enum);
    const zec::PipelineStateObjectDesc& get_pipeline_desc(const EPipelineIds pipelines_enum);
    const zec::render_graph::TextureResourceDesc& get_texture_resource_desc(const EResourceIds resource_enum);
    const zec::render_graph::BufferResourceDesc& get_buffer_resource_desc(const EResourceIds resource_enum);

    namespace pass_resource_descs
    {
        extern zec::render_graph::TextureResourceDesc DEPTH_TARGET;
        extern zec::render_graph::TextureResourceDesc HDR_TARGET;
        extern zec::render_graph::TextureResourceDesc SDR_TARGET;
        extern zec::render_graph::BufferResourceDesc SPOT_LIGHT_INDICES;
        extern zec::render_graph::BufferResourceDesc POINT_LIGHT_INDICES;

    }
}
