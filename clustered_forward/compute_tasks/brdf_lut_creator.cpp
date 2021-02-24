#include "brdf_lut_creator.h"

using namespace zec;

namespace clustered
{
    void BRDFLutCreator::init()
    {
        ASSERT(!is_valid(resource_layout));
        ASSERT(!is_valid(pso_handle));

        // What do we need to set up before we can do the forward pass?
        // Well for one, PSOs
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::COMPUTE, .num_constants = 1 },
             },
            .num_constants = 1,
            .num_constant_buffers = 0,
            .tables = {
                {.usage = ResourceAccess::WRITE },
            },
            .num_resource_tables = 1,
            .num_static_samplers = 0,
        };

        resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {
            .resource_layout = resource_layout,
            .used_stages = PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/clustered_forward/brdf_lut_creator.hlsl",
        };
        pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void BRDFLutCreator::record(CommandContextHandle cmd_ctx, const Settings& settings)
    {
        ASSERT(is_valid(resource_layout));
        ASSERT(is_valid(resource_layout));

        gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
        gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso_handle);

        u32 out_texture_id = gfx::textures::get_shader_writable_index(settings.out_texture);
        gfx::cmd::bind_compute_constants(cmd_ctx, &out_texture_id, 1, 0);
        gfx::cmd::bind_compute_resource_table(cmd_ctx, 1);

        const u32 lut_width = gfx::textures::get_texture_info(settings.out_texture).width;

        const u32 dispatch_size = lut_width / 8u;
        gfx::cmd::dispatch(cmd_ctx, dispatch_size, dispatch_size, 1);
    }


}
