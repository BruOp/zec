//#include "light_binning_pass.h"
//
//using namespace zec;
//
//namespace clustered{
//    const render_pass_system::PassOutputDesc LightBinningPass::pass_outputs[] = {
//        {
//            .id = PassResources::COUNT_BUFFER.id,
//                .name = PassResources::COUNT_BUFFER.name,
//                .type = render_pass_system::PassResourceType::BUFFER,
//                .usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
//                .buffer_desc = {
//                    .type = BufferType::RAW,
//                    .byte_size = 4,
//                    .stride = 4,
//            }
//        },
//        {
//            .id = PassResources::LIGHT_INDICES.id,
//            .name = PassResources::LIGHT_INDICES.name,
//            .type = render_pass_system::PassResourceType::BUFFER,
//            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
//            .buffer_desc = {
//                .type = BufferType::RAW,
//                .byte_size = SpotLight::MAX_SPOT_LIGHTS * sizeof(u32) * CLUSTER_SETUP.max_lights_per_bin,
//                .stride = 4,
//                }
//        },
//        {
//            .id = PassResources::CLUSTER_OFFSETS.id,
//            .name = PassResources::CLUSTER_OFFSETS.name,
//            .type = render_pass_system::PassResourceType::TEXTURE,
//            .usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
//            .texture_desc = {
//                .size_class = render_pass_system::SizeClass::ABSOLUTE,
//                .format = BufferFormat::UINT32_2,
//                .width = float(CLUSTER_SETUP.width),
//                .height = float(CLUSTER_SETUP.height),
//                .depth = CLUSTER_SETUP.depth,
//                .num_mips = 1,
//                .array_size = 1,
//                .is_cubemap = false,
//                .is_3d = true,
//            },
//        },
//    };
//
//    void LightBinningPass::setup()
//    {
//        ResourceLayoutDesc resource_layout_desc{
//            .num_constants = 0,
//            .constant_buffers = {
//                {.visibility = ShaderVisibility::COMPUTE },
//                {.visibility = ShaderVisibility::COMPUTE },
//                {.visibility = ShaderVisibility::COMPUTE },
//            },
//            .num_constant_buffers = 3,
//            .tables = {
//                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
//                {.usage = ResourceAccess::WRITE, .count = DESCRIPTOR_TABLE_SIZE },
//                {.usage = ResourceAccess::WRITE, .count = DESCRIPTOR_TABLE_SIZE },
//            },
//            .num_resource_tables = 3,
//            .num_static_samplers = 0,
//        };
//
//        resource_layout = gfx::pipelines::create_resource_layout(resource_layout_desc);
//        PipelineStateObjectDesc pipeline_desc = {
//            .resource_layout = resource_layout,
//            .used_stages = PipelineStage::PIPELINE_STAGE_COMPUTE,
//            .shader_file_path = L"shaders/clustered_forward/light_assignment.hlsl",
//        };
//
//        pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
//
//        binning_cb = gfx::buffers::create({
//            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
//            .type = BufferType::DEFAULT,
//            .byte_size = sizeof(BinningConstants),
//            .stride = 0 });
//    }
//}
