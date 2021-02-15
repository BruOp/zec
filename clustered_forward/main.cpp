
#include "app.h"
#include "core/zec_math.h"
#include "utils/crc_hash.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"

#include "bounding_meshes.h"

using namespace zec;

static constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;
static constexpr size_t MAX_NUM_OBJECTS = 16384;

#define PASS_RESOURCE_IDENTIFIER(x) { .id = ctcrc32(x), .name = x }

namespace clustered
{
    struct SpotLight
    {
        static constexpr size_t MAX_SPOT_LIGHTS = 1024;

        vec3 position;
        float radius;
        vec3 direction;
        float umbra_angle;
        float penumbra_angle;
        vec3 color;
    };

    struct ResourceIdentifier
    {
        u64 id;
        const char* name;
    };

    struct TonemapPassConstants
    {
        u32 src_texture;
        float exposure;
    };

    struct IrradiancePassConstants
    {
        u32 src_texture_idx = UINT32_MAX;
        u32 out_texture_idx = UINT32_MAX;
        u32 src_img_width = 0;
    };


    namespace PassResources
    {
        constexpr ResourceIdentifier DEPTH_TARGET = PASS_RESOURCE_IDENTIFIER("depth");
        constexpr ResourceIdentifier HDR_TARGET = PASS_RESOURCE_IDENTIFIER("hdr");
        constexpr ResourceIdentifier SDR_TARGET = PASS_RESOURCE_IDENTIFIER("sdr");
    }

    struct SceneConstantData
    {
        float time = 0.0f;
        u32 radiance_map_idx = UINT32_MAX;
        u32 irradiance_map_idx = UINT32_MAX;
        u32 brdf_lut_idx = UINT32_MAX;
        u32 num_spot_lights = 0;
        u32 spot_light_buffer_idx = UINT32_MAX;
    };

    struct ViewConstantData
    {
        mat4 VP;
        mat4 invVP;
        vec3 camera_position;
        float padding;
    };

    struct VertexShaderData
    {
        mat4 model_transform = {};
        mat3 normal_transform = {};
    };

    static_assert(sizeof(VertexShaderData) % 16 == 0);

    struct MaterialData
    {
        vec4 base_color_factor;
        vec3 emissive_factor;
        float metallic_factor;
        float roughness_factor;
        u32 base_color_texture_idx;
        u32 metallic_roughness_texture_idx;
        u32 normal_texture_idx;
        u32 occlusion_texture_idx;
        u32 emissive_texture_idx;
        float padding[18];
    };

    static_assert(sizeof(MaterialData) == 128);

    struct SceneBuffers
    {
        BufferHandle scene_constants = {};
        BufferHandle spot_lights_buffer = {};
    };

    class Renderables
    {
    public:

        // Owning
        size_t num_entities;
        Array<u32> material_indices;
        Array<MeshHandle> meshes;
        Array<VertexShaderData> vertex_shader_data;

        u32 material_buffer_idx;
        BufferHandle vs_buffer = {};

        void init(const BufferHandle materials_buffer, const u32 max_num_entities)
        {
            material_buffer_idx = gfx::buffers::get_shader_readable_index(materials_buffer);

            BufferDesc vs_buffer_desc{
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::RAW,
                .byte_size = sizeof(VertexShaderData) * max_num_entities,
                .stride = 4,
            };
            vs_buffer = gfx::buffers::create(vs_buffer_desc);
            gfx::set_debug_name(vs_buffer, L"Vertex Shader Constants Buffer");

        }

        void push_renderable(const u32 material_idx, const MeshHandle mesh_handle, const VertexShaderData& vs_data)
        {
            num_entities++;
            material_indices.push_back(material_idx);
            meshes.push_back(mesh_handle);
            vertex_shader_data.push_back(vs_data);
            ASSERT(meshes.size == vertex_shader_data.size && meshes.size == num_entities);
        }
    };

    class ForwardPass : public render_pass_system::IRenderPass
    {
    public:
        Renderables* scene_renderables = nullptr;
        SceneBuffers* scene_buffers = nullptr;
        BufferHandle view_cb_handle = {};

        virtual void setup() final
        {
            ResourceLayoutDesc layout_desc{
                .constants = {
                    {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
                    {.visibility = ShaderVisibility::ALL, .num_constants = 2 },
                },
                .num_constants = 2,
                .constant_buffers = {
                    { ShaderVisibility::ALL },
                    { ShaderVisibility::PIXEL },
                },
                .num_constant_buffers = 2,
                .tables = {
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                },
                .num_resource_tables = 3,
                .static_samplers = {
                    {
                        .filtering = SamplerFilterType::ANISOTROPIC,
                        .wrap_u = SamplerWrapMode::WRAP,
                        .wrap_v = SamplerWrapMode::WRAP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };

            resource_layout = gfx::pipelines::create_resource_layout(layout_desc);
            gfx::set_debug_name(resource_layout, L"Forward Pass Layout");

            // Create the Pipeline State Object
            PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
            } };
            pipeline_desc.shader_file_path = L"shaders/clustered_forward/mesh_forward.hlsl";
            pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
            pipeline_desc.depth_buffer_format = BufferFormat::D32;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
            pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::GREATER;
            pipeline_desc.depth_stencil_state.depth_write = TRUE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
            gfx::set_debug_name(pso, L"Forward Pass Pipeline");
        };

        void copy() override final
        {
            // Shrug
        };

        void record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx) override final
        {
            constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

            TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.id);
            TextureHandle render_target = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
            const TextureInfo& texture_info = gfx::textures::get_texture_info(render_target);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            gfx::cmd::graphics::set_active_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::graphics::set_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            gfx::cmd::clear_render_target(cmd_ctx, render_target, clear_color);
            gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 0.0f, 0);
            gfx::cmd::set_render_targets(cmd_ctx, &render_target, 1, depth_target);

            gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
            gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, scene_buffers->scene_constants, u32(BindingSlots::SCENE_CONSTANT_BUFFER));

            const auto* scene_data = scene_renderables;
            gfx::cmd::graphics::bind_resource_table(cmd_ctx, u32(BindingSlots::RAW_BUFFERS_TABLE));
            gfx::cmd::graphics::bind_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_2D_TABLE));
            gfx::cmd::graphics::bind_resource_table(cmd_ctx, u32(BindingSlots::TEXTURE_CUBE_TABLE));

            u32 buffer_descriptors[2] = {
                gfx::buffers::get_shader_readable_index(scene_data->vs_buffer),
                scene_data->material_buffer_idx,
            };
            gfx::cmd::graphics::bind_constants(cmd_ctx, &buffer_descriptors, 2, u32(BindingSlots::BUFFERS_DESCRIPTORS));

            for (u32 i = 0; i < scene_data->num_entities; i++) {
                u32 per_draw_indices[] = {
                    i,
                    scene_data->material_indices[i]
                };
                gfx::cmd::graphics::bind_constants(cmd_ctx, &per_draw_indices, 2, u32(BindingSlots::PER_INSTANCE_CONSTANTS));
                gfx::cmd::graphics::draw_mesh(cmd_ctx, scene_data->meshes[i]);
            }
        };

        void shutdown() override final
        {
            // TODO: Free pipeline and resource layouts
        };

        const char* get_name() const override final
        {
            return pass_name;
        }

        const InputList get_input_list() const override final
        {
            return {
                .ptr = nullptr,
                .count = 0
            };
        };

        const OutputList get_output_list() const override final
        {
            return {
                .ptr = pass_outputs,
                .count = ARRAY_SIZE(pass_outputs),
            };
        }

        const CommandQueueType get_queue_type() const override final
        {
            return command_queue_type;
        }

        static constexpr CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "ForwardPass";
        static constexpr render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .name = PassResources::HDR_TARGET.name,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = BufferFormat::R16G16B16A16_FLOAT}
            },
            {
                .id = PassResources::DEPTH_TARGET.id,
                .name = PassResources::DEPTH_TARGET.name,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                .texture_desc = {.format = BufferFormat::D32 }
            }
        };

    private:
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso = {};

        enum struct BindingSlots : u32
        {
            PER_INSTANCE_CONSTANTS = 0,
            BUFFERS_DESCRIPTORS,
            VIEW_CONSTANT_BUFFER,
            SCENE_CONSTANT_BUFFER,
            RAW_BUFFERS_TABLE,
            TEXTURE_2D_TABLE,
            TEXTURE_CUBE_TABLE,
        };
    };


    class BackgroundPass : public render_pass_system::IRenderPass
    {
    public:
        BufferHandle view_cb_handle = {};
        SceneBuffers scene_buffers = {};
        float mip_level = 1.0f;

        void setup() override final
        {
            // Create fullscreen quad
            MeshDesc fullscreen_desc{
                .index_buffer_desc = {
                    .usage = RESOURCE_USAGE_INDEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(geometry::k_fullscreen_indices),
                    .stride = sizeof(geometry::k_fullscreen_indices[0]),
                },
                .vertex_buffer_descs = {
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(geometry::k_fullscreen_positions),
                        .stride = 3 * sizeof(geometry::k_fullscreen_positions[0]),
                    },
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(geometry::k_fullscreen_uvs),
                        .stride = 2 * sizeof(geometry::k_fullscreen_uvs[0]),
                    },
                },
                .index_buffer_data = geometry::k_fullscreen_indices,
                .vertex_buffer_data = {
                    geometry::k_fullscreen_positions,
                    geometry::k_fullscreen_uvs
                }
            };
            CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);
            fullscreen_mesh = gfx::meshes::create(copy_ctx, fullscreen_desc);
            gfx::cmd::cpu_wait(gfx::cmd::return_and_execute(&copy_ctx, 1));

            ResourceLayoutDesc layout_desc{
                .constants = {
                    {.visibility = ShaderVisibility::PIXEL, .num_constants = 1 },
                 },
                .num_constants = 1,
                .constant_buffers = {
                    { ShaderVisibility::ALL },
                    { ShaderVisibility::PIXEL },
                },
                .num_constant_buffers = 2,
                .tables = {
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                },
                .num_resource_tables = 1,
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

            // Create the Pipeline State Object
            PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
            } };
            pipeline_desc.shader_file_path = L"shaders/clustered_forward/background_pass.hlsl";
            pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
            pipeline_desc.depth_buffer_format = BufferFormat::D32;
            pipeline_desc.resource_layout = resource_layout;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
            pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::GREATER_OR_EQUAL;
            pipeline_desc.depth_stencil_state.depth_write = FALSE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        }

        void copy() override final { };

        void record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx) override final
        {
            TextureHandle depth_target = resource_map.get_texture_resource(PassResources::DEPTH_TARGET.id);
            TextureHandle hdr_buffer = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
            const TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            gfx::cmd::graphics::set_active_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::graphics::set_pipeline_state(cmd_ctx, pso_handle);
            gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            gfx::cmd::graphics::bind_constants(cmd_ctx, &mip_level, 1, u32(BindingSlots::MIP_LEVEL));
            gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, view_cb_handle, u32(BindingSlots::VIEW_CONSTANT_BUFFER));
            gfx::cmd::graphics::bind_constant_buffer(cmd_ctx, scene_buffers.scene_constants, u32(BindingSlots::SCENE_CONSTANTS_BUFFER));
            gfx::cmd::graphics::bind_resource_table(cmd_ctx, u32(BindingSlots::RESOURCE_TABLE));

            gfx::cmd::graphics::draw_mesh(cmd_ctx, fullscreen_mesh);
        };

        void shutdown() override final
        {
            // TODO
        };

        const char* get_name() const override final { 
            return pass_name;
        }

        const CommandQueueType get_queue_type() const override final
        {
            return command_queue_type;
        }

        const InputList get_input_list() const override final
        {
            return {
                .ptr = pass_inputs,
                .count = ARRAY_SIZE(pass_inputs)
            };
        };

        const OutputList get_output_list() const override final
        {
            return {
                .ptr = pass_outputs,
                .count = ARRAY_SIZE(pass_outputs),
            };
        }


    private:
        enum struct BindingSlots : u32
        {
            MIP_LEVEL = 0,
            VIEW_CONSTANT_BUFFER,
            SCENE_CONSTANTS_BUFFER,
            RESOURCE_TABLE
        };

        MeshHandle fullscreen_mesh = {};
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};

        static constexpr CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "BackgroundPass";
        static constexpr render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::DEPTH_TARGET.id,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            },
        };
        static constexpr render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .name = PassResources::HDR_TARGET.name,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = BufferFormat::R16G16B16A16_FLOAT}
            },
        };
    };

    class ToneMappingPass : public render_pass_system::IRenderPass
    {
    public:
        // Settings
        float exposure = 1.0f;

        void setup() override final
        {
            // Create fullscreen quad
            MeshDesc fullscreen_desc{
                .index_buffer_desc = {
                    .usage = RESOURCE_USAGE_INDEX,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(geometry::k_fullscreen_indices),
                    .stride = sizeof(geometry::k_fullscreen_indices[0]),
                },
                .vertex_buffer_descs = {
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(geometry::k_fullscreen_positions),
                        .stride = 3 * sizeof(geometry::k_fullscreen_positions[0]),
                    },
                    {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = sizeof(geometry::k_fullscreen_uvs),
                        .stride = 2 * sizeof(geometry::k_fullscreen_uvs[0]),
                    },
                },
                .index_buffer_data = geometry::k_fullscreen_indices,
                .vertex_buffer_data = {
                    geometry::k_fullscreen_positions,
                    geometry::k_fullscreen_uvs
                }
            };
            CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);
            fullscreen_mesh = gfx::meshes::create(copy_ctx, fullscreen_desc);
            gfx::cmd::cpu_wait(gfx::cmd::return_and_execute(&copy_ctx, 1));

            ResourceLayoutDesc tonemapping_layout_desc{
                .constants = {{
                    .visibility = ShaderVisibility::PIXEL,
                    .num_constants = 2
                 }},
                .num_constants = 1,
                .num_constant_buffers = 0,
                .tables = {
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                },
                .num_resource_tables = 1,
                .static_samplers = {
                    {
                        .filtering = SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT,
                        .wrap_u = SamplerWrapMode::CLAMP,
                        .wrap_v = SamplerWrapMode::CLAMP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };

            resource_layout = gfx::pipelines::create_resource_layout(tonemapping_layout_desc);
            PipelineStateObjectDesc pipeline_desc = {
                 .resource_layout = resource_layout,
                .input_assembly_desc = {{
                    { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                    { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
                 } },
                 .rtv_formats = {{ BufferFormat::R8G8B8A8_UNORM_SRGB }},
                 .shader_file_path = L"shaders/clustered_forward/basic_tone_map.hlsl",
            };
            pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::OFF;
            pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
            pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

            pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        }

        void copy() override final { };

        void record(const render_pass_system::ResourceMap& resource_map, CommandContextHandle cmd_ctx) override final
        {
            TextureHandle hdr_buffer = resource_map.get_texture_resource(PassResources::HDR_TARGET.id);
            TextureHandle sdr_buffer = resource_map.get_texture_resource(PassResources::SDR_TARGET.id);

            gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

            const TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
            Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
            Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

            gfx::cmd::graphics::set_active_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::graphics::set_pipeline_state(cmd_ctx, pso);
            gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
            gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

            TonemapPassConstants tonemapping_constants = {
                .src_texture = gfx::textures::get_shader_readable_index(hdr_buffer),
                .exposure = exposure
            };
            gfx::cmd::graphics::bind_constants(cmd_ctx, &tonemapping_constants, 2, 0);
            gfx::cmd::graphics::bind_resource_table(cmd_ctx, 1);

            gfx::cmd::graphics::draw_mesh(cmd_ctx, fullscreen_mesh);
        };

        // TODO!
        void shutdown() override final { }


        const char* get_name() const override final
        {
            return pass_name;
        }

        const CommandQueueType get_queue_type() const override final
        {
            return command_queue_type;
        }

        const InputList get_input_list() const override final
        {
            return {
                .ptr = pass_inputs,
                .count = ARRAY_SIZE(pass_inputs)
            };
        };

        const OutputList get_output_list() const override final
        {
            return {
                .ptr = pass_outputs,
                .count = ARRAY_SIZE(pass_outputs),
            };
        }


    private:
        
        MeshHandle fullscreen_mesh;
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso = {};

        static constexpr CommandQueueType command_queue_type = CommandQueueType::GRAPHICS;
        static constexpr char pass_name[] = "Tone Mapping Pass";
        static constexpr render_pass_system::PassInputDesc pass_inputs[] = {
            {
                .id = PassResources::HDR_TARGET.id,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_SHADER_READABLE,
            },
        };
        static constexpr render_pass_system::PassOutputDesc pass_outputs[] = {
            {
                .id = PassResources::SDR_TARGET.id,
                .name = PassResources::SDR_TARGET.name,
                .type = render_pass_system::PassResourceType::TEXTURE,
                .usage = RESOURCE_USAGE_RENDER_TARGET,
                .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB}
            },
        };
    };

    class BRDFLutCreator
    {
    public:
        struct Settings
        {
            TextureHandle out_texture = {};
        };

        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};

        void init()
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
                    {.usage = ResourceAccess::WRITE, .count = DESCRIPTOR_TABLE_SIZE },
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

        void record(CommandContextHandle cmd_ctx, const Settings& settings)
        {
            ASSERT(is_valid(resource_layout));
            ASSERT(is_valid(resource_layout));

            gfx::cmd::compute::set_active_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::compute::set_pipeline_state(cmd_ctx, pso_handle);

            u32 out_texture_id = gfx::textures::get_shader_writable_index(settings.out_texture);
            gfx::cmd::compute::bind_constants(cmd_ctx, &out_texture_id, 1, 0);
            gfx::cmd::compute::bind_resource_table(cmd_ctx, 1);

            const u32 lut_width = gfx::textures::get_texture_info(settings.out_texture).width;

            const u32 dispatch_size = lut_width / 8u;
            gfx::cmd::compute::dispatch(cmd_ctx, dispatch_size, dispatch_size, 1);
        }
    };

    class IrradianceMapCreator
    {
    public:
        struct Settings
        {
            TextureHandle src_texture = {};
            TextureHandle out_texture = {};
        };

        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};

        void init()
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
                    {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                    {.usage = ResourceAccess::WRITE, .count = DESCRIPTOR_TABLE_SIZE },
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

            // Create the Pipeline State Object
            PipelineStateObjectDesc pipeline_desc = {
                .resource_layout = resource_layout,
                .used_stages = PIPELINE_STAGE_COMPUTE,
                .shader_file_path = L"shaders/clustered_forward/env_map_irradiance.hlsl",
            };
            pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        }

        void record(CommandContextHandle cmd_ctx, const Settings& settings)
        {
            ASSERT(is_valid(resource_layout));
            ASSERT(is_valid(resource_layout));

            gfx::cmd::compute::set_active_resource_layout(cmd_ctx, resource_layout);
            gfx::cmd::compute::set_pipeline_state(cmd_ctx, pso_handle);

            gfx::cmd::compute::bind_resource_table(cmd_ctx, 1);
            gfx::cmd::compute::bind_resource_table(cmd_ctx, 2);
            TextureInfo src_texture_info = gfx::textures::get_texture_info(settings.src_texture);
            TextureInfo out_texture_info = gfx::textures::get_texture_info(settings.out_texture);

            IrradiancePassConstants constants = {
                .src_texture_idx = gfx::textures::get_shader_readable_index(settings.src_texture),
                .out_texture_idx = gfx::textures::get_shader_writable_index(settings.out_texture),
                .src_img_width = src_texture_info.width,
            };
            gfx::cmd::compute::bind_constants(cmd_ctx, &constants, 3, 0);

            const u32 dispatch_size = max(1u, out_texture_info.width / 8u);
            gfx::cmd::compute::dispatch(cmd_ctx, dispatch_size, dispatch_size, 6);
        }
    };

    class ClusteredForward : public zec::App
    {
        struct Passes
        {
        public:
            ForwardPass forward = {};
            BackgroundPass background = {};
            ToneMappingPass tone_mapping = {};
        };
    public:
        ClusteredForward() : App{ L"Clustered Forward Rendering" } { }

        FPSCameraController camera_controller = {};

        MeshHandle fullscreen_mesh;

        PerspectiveCamera camera;
        PerspectiveCamera debug_camera;

        // Scene data
        Array<MaterialData> material_instances;
        Array<SpotLight> spot_lights;

        // Rendering Data
        Renderables scene_renderables = {};
        SceneBuffers scene_buffers = {};
        SceneConstantData scene_constant_data = {};

        BufferHandle view_cb_handle = {};
        // GPU side copy of the material instances data
        BufferHandle materials_buffer = {};

        Passes render_passes{ };
        render_pass_system::RenderPassList render_list;


    protected:
        void init() override final
        {
            float vertical_fov = deg_to_rad(65.0f);
            float camera_near = 0.1f;
            float camera_far = 100.0f;

            camera = create_camera(float(width) / float(height), vertical_fov, camera_near, camera_far, CAMERA_CREATION_FLAG_REVERSE_Z);
            camera.position = vec3{ 0.0f, 0.0f, -2.0f };

            camera_controller.init();
            camera_controller.set_camera(&camera);

            // Create lights
            {
                constexpr vec3 light_colors[] = {
                    {1.0f, 0.0f, 0.0f },
                    {0.0f, 1.0f, 0.0f },
                    {0.0f, 0.0f, 1.0f },
                    {1.0f, 1.0f, 0.0f },
                    {1.0f, 0.0f, 1.0f },
                    {0.0f, 1.0f, 1.0f },
                    {1.0f, 1.0f, 1.0f },
                    {0.5f, 0.3f, 1.0f },
                };

                constexpr size_t num_lights = 8;
                for (size_t i = 0; i < num_lights; i++) {
                    // place the lights in a ring around the origin
                    float ring_radius = 3.0f;
                    vec3 position = vec3{ cosf(k_2_pi * float(i) / num_lights), 1.0f, sinf(k_2_pi * float(i) / num_lights) };
                    position = ring_radius * position;
                    spot_lights.push_back({
                        .position = position,
                        .radius = 15.0f,
                        .direction = normalize(-1.0f * position),
                        .umbra_angle = k_half_pi * 0.5f,
                        .penumbra_angle = k_half_pi * 0.4f,
                        .color = light_colors[i] * 5.0f,
                        });
                }
            }

            view_cb_handle = gfx::buffers::create({
                .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::DEFAULT,
                .byte_size = sizeof(ViewConstantData),
                .stride = 0 });
            gfx::set_debug_name(view_cb_handle, L"View Constant Buffer");

            // Scene buffers
            {
                scene_buffers.scene_constants = gfx::buffers::create({
                    .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
                    .type = BufferType::DEFAULT,
                    .byte_size = sizeof(SceneConstantData), // will get bumped up to 256
                    .stride = 0 });
                gfx::set_debug_name(scene_buffers.scene_constants, L"Scene Constants Buffers");

                scene_buffers.spot_lights_buffer = gfx::buffers::create({
                    .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
                    .type = BufferType::RAW,
                    .byte_size = sizeof(SpotLight) * SpotLight::MAX_SPOT_LIGHTS,
                    .stride = 4 });
                gfx::set_debug_name(scene_buffers.spot_lights_buffer, L"Spot Lights Buffer");
            }

            CommandContextHandle copy_ctx = gfx::cmd::provision(CommandQueueType::COPY);

            TextureDesc brdf_lut_desc{
                .width = 256,
                .height = 256,
                .depth = 1,
                .num_mips = 1,
                .array_size = 1,
                .is_cubemap = 0,
                .is_3d = 0,
                .format = BufferFormat::FLOAT_2,
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
                .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
            };
            TextureHandle brdf_lut_map = gfx::textures::create(brdf_lut_desc);
            scene_constant_data.brdf_lut_idx = gfx::textures::get_shader_readable_index(brdf_lut_map);
            gfx::set_debug_name(brdf_lut_map, L"BRDF LUT");

            TextureDesc irradiance_desc{
                .width = 16,
                .height = 16,
                .depth = 1,
                .num_mips = 1,
                .array_size = 6,
                .is_cubemap = 1,
                .is_3d = 0,
                .format = BufferFormat::R16G16B16A16_FLOAT,
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
                .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
            };
            TextureHandle irradiance_map = gfx::textures::create(irradiance_desc);
            gfx::set_debug_name(irradiance_map, L"Irradiance Map");

            TextureHandle radiance_map = gfx::textures::create_from_file(copy_ctx, "textures/prefiltered_paper_mill.dds");
            scene_constant_data.irradiance_map_idx = gfx::textures::get_shader_readable_index(irradiance_map);
            scene_constant_data.radiance_map_idx = gfx::textures::get_shader_readable_index(radiance_map);

            gltf::Context gltf_context{};
            gltf::load_gltf_file("models/flight_helmet/FlightHelmet.gltf", copy_ctx, gltf_context, gltf::GLTF_LOADING_FLAG_NONE);
            CmdReceipt receipt = gfx::cmd::return_and_execute(&copy_ctx, 1);

            gfx::cmd::gpu_wait(CommandQueueType::GRAPHICS, receipt);

            CommandContextHandle graphics_ctx = gfx::cmd::provision(CommandQueueType::GRAPHICS);

            // Compute BRDF and Irraadiance maps
            {
                BRDFLutCreator brdf_lut_creator{};
                brdf_lut_creator.init();
                brdf_lut_creator.record(graphics_ctx, { .out_texture = brdf_lut_map });

                IrradianceMapCreator irradiance_map_creator{};
                irradiance_map_creator.init();
                irradiance_map_creator.record(graphics_ctx, { .src_texture = radiance_map, .out_texture = irradiance_map });

                ResourceTransitionDesc transition_descs[] = {
                    {
                        .type = ResourceTransitionType::TEXTURE,
                        .texture = irradiance_map,
                        .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                        .after = RESOURCE_USAGE_SHADER_READABLE,
                    },
                    {
                        .type = ResourceTransitionType::TEXTURE,
                        .texture = brdf_lut_map,
                        .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                        .after = RESOURCE_USAGE_SHADER_READABLE,
                    },
                };
                gfx::cmd::transition_resources(graphics_ctx, transition_descs, ARRAY_SIZE(transition_descs));
            }

            receipt = gfx::cmd::return_and_execute(&graphics_ctx, 1);

            // Materials buffer
            material_instances.reserve(gltf_context.materials.size);
            for (const auto& material : gltf_context.materials) {
                material_instances.push_back({
                    .base_color_factor = material.base_color_factor,
                    .emissive_factor = material.emissive_factor,
                    .metallic_factor = material.metallic_factor,
                    .roughness_factor = material.roughness_factor,
                    .base_color_texture_idx = material.base_color_texture_idx,
                    .metallic_roughness_texture_idx = material.metallic_roughness_texture_idx,
                    .normal_texture_idx = material.normal_texture_idx,
                    .occlusion_texture_idx = material.occlusion_texture_idx,
                    .emissive_texture_idx = material.emissive_texture_idx,
                    });
            }

            materials_buffer = gfx::buffers::create({
                .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_DYNAMIC,
                .type = BufferType::RAW,
                .byte_size = u32(material_instances.get_byte_size()),
                .stride = 4 });
            gfx::set_debug_name(materials_buffer, L"Materials Buffer");
            gfx::buffers::set_data(materials_buffer, material_instances.data, material_instances.get_byte_size());

            for (size_t i = 0; i < gltf_context.draw_calls.size; i++) {
                const auto& draw_call = gltf_context.draw_calls[i];
                scene_renderables.push_renderable(
                    draw_call.material_index,
                    draw_call.mesh,
                    {
                        .model_transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx],
                        .normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx],
                    });
            }

            scene_renderables.init(materials_buffer, scene_renderables.num_entities);

            render_passes.forward.view_cb_handle = view_cb_handle;
            render_passes.forward.scene_renderables = &scene_renderables;
            render_passes.forward.scene_buffers = &scene_buffers;

            render_passes.background.view_cb_handle = view_cb_handle;
            render_passes.background.scene_buffers = scene_buffers;

            render_pass_system::IRenderPass* pass_ptrs[] = {
                &render_passes.forward,
                &render_passes.background,
                &render_passes.tone_mapping,
            };
            render_pass_system::RenderPassListDesc render_list_desc = {
                .render_passes = pass_ptrs,
                .num_render_passes = ARRAY_SIZE(pass_ptrs),
                .resource_to_use_as_backbuffer = PassResources::SDR_TARGET.id,
            };

            render_list.compile(render_list_desc);
            render_list.setup();

            gfx::cmd::cpu_wait(receipt);

        }

        void shutdown() override final
        {
            render_list.shutdown();
        }

        void update(const zec::TimeData& time_data) override final
        {
            camera_controller.update(time_data.delta_seconds_f);
        }

        void copy() override final
        {
            ViewConstantData view_constant_data{
                .VP = camera.projection * camera.view,
                .invVP = invert(camera.projection * mat4(to_mat3(camera.view), {})),
                .camera_position = camera.position,
            };
            gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));

            scene_constant_data.num_spot_lights = u32(spot_lights.size);
            scene_constant_data.spot_light_buffer_idx = gfx::buffers::get_shader_readable_index(scene_buffers.spot_lights_buffer);

            gfx::buffers::update(
                scene_buffers.scene_constants,
                &scene_constant_data,
                sizeof(scene_constant_data));

            gfx::buffers::update(
                scene_buffers.spot_lights_buffer,
                spot_lights.data,
                spot_lights.size * sizeof(SpotLight));

            gfx::buffers::update(
                scene_renderables.vs_buffer,
                scene_renderables.vertex_shader_data.data,
                scene_renderables.vertex_shader_data.size * sizeof(VertexShaderData));

            render_list.copy();
        }

        void render() override final
        {
            render_list.execute();
        }

        void before_reset() override final
        { }

        void after_reset() override final
        { }
    };

}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef DEBUG
    int res = 0;
    _CrtMemState s1, s2, s3;
    _CrtMemCheckpoint(&s1);
    {
        clustered::ClusteredForward  app{};
        res = app.run();
    }
    _CrtMemCheckpoint(&s2);


    if (_CrtMemDifference(&s3, &s1, &s2))
        _CrtMemDumpStatistics(&s3);
    _CrtDumpMemoryLeaks();
    return res;
#else
    clustered::ClusteredForward  app{};
    return app.run();
#endif // DEBUG
}