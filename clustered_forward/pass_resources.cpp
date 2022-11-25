#include "pass_resources.h"
#include <array>
#include "zec_math.h"

namespace clustered
{
    using namespace zec;
    using namespace zec::render_graph;

    static constexpr u32 NUM_LINES_SUPPORTED = 4096u;

    namespace pass_resource_descs
    {
        zec::render_graph::TextureResourceDesc DEPTH_TARGET = {
            .identifier = pass_resources::DEPTH_TARGET,
            .name = L"depth",
            .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
            .relative_width_factor = 1.0f,
            .relative_height_factor = 1.0f,
            .desc = {
                .num_mips = 1,
                .array_size = 1,
                .is_cubemap = false,
                .is_3d = false,
                .format = zec::BufferFormat::D32,
                .usage = zec::RESOURCE_USAGE_DEPTH_STENCIL,
                .initial_state = zec::RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };
        zec::render_graph::TextureResourceDesc HDR_TARGET = {
            .identifier = pass_resources::HDR_TARGET,
            .name = L"hdr",
            .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
            .relative_width_factor = 1.0f,
            .relative_height_factor = 1.0f,
            .desc = {
                .num_mips = 1,
                .array_size = 1,
                .is_cubemap = false,
                .is_3d = false,
                .format = zec::BufferFormat::R16G16B16A16_FLOAT,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET | zec::RESOURCE_USAGE_SHADER_READABLE,
                .initial_state = zec::RESOURCE_USAGE_RENDER_TARGET,
            },
        };
        zec::render_graph::TextureResourceDesc SDR_TARGET = {
            .identifier = pass_resources::SDR_TARGET,
            .name = L"sdr",
            .sizing = zec::render_graph::Sizing::RELATIVE_TO_SWAP_CHAIN,
            .relative_width_factor = 1.0f,
            .relative_height_factor = 1.0f,
            .desc = {
                .num_mips = 1,
                .array_size = 1,
                .is_cubemap = false,
                .is_3d = false,
                .format = zec::BufferFormat::R8G8B8A8_UNORM_SRGB,
                .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
                .initial_state = zec::RESOURCE_USAGE_RENDER_TARGET,
            },
        };

        zec::render_graph::BufferResourceDesc SPOT_LIGHT_INDICES = {
            .identifier = pass_resources::SPOT_LIGHT_INDICES,
            .name = L"spot light indices",
            .initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
            .desc = {
                .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_SHADER_READABLE,
                .type = zec::BufferType::RAW,
                .byte_size = 0,
                .stride = 4,
            }
        };
        zec::render_graph::BufferResourceDesc POINT_LIGHT_INDICES = {
            .identifier = pass_resources::POINT_LIGHT_INDICES,
            .name = L"point light indices",
            .initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
            .desc = {
                .usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_SHADER_READABLE,
                .type = zec::BufferType::RAW,
                .byte_size = 0,
                .stride = 4,
            }
        };
		zec::render_graph::BufferResourceDesc DEBUG_LINES_POSITIONS = {
			.identifier = pass_resources::DEBUG_LINES_POSITIONS,
			.name = L"point light indices",
			.initial_usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE,
			.desc = {
				.usage = zec::RESOURCE_USAGE_COMPUTE_WRITABLE | zec::RESOURCE_USAGE_VERTEX,
				.type = zec::BufferType::RAW,
                // We'll use the first element to store things like the number of lines we've written so far
				.byte_size = (NUM_LINES_SUPPORTED + 1) * sizeof(vec3) * 2,
				.stride = sizeof(vec3),
			}
		};
    }

    // TODO: Maybe include the enum value so we can validate that we're providing this data correctly
    // TODO: use SM 6.6 and get rid of some of this data
    // see https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
    std::array<ResourceLayoutDesc, EResourceLayoutIds::RESOURCE_LAYOUT_COUNT> resource_layouts = {
        // DEPTH_PASS_RESOURCE_LAYOUT
        ResourceLayoutDesc{
            .constants = {
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
                {.visibility = ShaderVisibility::ALL, .num_constants = 1 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 1,
            .tables = {
                {.usage = ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        },
        // BACKGROUND_PASS_RESOURCE_LAYOUT
        {
            .constants = {
                {.visibility = zec::ShaderVisibility::PIXEL, .num_constants = 2 },
                },
            .num_constants = 1,
            .constant_buffers = {
                { zec::ShaderVisibility::ALL },
            },
            .num_constant_buffers = 1,
            .tables = {
                {.usage = zec::ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .static_samplers = {
                {
                    .filtering = zec::SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                    .wrap_u = zec::SamplerWrapMode::WRAP,
                    .wrap_v = zec::SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
            },
            .num_static_samplers = 1,
        },
        // FORWARD_PASS_RESOUCE_LAYOUT
        {
            .constants = {
                {.visibility = zec::ShaderVisibility::ALL, .num_constants = 2 },
                {.visibility = zec::ShaderVisibility::ALL, .num_constants = 2 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { zec::ShaderVisibility::ALL },
                { zec::ShaderVisibility::PIXEL },
                { zec::ShaderVisibility::PIXEL },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = zec::ResourceAccess::READ },
                {.usage = zec::ResourceAccess::READ },
                {.usage = zec::ResourceAccess::READ },
            },
            .num_resource_tables = 3,
            .static_samplers = {
                {
                    .filtering = zec::SamplerFilterType::ANISOTROPIC,
                    .wrap_u = zec::SamplerWrapMode::WRAP,
                    .wrap_v = zec::SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
                {
                    .filtering = zec::SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_POINT,
                    .wrap_u = zec::SamplerWrapMode::CLAMP,
                    .wrap_v = zec::SamplerWrapMode::CLAMP,
                    .binding_slot = 1,
                },
            },
            .num_static_samplers = 2,
        },
        // TONE_MAPPING_PASS_RESOURCE_LAYOUT
        {
            .constants = {{
                .visibility = zec::ShaderVisibility::PIXEL,
                .num_constants = 2
                }},
            .num_constants = 1,
            .num_constant_buffers = 0,
            .tables = {
                {.usage = zec::ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .static_samplers = {
                {
                    .filtering = zec::SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
                    .wrap_u = zec::SamplerWrapMode::CLAMP,
                    .wrap_v = zec::SamplerWrapMode::CLAMP,
                    .binding_slot = 0,
                },
            },
            .num_static_samplers = 1,
        },
        // LIGHT_BINNING_PASS_RESOURCE_LAYOUT
        {
            .num_constants = 0,
            .constant_buffers = {
                {.visibility = zec::ShaderVisibility::COMPUTE },
                {.visibility = zec::ShaderVisibility::COMPUTE },
                {.visibility = zec::ShaderVisibility::COMPUTE },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = zec::ResourceAccess::READ },
                {.usage = zec::ResourceAccess::WRITE },
            },
            .num_resource_tables = 2,
            .num_static_samplers = 0,
        },
        // CLUSTERED_DEBUG_PASS_RESOURCE_LAYOUT
        {
            .constants = {
                {.visibility = zec::ShaderVisibility::ALL, .num_constants = 2 },
                {.visibility = zec::ShaderVisibility::ALL, .num_constants = 2 },
            },
            .num_constants = 2,
            .constant_buffers = {
                { zec::ShaderVisibility::ALL },
                { zec::ShaderVisibility::ALL },
                { zec::ShaderVisibility::PIXEL },
            },
            .num_constant_buffers = 3,
            .tables = {
                {.usage = zec::ResourceAccess::READ },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        },
        // PREPARE_DEBUG_DATA_RESOURCE_LAYOUT
        {
            .num_constants = 3,
            .tables = {
                { .usage=zec::ResourceAccess::WRITE },
            },
            .num_resource_tables=1,
        }
    };

    std::array<ShaderCompilationDesc, EShaderIds::SHADERS_COUNT> shader_compilation_descs = {
        // DEPTH_PASS_SHADER
        ShaderCompilationDesc{
            .used_stages = zec::PIPELINE_STAGE_VERTEX,
            .shader_file_path = L"shaders/clustered_forward/depth_pass.hlsl",
        },
        // BACKGROUND_PASS_SHADER
        {
                .used_stages = zec::PIPELINE_STAGE_VERTEX | zec::PIPELINE_STAGE_PIXEL,
                .shader_file_path = L"shaders/clustered_forward/background_pass.hlsl",
        },
        // FORWARD_PASS_PIPELINE
        {
            .used_stages = zec::PIPELINE_STAGE_VERTEX | zec::PIPELINE_STAGE_PIXEL,
            .shader_file_path = L"shaders/clustered_forward/mesh_forward.hlsl",
        },
        // TONE_MAPPING_PASS_SHADER
        {
                .used_stages = zec::PIPELINE_STAGE_VERTEX | zec::PIPELINE_STAGE_PIXEL,
                .shader_file_path = L"shaders/clustered_forward/basic_tone_map.hlsl",
        },
        // LIGHT_BINNING_PASS_SPOT_LIGHT_SHADER
        {
            .used_stages = zec::PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/spot_light_assignment.hlsl",
        },
        // LIGHT_BINNING_PASS_SPOT_LIGHT_PIPELINE
        // TODO: These should just use HLSL defines...
        {
            .used_stages = zec::PipelineStage::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/point_light_assignment.hlsl",
        },
        // CLUSTERED_DEBUG_PASS_SHADER
        {
            .used_stages = zec::PIPELINE_STAGE_VERTEX | zec::PIPELINE_STAGE_PIXEL,
            .shader_file_path = L"shaders/clustered_forward/cluster_debug.hlsl",
		},
        // PREPARE_DEBUG_DATA_SHADER
		{
            .used_stages = zec::PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/clear_buffer.hlsl"
        }
    };

    std::array<PipelineStateObjectDesc, EPipelineIds::PIPELINES_COUNT> pipeline_descs = {
        // DEPTH_PASS_PIPELINE
        PipelineStateObjectDesc{
            .input_assembly_desc = { {
                { zec::MESH_ATTRIBUTE_POSITION, 0, zec::BufferFormat::FLOAT_3, 0 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = zec::ComparisonFunc::GREATER,
                .depth_write = true,
            },
            .raster_state_desc = {
                .cull_mode = zec::CullMode::BACK_CCW,
                .flags = zec::DEPTH_CLIP_ENABLED,
            },
            .depth_buffer_format = zec::BufferFormat::D32,
        },
        // BACKGROUND_PASS_PIPELINE
        {
            .input_assembly_desc = { {
                { zec::MESH_ATTRIBUTE_POSITION, 0, zec::BufferFormat::FLOAT_3, 0 },
                { zec::MESH_ATTRIBUTE_TEXCOORD, 0, zec::BufferFormat::FLOAT_2, 1 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = zec::ComparisonFunc::GREATER_OR_EQUAL,
                .depth_write = false,
            },
            .raster_state_desc = {
                .cull_mode = zec::CullMode::NONE,
                .flags = zec::DEPTH_CLIP_ENABLED,
            },
            .rtv_formats = { { pass_resource_descs::HDR_TARGET.desc.format } },
            .depth_buffer_format = zec::BufferFormat::D32,
        },
        // FORWARD_PASS_PIPELINE
         {
            .input_assembly_desc = { {
                { zec::MESH_ATTRIBUTE_POSITION, 0, zec::BufferFormat::FLOAT_3, 0 },
                { zec::MESH_ATTRIBUTE_NORMAL, 0, zec::BufferFormat::FLOAT_3, 1 },
                { zec::MESH_ATTRIBUTE_TEXCOORD, 0, zec::BufferFormat::FLOAT_2, 2 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = zec::ComparisonFunc::EQUAL,
                .depth_write = true,
            },
            .raster_state_desc = {
                .cull_mode = zec::CullMode::BACK_CCW,
                .flags = zec::DEPTH_CLIP_ENABLED,
            },
            .rtv_formats = {{ pass_resource_descs::HDR_TARGET.desc.format }},
            .depth_buffer_format = zec::BufferFormat::D32,
        },
        // TONE_MAPPING_PASS_PIPELINE
        {
            .input_assembly_desc = {{
                { zec::MESH_ATTRIBUTE_POSITION, 0, zec::BufferFormat::FLOAT_3, 0 },
                { zec::MESH_ATTRIBUTE_TEXCOORD, 0, zec::BufferFormat::FLOAT_2, 1 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = zec::ComparisonFunc::OFF,
            },
            .raster_state_desc = {
                .cull_mode = zec::CullMode::NONE,
            },
            .rtv_formats = {{ pass_resource_descs::SDR_TARGET.desc.format }},
        },
        // LIGHT_BINNING_PASS_POINT_LIGHT_PIPELINE
        {},
        // LIGHT_BINNING_SPOT_LIGHT_PASS_PIPELINE
        {},
        // CLUSTERED_DEBUG_PASS_PIPELINE
        {
            .input_assembly_desc = { {
                { zec::MESH_ATTRIBUTE_POSITION, 0, zec::BufferFormat::FLOAT_3, 0 },
            } },
            .depth_stencil_state = {
                .depth_cull_mode = zec::ComparisonFunc::EQUAL,
                .depth_write = true,
            },
            .raster_state_desc = {
                .cull_mode = zec::CullMode::BACK_CCW,
                .flags = zec::DEPTH_CLIP_ENABLED,
            },
            .rtv_formats = { { pass_resource_descs::SDR_TARGET.desc.format } },
            .depth_buffer_format = zec::BufferFormat::D32,
        },
		// PREPARE_DEBUG_DATA_PIPELINE
        {},
    };

    zec::ResourceLayoutDesc get_resource_layout_desc(const EResourceLayoutIds& layouts_enum)
    {
        return resource_layouts.at(layouts_enum);
    }

    zec::ShaderCompilationDesc get_shader_compilation_desc(const EShaderIds& shaders_enum)
    {
        return shader_compilation_descs.at(shaders_enum);
    }

    zec::PipelineStateObjectDesc get_pipeline_desc(const EPipelineIds& pipelines_enum)
    {
        return pipeline_descs.at(pipelines_enum);
    }
}
