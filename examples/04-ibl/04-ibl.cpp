#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"
#include "camera.h"
#include "gltf_loading.h"
#include "gfx/render_system.h"

using namespace zec;

constexpr u32 DESCRIPTOR_TABLE_SIZE = 4096;

namespace ResourceNames
{
    constexpr char DEPTH_TARGET[] = "depth";
    constexpr char HDR_BUFFER[] = "hdr";
    constexpr char SDR_BUFFER[] = "sdr";
}

struct ViewConstantData
{
    mat4 VP;
    mat4 invVP;
    vec3 camera_position;
    u32 radiance_map_idx;
    u32 irradiance_map_idx;
    u32 brdf_LUT;
    float num_env_levels;
    float time;
    float padding[24];
};

static_assert(sizeof(ViewConstantData) == 256);

struct DrawConstantData
{
    mat4 model = {};
    mat3 normal_transform = {};
    gltf::MaterialData material_data = {};
    float padding[22] = {};
};

static_assert(sizeof(DrawConstantData) == 256);

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

namespace ForwardPass
{
    struct Context
    {
        // Not owned
        BufferHandle view_cb_handle = {};
        Array<BufferHandle>* draw_data_buffer_handles = nullptr;
        gltf::Context* scene;

        // Owned
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };

    void setup(void* context)
    {
        Context* forward_context = reinterpret_cast<Context*>(context);

        // What do we need to set up before we can do the forward pass?
        // Well for one, PSOs
        ResourceLayoutDesc layout_desc{
            .num_constants = 0,
            .constant_buffers = {
                { ShaderVisibility::ALL },
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 2,
            .tables = {
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
                {.usage = ResourceAccess::READ, .count = DESCRIPTOR_TABLE_SIZE },
            },
            .num_resource_tables = 2,
            .static_samplers = {
                {
                    .filtering = SamplerFilterType::ANISOTROPIC,
                    .wrap_u = SamplerWrapMode::WRAP,
                    .wrap_v = SamplerWrapMode::WRAP,
                    .binding_slot = 0,
                },
                {
                    .filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
                    .wrap_u = SamplerWrapMode::CLAMP,
                    .wrap_v = SamplerWrapMode::CLAMP,
                    .binding_slot = 1,
                },
            },
            .num_static_samplers = 2,
        };

        forward_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
            { MESH_ATTRIBUTE_NORMAL, 0, BufferFormat::FLOAT_3, 1 },
            { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 2 },
        } };
        pipeline_desc.shader_file_path = L"shaders/04-ibl/gltf_shader.hlsl";
        pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = forward_context->resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::BACK_CCW;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS;
        pipeline_desc.depth_stencil_state.depth_write = TRUE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        forward_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
        gfx::pipelines::set_debug_name(forward_context->pso_handle, L"Forward Rendering Pipeline");
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        Context* forward_context = reinterpret_cast<Context*>(context);

        TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle hdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::HDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, forward_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, forward_context->pso_handle);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::clear_render_target(cmd_ctx, hdr_buffer, clear_color);
        gfx::cmd::clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);
        gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);

        gfx::cmd::bind_constant_buffer(cmd_ctx, forward_context->view_cb_handle, 1);
        gfx::cmd::bind_resource_table(cmd_ctx, 2);
        gfx::cmd::bind_resource_table(cmd_ctx, 3);

        for (size_t i = 0; i < forward_context->scene->draw_calls.size; i++) {
            gfx::cmd::bind_constant_buffer(cmd_ctx, (*forward_context->draw_data_buffer_handles)[i], 0);
            gfx::cmd::draw_mesh(cmd_ctx, forward_context->scene->draw_calls[i].mesh);
        }
    };

    void destroy(void* context)
    {

    }
}

namespace BackgroundPass
{
    struct Context
    {
        // Not owned
        BufferHandle view_cb_handle = {};
        MeshHandle fullscreen_quad = {};
        // Owned
        float mip_level = 1.0f;
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };

    void setup(void* context)
    {
        Context* background_context = reinterpret_cast<Context*>(context);

        // What do we need to set up before we can do the forward pass?
        // Well for one, PSOs
        ResourceLayoutDesc layout_desc{
            .constants = {
                {.visibility = ShaderVisibility::PIXEL, .num_constants = 1 },
             },
            .num_constants = 1,
            .constant_buffers = {
                { ShaderVisibility::ALL },
            },
            .num_constant_buffers = 1,
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

        background_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {};
        pipeline_desc.input_assembly_desc = { {
            { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
            { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
        } };
        pipeline_desc.shader_file_path = L"shaders/04-ibl/background_pass.hlsl";
        pipeline_desc.rtv_formats[0] = BufferFormat::R16G16B16A16_FLOAT;
        pipeline_desc.depth_buffer_format = BufferFormat::D32;
        pipeline_desc.resource_layout = background_context->resource_layout;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        pipeline_desc.raster_state_desc.flags |= DEPTH_CLIP_ENABLED;
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::LESS_OR_EQUAL;
        pipeline_desc.depth_stencil_state.depth_write = FALSE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        background_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        Context* background_context = reinterpret_cast<Context*>(context);

        TextureHandle depth_target = RenderSystem::get_texture_resource(render_list, ResourceNames::DEPTH_TARGET);
        TextureHandle hdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::HDR_BUFFER);
        TextureInfo& texture_info = gfx::textures::get_texture_info(hdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, background_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, background_context->pso_handle);
        gfx::cmd::set_render_targets(cmd_ctx, &hdr_buffer, 1, depth_target);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        gfx::cmd::bind_constant(cmd_ctx, &background_context->mip_level, 1, 0);
        gfx::cmd::bind_constant_buffer(cmd_ctx, background_context->view_cb_handle, 1);
        gfx::cmd::bind_resource_table(cmd_ctx, 2);

        gfx::cmd::draw_mesh(cmd_ctx, background_context->fullscreen_quad);
    };

    void destroy(void* context)
    {

    }
}

namespace ToneMapping
{
    struct Context
    {
        // Non-owning
        float exposure = 1.0f;
        MeshHandle fullscreen_mesh;

        // Owning
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso = {};
    };

    void setup(void* context)
    {
        Context* tonemapping_context = reinterpret_cast<Context*>(context);
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

        tonemapping_context->resource_layout = gfx::pipelines::create_resource_layout(tonemapping_layout_desc);
        PipelineStateObjectDesc pipeline_desc = {
             .resource_layout = tonemapping_context->resource_layout,
            .input_assembly_desc = {{
                { MESH_ATTRIBUTE_POSITION, 0, BufferFormat::FLOAT_3, 0 },
                { MESH_ATTRIBUTE_TEXCOORD, 0, BufferFormat::FLOAT_2, 1 },
             } },
             .rtv_formats = {{ BufferFormat::R8G8B8A8_UNORM_SRGB }},
             .shader_file_path = L"shaders/04-ibl/basic_tone_map.hlsl",
        };
        pipeline_desc.depth_stencil_state.depth_cull_mode = ComparisonFunc::OFF;
        pipeline_desc.raster_state_desc.cull_mode = CullMode::NONE;
        pipeline_desc.used_stages = PIPELINE_STAGE_VERTEX | PIPELINE_STAGE_PIXEL;

        tonemapping_context->pso = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void record(RenderSystem::RenderList& render_list, CommandContextHandle cmd_ctx, void* context)
    {
        Context* tonemapping_context = reinterpret_cast<Context*>(context);

        TextureHandle hdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::HDR_BUFFER);
        TextureHandle sdr_buffer = RenderSystem::get_texture_resource(render_list, ResourceNames::SDR_BUFFER);

        gfx::cmd::set_render_targets(cmd_ctx, &sdr_buffer, 1);

        TextureInfo& texture_info = gfx::textures::get_texture_info(sdr_buffer);
        Viewport viewport = { 0.0f, 0.0f, static_cast<float>(texture_info.width), static_cast<float>(texture_info.height) };
        Scissor scissor{ 0, 0, texture_info.width, texture_info.height };

        gfx::cmd::set_active_resource_layout(cmd_ctx, tonemapping_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, tonemapping_context->pso);
        gfx::cmd::set_viewports(cmd_ctx, &viewport, 1);
        gfx::cmd::set_scissors(cmd_ctx, &scissor, 1);

        TonemapPassConstants tonemapping_constants = {
            .src_texture = gfx::textures::get_shader_readable_index(hdr_buffer),
            .exposure = tonemapping_context->exposure
        };
        gfx::cmd::bind_constant(cmd_ctx, &tonemapping_constants, 2, 0);
        gfx::cmd::bind_resource_table(cmd_ctx, 1);

        gfx::cmd::draw_mesh(cmd_ctx, tonemapping_context->fullscreen_mesh);
    };

    void destroy(void* context)
    {

    }
}

namespace BRDFLutCreator
{
    struct Context
    {
        TextureHandle out_texture = {};
        // Owned
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };

    void setup(Context* pass_context)
    {
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

        pass_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {
            .resource_layout = pass_context->resource_layout,
            .used_stages = PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/04-ibl/brdf_lut_creator.hlsl",
        };
        pass_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void record(CommandContextHandle cmd_ctx, Context* pass_context)
    {
        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso_handle);

        gfx::cmd::bind_constant(cmd_ctx, &pass_context->out_texture, 1, 0);
        gfx::cmd::bind_resource_table(cmd_ctx, 1);

        const u32 lut_width = gfx::textures::get_texture_info(pass_context->out_texture).width;

        const u32 dispatch_size = lut_width / 8u;
        gfx::cmd::dispatch(cmd_ctx, dispatch_size, dispatch_size, 1);
    }
}

namespace IrradiancePass
{
    struct Context
    {
        TextureHandle src_texture = {};
        TextureHandle out_texture = {};
        // Owned
        ResourceLayoutHandle resource_layout = {};
        PipelineStateHandle pso_handle = {};
    };

    void setup(Context* pass_context)
    {
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

        pass_context->resource_layout = gfx::pipelines::create_resource_layout(layout_desc);

        // Create the Pipeline State Object
        PipelineStateObjectDesc pipeline_desc = {
            .resource_layout = pass_context->resource_layout,
            .used_stages = PIPELINE_STAGE_COMPUTE,
            .shader_file_path = L"shaders/04-ibl/env_map_irradiance.hlsl",
        };
        pass_context->pso_handle = gfx::pipelines::create_pipeline_state_object(pipeline_desc);
    }

    void record(CommandContextHandle cmd_ctx, Context* pass_context)
    {
        gfx::cmd::set_active_resource_layout(cmd_ctx, pass_context->resource_layout);
        gfx::cmd::set_pipeline_state(cmd_ctx, pass_context->pso_handle);

        gfx::cmd::bind_resource_table(cmd_ctx, 1);
        gfx::cmd::bind_resource_table(cmd_ctx, 2);
        TextureInfo src_texture_info = gfx::textures::get_texture_info(pass_context->src_texture);
        TextureInfo out_texture_info = gfx::textures::get_texture_info(pass_context->out_texture);

        IrradiancePassConstants constants = {
            .src_texture_idx = gfx::textures::get_shader_readable_index(pass_context->src_texture),
            .out_texture_idx = gfx::textures::get_shader_writable_index(pass_context->out_texture),
            .src_img_width = src_texture_info.width,
        };
        gfx::cmd::bind_constant(cmd_ctx, &constants, 3, 0);

        const u32 dispatch_size = max(1u, out_texture_info.width / 8u);
        gfx::cmd::dispatch(cmd_ctx, dispatch_size, dispatch_size, 6);
    }
}

class ToneMappingApp : public zec::App
{
public:
    ToneMappingApp() : App{ L"Basic Tone Mapping" } { }
    float frame_times[120] = { 0.0f };

    PerspectiveCamera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{};

    ViewConstantData view_constant_data = {};
    Array<DrawConstantData> draw_constant_data;
    gltf::Context gltf_context;

    BufferHandle view_cb_handle = {};
    Array<BufferHandle> draw_data_buffer_handles = {};
    MeshHandle fullscreen_mesh;

    TextureHandle brdf_lut_map = {};
    TextureHandle irradiance_map = {};
    TextureHandle radiance_map = {};

    ForwardPass::Context forward_context = {};
    BackgroundPass::Context background_context = {};
    ToneMapping::Context tonemapping_context = {};

    RenderSystem::RenderList render_list;

protected:
    void init() override final
    {
        camera_controller.init();
        camera_controller.set_camera(&camera);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 7.0f;

        camera.projection = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        gfx::begin_upload();

        constexpr float fullscreen_positions[] = {
             1.0f,  3.0f, 1.0f,
             1.0f, -1.0f, 1.0f,
            -3.0f, -1.0f, 1.0f,
        };

        constexpr float fullscreen_uvs[] = {
             1.0f,  -1.0f,
             1.0f,   1.0f,
            -1.0f,   1.0f,
        };

        constexpr u16 fullscreen_indices[] = { 0, 1, 2 };

        MeshDesc fullscreen_desc{};
        fullscreen_desc.index_buffer_desc.usage = RESOURCE_USAGE_INDEX;
        fullscreen_desc.index_buffer_desc.type = BufferType::DEFAULT;
        fullscreen_desc.index_buffer_desc.byte_size = sizeof(fullscreen_indices);
        fullscreen_desc.index_buffer_desc.stride = sizeof(fullscreen_indices[0]);
        fullscreen_desc.index_buffer_desc.data = (void*)fullscreen_indices;

        fullscreen_desc.vertex_buffer_descs[0] = {
            .usage = RESOURCE_USAGE_VERTEX,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(fullscreen_positions),
            .stride = 3 * sizeof(fullscreen_positions[0]),
            .data = (void*)(fullscreen_positions)
        };
        fullscreen_desc.vertex_buffer_descs[1] = {
           .usage = RESOURCE_USAGE_VERTEX,
           .type = BufferType::DEFAULT,
           .byte_size = sizeof(fullscreen_uvs),
           .stride = 2 * sizeof(fullscreen_uvs[0]),
           .data = (void*)(fullscreen_uvs)
        };

        fullscreen_mesh = gfx::meshes::create(fullscreen_desc);

        TextureDesc brdf_lut_desc{
            .width = 256,
            .height = 256,
            .depth = 1,
            .num_mips = 1,
            .array_size = 1,
            .is_cubemap = 0,
            .is_3d = 0,
            .format = BufferFormat::HALF_2,
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
            .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
        };
        brdf_lut_map = gfx::textures::create(brdf_lut_desc);

        TextureDesc irradiance_desc{
            .width = 64,
            .height = 64,
            .depth = 1,
            .num_mips = 1,
            .array_size = 6,
            .is_cubemap = 1,
            .is_3d = 0,
            .format = BufferFormat::R16G16B16A16_FLOAT,
            .usage = RESOURCE_USAGE_SHADER_READABLE | RESOURCE_USAGE_COMPUTE_WRITABLE,
            .initial_state = RESOURCE_USAGE_COMPUTE_WRITABLE
        };
        irradiance_map = gfx::textures::create(irradiance_desc);
        radiance_map = gfx::textures::load_from_file("textures/prefiltered_paper_mill.dds");

        //gltf::load_gltf_file("models/damaged_helmet/DamagedHelmet.gltf", gltf_context);
        gltf::load_gltf_file("models/flight_helmet/FlightHelmet.gltf", gltf_context);
        //gltf::load_gltf_file("models/environment_test/EnvironmentTest.gltf", gltf_context);

        gfx::end_upload();

        BufferDesc cb_desc = {
            .usage = RESOURCE_USAGE_CONSTANT | RESOURCE_USAGE_DYNAMIC,
            .type = BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
            .data = nullptr,
        };

        view_cb_handle = gfx::buffers::create(cb_desc);

        const size_t num_draws = gltf_context.draw_calls.size;
        draw_constant_data.reserve(num_draws);
        draw_data_buffer_handles.reserve(num_draws);
        for (size_t i = 0; i < num_draws; i++) {

            const auto& draw_call = gltf_context.draw_calls[i];
            const auto& transform = gltf_context.scene_graph.global_transforms[draw_call.scene_node_idx];
            const auto& normal_transform = gltf_context.scene_graph.normal_transforms[draw_call.scene_node_idx];

            draw_constant_data.push_back({
                .model = transform,
                .normal_transform = normal_transform,
                .material_data = gltf_context.materials[draw_call.material_index],
                });

            BufferDesc draw_cb_desc = cb_desc;
            draw_cb_desc.data = &draw_constant_data[i];

            const size_t node_idx = draw_data_buffer_handles.push_back(
                gfx::buffers::create(draw_cb_desc)
            );
        }

        forward_context = {
            .view_cb_handle = view_cb_handle,
            .draw_data_buffer_handles = &draw_data_buffer_handles,
            .scene = &gltf_context,
        };

        background_context = {
            .view_cb_handle = view_cb_handle,
            .fullscreen_quad = fullscreen_mesh,
            .mip_level = 1.0f,
        };

        tonemapping_context = {
            .exposure = 0.5f,
            .fullscreen_mesh = fullscreen_mesh,
        };

        RenderSystem::RenderPassDesc render_pass_descs[] = {
            {
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {},
                .outputs = {
                    {
                        .name = ResourceNames::HDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_RENDER_TARGET,
                        .texture_desc = {.format = BufferFormat::R16G16B16A16_FLOAT}
                    },
                    {
                        .name = ResourceNames::DEPTH_TARGET,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                        .texture_desc = {.format = BufferFormat::D32 }
                    }
                },
                .context = &forward_context,
                .setup = &ForwardPass::setup,
                .execute = &ForwardPass::record,
                .destroy = &ForwardPass::destroy
            },
            {
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {},
                .outputs = {
                    {
                        .name = ResourceNames::HDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_RENDER_TARGET,
                        .texture_desc = {.format = BufferFormat::R16G16B16A16_FLOAT}
                    },
                    {
                        .name = ResourceNames::DEPTH_TARGET,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
                        .texture_desc = {.format = BufferFormat::D32 }
                    }
                },
                .context = &background_context,
                .setup = &BackgroundPass::setup,
                .execute = &BackgroundPass::record,
                .destroy = &BackgroundPass::destroy
            },
            {
                .queue_type = CommandQueueType::GRAPHICS,
                .inputs = {
                    {
                        .name = ResourceNames::HDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_SHADER_READABLE
                    }
                },
                .outputs = {
                    {
                        .name = ResourceNames::SDR_BUFFER,
                        .type = RenderSystem::PassResourceType::TEXTURE,
                        .usage = RESOURCE_USAGE_RENDER_TARGET,
                        .texture_desc = {.format = BufferFormat::R8G8B8A8_UNORM_SRGB }
                    }
                },
                .context = &tonemapping_context,
                .setup = &ToneMapping::setup,
                .execute = &ToneMapping::record,
                .destroy = &ToneMapping::destroy
            },
        };
        RenderSystem::RenderListDesc render_list_desc = {
            .render_pass_descs = render_pass_descs,
            .num_render_passes = ARRAY_SIZE(render_pass_descs),
            .resource_to_use_as_backbuffer = ResourceNames::SDR_BUFFER,
        };

        RenderSystem::compile_render_list(render_list, render_list_desc);
        RenderSystem::setup(render_list);


        {
            CommandContextHandle cmd_ctx = gfx::cmd::provision(CommandQueueType::COMPUTE);

            BRDFLutCreator::Context brdf_context{
                .out_texture = brdf_lut_map,
            };
            BRDFLutCreator::setup(&brdf_context);
            BRDFLutCreator::record(cmd_ctx, &brdf_context);

            IrradiancePass::Context irradiance_pass_context{
                .src_texture = radiance_map,
                .out_texture = irradiance_map,
            };
            IrradiancePass::setup(&irradiance_pass_context);
            IrradiancePass::record(cmd_ctx, &irradiance_pass_context);

            ResourceTransitionDesc transitions[] = {
                {
                    .type = ResourceTransitionType::TEXTURE,
                    .texture = irradiance_map,
                    .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                    .after = RESOURCE_USAGE_SHADER_READABLE
                },
                {
                    .type = ResourceTransitionType::TEXTURE,
                    .texture = brdf_lut_map,
                    .before = RESOURCE_USAGE_COMPUTE_WRITABLE,
                    .after = RESOURCE_USAGE_SHADER_READABLE
                },
            };
            gfx::cmd::transition_resources(cmd_ctx, transitions, ARRAY_SIZE(transitions));

            CmdReceipt receipt = gfx::cmd::return_and_execute(&cmd_ctx, 1);
            gfx::cmd::cpu_wait(receipt);
        }
    }

    void shutdown() override final
    {
        RenderSystem::destroy(render_list);
    }

    void update(const zec::TimeData& time_data) override final
    {
        frame_times[gfx::get_current_cpu_frame() % 120] = time_data.delta_milliseconds_f;

        const TextureInfo& radiance_map_info = gfx::textures::get_texture_info(radiance_map);

        camera_controller.update(time_data.delta_seconds_f);
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.invVP = invert(camera.projection * mat4(to_mat3(camera.view), {}));
        view_constant_data.camera_position = camera.position;
        view_constant_data.radiance_map_idx = gfx::textures::get_shader_readable_index(radiance_map);
        view_constant_data.irradiance_map_idx = gfx::textures::get_shader_readable_index(irradiance_map);
        view_constant_data.brdf_LUT = gfx::textures::get_shader_readable_index(brdf_lut_map);
        view_constant_data.num_env_levels = float(radiance_map_info.num_mips);
        view_constant_data.time = time_data.elapsed_seconds_f;
    }

    void copy() override final
    {
        gfx::buffers::update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
    }

    void render() override final
    {
        RenderSystem::execute(render_list);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    ToneMappingApp app{};
    return app.run();
}