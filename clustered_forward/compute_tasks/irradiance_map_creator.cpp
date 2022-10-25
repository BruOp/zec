#include "irradiance_map_creator.h"
#include <utils/utils.h>

using namespace zec;

namespace clustered
{
    void IrradianceMapCreator::init()

    {
        ASSERT(!is_valid(resource_layout));
        ASSERT(!is_valid(resource_layout));
        // What do we need to set up before we can do the forward pass?
        // Well for one, PSOs
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::COMPUTE, .num_constants = sizeof(IrradiancePassConstants) / 4u}
            },
            .num_constants = 1,
            .num_constant_buffers = 0,
            .tables = {
                {.usage = ResourceAccess::READ },
                {.usage = ResourceAccess::WRITE },
            },
            .num_resource_tables = 2,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                    .wrap_u = SamplerWrapMode::WRAP,
                    .wrap_v = SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
            },
            .num_static_samplers = 1,
        };

        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        ShaderCompilationDesc shader_compilation_desc = {
            .used_stages = PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/env_map_irradiance.hlsl",
        };
        std::string errors{};
        ShaderBlobsHandle blobs = {};
        ZecResult res = gfx::shader_compilation::compile_shaders(shader_compilation_desc, blobs, errors);
        ASSERT_MSG(is_success(res), errors.c_str());

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pso_handle = gfx::pipelines::create_pipeline_state_object(blobs, resource_layout, pipeline_desc, L"Irradiance map creator");

        gfx::shader_compilation::release_blobs(blobs);
    }

    void IrradianceMapCreator::record(zec::CommandContextHandle cmd_ctx, const Settings& settings)
    {
        ASSERT(is_valid(resource_layout));
        ASSERT(is_valid(resource_layout));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso_handle);

        gfx::cmd::bind_compute_resource_table(cmd_ctx, 1);
        gfx::cmd::bind_compute_resource_table(cmd_ctx, 2);
        TextureInfo src_texture_info = gfx::textures::get_texture_info(settings.src_texture);
        TextureInfo out_texture_info = gfx::textures::get_texture_info(settings.out_texture);

        IrradiancePassConstants constants = {
            .src_texture_idx = gfx::textures::get_shader_readable_index(settings.src_texture),
            .out_texture_idx = gfx::textures::get_shader_writable_index(settings.out_texture),
            .src_img_width = src_texture_info.width,
        };
        gfx::cmd::bind_compute_constants(cmd_ctx, &constants, 3, 0);

        const u32 dispatch_size = max(1u, out_texture_info.width / 8u);
        gfx::cmd::dispatch(cmd_ctx, dispatch_size, dispatch_size, 6);
    }
}
