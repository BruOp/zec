#include <windows.h>
#include <app.h>
#include <core/zec_math.h>
#include <camera.h>
#include <asset_lib.h>
#include <gfx/samplers.h>

using namespace zec;

struct ViewConstantData
{
    mat4 view;
    mat4 projection;
    mat4 VP;
    vec3 camera_position;
    float time;
    float padding[12];
};

static_assert(sizeof(ViewConstantData) == 256);

struct MaterialTextureInfo
{
    static constexpr u32 k_invalid_texture_index = (1u << 24u) - 1u;
    u32 index : 24 = k_invalid_texture_index;
    u32 sampler_idx : 8 = UINT8_MAX;
};

struct MaterialData
{
    zec::vec4 albedo = { 1.f, 1.f, 1.f, 1.f };
    zec::vec3 emissive = { 0.f };
    float metalness = 1.0f;
    float roughness = 1.0f;
    float alpha_cutoff = 0.5f;
    MaterialTextureInfo albedo_texture = {};
    MaterialTextureInfo normal_texture = {};
    MaterialTextureInfo metalness_roughness_ao_texture = {};
    MaterialTextureInfo emissive_texture = {};
    float padding[2] = {};
};

struct TransformData
{
    mat34 local_transform;
    mat34 global_transform;
    mat3 normal_transform;
};

struct DrawConstantData
{
    MaterialData material_data = {};
    mat34 model = {};
    mat3 normal_transform = {};
    u32 padding[24];
};

static_assert(sizeof(DrawConstantData) == 256);

mat34 compute_local_transform(const assets::TransformNode& transform)
{
    mat34 local_mat = identity_mat34();
    // Update local transform
    const vec3& scale = transform.scale;
    set_scale(local_mat, scale);
    mat34 rotation_matrix = quat_to_mat34(transform.rotation);
    local_mat = rotation_matrix * local_mat;
    set_translation(local_mat, transform.translation);
    return local_mat;
}


class AssetLoadingApp : public zec::App
{
public:
    AssetLoadingApp() : App{ L"Basic GLTF Loading and Rendering" } { }

    float frame_times[120] = { 0.0f };
    float clear_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    LinearAllocator allocator{};

    PerspectiveCamera camera = {};
    OrbitCameraController camera_controller = OrbitCameraController{};

    ViewConstantData view_constant_data = {};
    Array<DrawConstantData> draw_constant_data;
    Array<rhi::Draw> draws;

    rhi::BufferHandle view_cb_handle = {};
    Array<rhi::BufferHandle> draw_data_buffer_handles = {};
    rhi::ResourceLayoutHandle resource_layout = {};
    rhi::PipelineStateHandle pso_handle = {};

    rhi::TextureHandle depth_target = {};

protected:
    void init() override final
    {
        allocator.init(g_MB * 10);

        camera_controller.origin = vec3{ 0.0f, 0.0f, 0.0f };
        camera_controller.radius = 2.0f;
        //camera_controller.yaw = 0.0f;
        camera_controller.settings.zoom_sensitivity = 0.001f;
        camera_controller.settings.movement_sensitivity = 100.f;
        camera_controller.settings.yaw_sensitivity = 200.f;
        camera_controller.settings.pitch_sensitivity = 200.f;

        camera = create_camera(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            constexpr u32 num_textures = 1024;
            rhi::ResourceLayoutDesc layout_desc{
                .num_constants = 0,
                .constant_buffers = {
                    { rhi::ShaderVisibility::ALL },
                    { rhi::ShaderVisibility::ALL },
                },
                .num_constant_buffers = 2,
                .tables = {
                    {.usage = rhi::ResourceAccess::READ, .count = 4096 },
                },
                .num_resource_tables = 1,
                .static_samplers = {
                    {
                        .filtering = rhi::SamplerFilterType::ANISOTROPIC,
                        .wrap_u = rhi::SamplerWrapMode::WRAP,
                        .wrap_v = rhi::SamplerWrapMode::WRAP,
                        .binding_slot = 0,
                    },
                },
                .num_static_samplers = 1,
            };

            resource_layout = renderer.resource_layouts_create(layout_desc);
        }

        // Create the Pipeline State Object
        {
            std::string errors{};
            rhi::ManagedShaderBlobsHandle shader_blob{ &renderer };
            shader_blob.compile(
                { .used_stages = rhi::PIPELINE_STAGE_VERTEX | rhi::PIPELINE_STAGE_PIXEL, .shader_file_path = L"shaders/gltf_shader.hlsl" },
                errors
            );
            rhi::PipelineStateObjectDesc pipeline_desc = {};
            pipeline_desc.input_assembly_desc = { {
                { rhi::MeshAttribute::POSITION, 0, rhi::BufferFormat::FLOAT_3, 0 },
                { rhi::MeshAttribute::NORMAL, 0, rhi::BufferFormat::FLOAT_3, 1 },
                { rhi::MeshAttribute::TEXCOORD, 0, rhi::BufferFormat::FLOAT_2, 2 },
            } };
            pipeline_desc.rtv_formats[0] = rhi::BufferFormat::R8G8B8A8_UNORM_SRGB;
            pipeline_desc.depth_buffer_format = rhi::BufferFormat::D32;
            pipeline_desc.raster_state_desc.cull_mode = rhi::CullMode::BACK_CCW;
            pipeline_desc.raster_state_desc.flags |= rhi::DEPTH_CLIP_ENABLED;
            pipeline_desc.depth_stencil_state.depth_cull_mode = rhi::ComparisonFunc::LESS;
            pipeline_desc.depth_stencil_state.depth_write = TRUE;

            pso_handle = renderer.pipelines_create(shader_blob.get(), resource_layout, pipeline_desc);
        }

        rhi::CommandContextHandle cmd_ctx = renderer.cmd_provision(rhi::CommandQueueType::COPY);

        rhi::BufferDesc cb_desc = {
            .usage = rhi::RESOURCE_USAGE_CONSTANT | rhi::RESOURCE_USAGE_DYNAMIC,
            .type = rhi::BufferType::DEFAULT,
            .byte_size = sizeof(ViewConstantData),
            .stride = 0,
        };

        view_cb_handle = renderer.buffers_create(cb_desc);

        // Asset loading
        {
            assets::MeshAsset model = {};
            LinearAllocator asset_allocator{};
            StackAllocator stack_allocator{};
            asset_allocator.init(g_GB);
            stack_allocator.init(1024 * 1024); // 1 MB
            ZecResult res = assets::load_binary_file("./models/FlightHelmet/FlightHelmet.azec", asset_allocator, model);
            ASSERT(res == ZecResult::SUCCESS);
            draws.init(&allocator, model.submeshes.get_size());

            rhi::BufferDesc index_buffer_desc{
                .usage = rhi::RESOURCE_USAGE_INDEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = u32(model.index_buffers.byte_size),
                    .stride = sizeof(u32),
            };
            rhi::BufferHandle index_buffer = renderer.buffers_create(index_buffer_desc);
            renderer.buffers_set_data(cmd_ctx, index_buffer, model.index_buffers.ptr, model.index_buffers.byte_size);

            rhi::BufferDesc vertex_buffer_descs[3];
            for (size_t i = 0; i < ARRAYSIZE(vertex_buffer_descs); i++)
            {
                vertex_buffer_descs[i] = {
                    .usage = rhi::RESOURCE_USAGE_VERTEX,
                    .type = rhi::BufferType::DEFAULT,
                    .byte_size = u32(model.vertex_buffers[i].byte_size),
                    .stride = assets::attr_meta_data[i].stride
                };
            }

            rhi::BufferHandle vertex_attribute_buffers[ARRAYSIZE(vertex_buffer_descs)];
            for (size_t i = 0u; i < ARRAYSIZE(vertex_attribute_buffers); i++)
            {
                vertex_attribute_buffers[i] = renderer.buffers_create(vertex_buffer_descs[i]);
                renderer.buffers_set_data(cmd_ctx, vertex_attribute_buffers[i], model.vertex_buffers[i].ptr, vertex_buffer_descs[i].byte_size);
            }

            Array<rhi::TextureHandle> texture_handles{ model.textures.get_size() };
            texture_handles.init(&allocator);
            for (size_t texture_idx = 0; texture_idx < model.textures.get_size(); texture_idx++)
            {
                assets::TextureDesc texture_desc = model.textures[texture_idx];
                wchar_t* path_ptr = static_cast<wchar_t*>(model.texture_paths.ptr) + size_t(texture_desc.path.offset);
                std::wstring_view path_view{ path_ptr, texture_desc.path.get_size() };
                rhi::TextureHandle handle = renderer.textures_create_from_file(cmd_ctx, path_view.data());
                texture_handles.push_back(handle);
            }

            Array<MaterialData> materials{ model.materials.get_size() };
            materials.init(&allocator);
            for (const assets::MaterialDesc& mat_desc : model.materials)
            {
                MaterialData mat_data{
                    .albedo = mat_desc.albedo,
                    .emissive = mat_desc.emissive,
                    .metalness = mat_desc.metalness,
                    .roughness = mat_desc.roughness,
                    .alpha_cutoff = mat_desc.alpha_cutoff,
                };
                // TEXTURE DATA
                if (is_valid(mat_desc.albedo_texture.texture))
                {
                    rhi::TextureHandle albedo_tex = texture_handles[mat_desc.albedo_texture.texture.idx];
                    mat_data.albedo_texture = { .index = renderer.get_readable_index(albedo_tex), .sampler_idx = u32(Samplers::ANISOTROPIC_WRAP) };
                }
                if (is_valid(mat_desc.normal_texture.texture))
                {
                    rhi::TextureHandle normal_texture = texture_handles[mat_desc.normal_texture.texture.idx];
                    mat_data.normal_texture = { .index = renderer.get_readable_index(normal_texture), .sampler_idx = u32(Samplers::ANISOTROPIC_WRAP) };
                }
                if (is_valid(mat_desc.metal_roughness_ao_texture.texture))
                {
                    rhi::TextureHandle MRAO_texture = texture_handles[mat_desc.metal_roughness_ao_texture.texture.idx];
                    mat_data.metalness_roughness_ao_texture = { .index = renderer.get_readable_index(MRAO_texture), .sampler_idx = u32(Samplers::ANISOTROPIC_WRAP) };
                }
                if (is_valid(mat_desc.emissive_texture.texture))
                {
                    rhi::TextureHandle emissive_texture = texture_handles[mat_desc.emissive_texture.texture.idx];
                    mat_data.emissive_texture = { .index = renderer.get_readable_index(emissive_texture), .sampler_idx = u32(Samplers::ANISOTROPIC_WRAP) };
                }

                materials.push_back(mat_data);
            }

            Array<TransformData> transform_nodes{ model.transform_nodes.get_size() };
            transform_nodes.init(&allocator);
            {
                // Process transformation chains
                for (size_t node_idx = 0; node_idx < model.transform_nodes.get_size(); ++node_idx)
                {
                    assets::TransformNode& node = model.transform_nodes[node_idx];
                    TransformData transform_data{};
                    transform_data.local_transform = compute_local_transform(node);

                    set_scale(transform_data.normal_transform, -node.scale);
                    transform_data.normal_transform = quat_to_mat3(node.rotation) * transform_data.normal_transform;

                    transform_nodes.push_back(transform_data);
                }

                for (size_t node_idx = 0; node_idx < model.transform_nodes.get_size(); ++node_idx)
                {
                    mat34 parent_transform{};
                    TransformData& transform_data = transform_nodes[node_idx];
                    assets::TransformNode& node = model.transform_nodes[node_idx];
                    if (is_valid(node.parent_transform))
                    {
                        ASSERT(node.parent_transform.idx < node_idx);
                        parent_transform = transform_nodes[node.parent_transform.idx].global_transform;
                    }
                    else
                    {
                        transform_data.global_transform = transform_data.local_transform;
                    }
                }

                // Process Meshes
                draw_data_buffer_handles.init(&allocator, model.render_nodes.get_size());
                for (size_t render_node_idx = 0; render_node_idx < model.render_nodes.get_size(); render_node_idx++)
                {
                    assets::RenderNode render_node = model.render_nodes[render_node_idx];
                    assets::MeshDesc& mesh = model.meshes[render_node.mesh_id.idx];

                    TypedArrayView<assets::SubmeshDesc> mesh_submeshes{ mesh.submeshes.size, &model.submeshes[mesh.submeshes.offset] };
                    for (size_t submesh_idx = 0; submesh_idx < mesh.submeshes.get_size(); submesh_idx++)
                    {
                        assets::SubmeshDesc submesh_desc = mesh_submeshes[submesh_idx];

                        rhi::Draw draw = {
                                .index_buffer = index_buffer,
                                .num_vertex_buffers = u32(submesh_desc.vertex_attributes.size),
                                .index_offset = submesh_desc.index_buffer_view.offset,
                                .index_count = submesh_desc.index_buffer_view.length,
                                .vertex_offset = submesh_desc.vertex_attributes[0].buffer_view_id.offset
                        };

                        for (size_t vertex_idx = 0; vertex_idx < submesh_desc.vertex_attributes.size; vertex_idx++)
                        {
                            draw.vertex_buffers[vertex_idx] = vertex_attribute_buffers[vertex_idx];
                        }

                        draws.push_back(draw);

                        const TransformData& transform_data = transform_nodes[render_node.transform_id.idx];
                        const MaterialData& material = materials[submesh_desc.material_id.idx];
                        DrawConstantData draw_constants = {
                                .material_data = material,
                                .model = transform_data.global_transform,
                                .normal_transform = transform_data.normal_transform,
                        };

                        rhi::BufferDesc cb_desc = {
                            .usage = rhi::RESOURCE_USAGE_CONSTANT | rhi::RESOURCE_USAGE_DYNAMIC,
                            .type = rhi::BufferType::DEFAULT,
                            .byte_size = sizeof(DrawConstantData),
                            .stride = 0,
                        };
                        rhi::BufferHandle cb = renderer.buffers_create(cb_desc);
                        renderer.buffers_set_data(cb, &draw_constants, sizeof(draw_constants));
                        draw_data_buffer_handles.push_back(cb);
                    }
                }

                materials.shutdown();
                texture_handles.shutdown();
                transform_nodes.shutdown();
            }

            stack_allocator.shutdown();
            asset_allocator.shutdown();
        }

        rhi::CmdReceipt upload_receipt = renderer.cmd_return_and_execute(&cmd_ctx, 1);

        rhi::TextureDesc depth_texture_desc = {
            .width = width,
            .height = height,
            .depth = 1,
            .num_mips = 1,
            .array_size = 1,
            .is_3d = false,
            .format = rhi::BufferFormat::D32,
            .usage = rhi::RESOURCE_USAGE_DEPTH_STENCIL,
            .clear_depth = 1.0,
        };
        depth_target = renderer.textures_create(depth_texture_desc);

        renderer.cmd_cpu_wait(upload_receipt);
    }

    void shutdown() override final
    {
        draws.shutdown();
        draw_data_buffer_handles.shutdown();
        allocator.shutdown();
    }

    void update(const zec::TimeData& time_data) override final
    {

        ui_renderer.begin_frame();
        {
            const auto framerate = ImGui::GetIO().Framerate;

            ImGui::Begin("GLTF Loader");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);

            ImGui::PlotHistogram("Frame Times", frame_times, IM_ARRAYSIZE(frame_times), 0, 0, 0, FLT_MAX, ImVec2(240.0f, 80.0f));

            ImGui::End();
        }
        ui_renderer.end_frame();

        const input::InputState input_state = input_manager.get_state();
        camera_controller.update(camera, input_state, time_data.delta_seconds_f);
        camera_controller.apply(camera);
    }

    void copy(const zec::TimeData& time_data)
    {
        view_constant_data.view = camera.view;
        view_constant_data.projection = camera.projection;
        view_constant_data.VP = camera.projection * camera.view;
        view_constant_data.camera_position = camera.position;
        view_constant_data.time = time_data.elapsed_seconds_f;
    }

    void render(const zec::TimeData& time_data) override final
    {
        rhi::CommandContextHandle cmd_ctx = renderer.begin_frame();

        renderer.buffers_update(view_cb_handle, &view_constant_data, sizeof(view_constant_data));
        rhi::Viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        rhi::Scissor scissor{ 0, 0, width, height };

        rhi::TextureHandle backbuffer = renderer.get_current_back_buffer_handle();
        renderer.cmd_clear_render_target(cmd_ctx, backbuffer, clear_color);
        renderer.cmd_clear_depth_target(cmd_ctx, depth_target, 1.0f, 0);

        renderer.cmd_set_graphics_resource_layout(cmd_ctx, resource_layout);
        renderer.cmd_set_graphics_pipeline_state(cmd_ctx, pso_handle);
        renderer.cmd_set_viewports(cmd_ctx, &viewport, 1);
        renderer.cmd_set_scissors(cmd_ctx, &scissor, 1);

        renderer.cmd_set_render_targets(cmd_ctx, &backbuffer, 1, depth_target);

        renderer.cmd_bind_graphics_resource_table(cmd_ctx, 2);
        renderer.cmd_bind_graphics_constant_buffer(cmd_ctx, view_cb_handle, 1);

        for (size_t i = 0; i < draws.get_size(); i++) {
            renderer.cmd_bind_graphics_constant_buffer(cmd_ctx, draw_data_buffer_handles[i], 0);
            renderer.cmd_draw(cmd_ctx, draws[i]);
        }

        ui_renderer.draw_frame(cmd_ctx);
        renderer.end_frame(cmd_ctx);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    AssetLoadingApp app{};
    return app.run();
}