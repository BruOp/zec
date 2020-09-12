#include "app.h"
#include "core/zec_math.h"
#include "utils/exceptions.h"

// TODO: Remove this once no longer using DXCall directly
using namespace zec;

struct DrawData
{
    mat44 model_view_transform;
    mat44 projection_matrix;
    float padding[32];
};

static_assert(sizeof(DrawData) == 256);

class HelloWorldApp : public zec::App
{
public:
    HelloWorldApp() : App{ L"Hello World!" } { }

    float clear_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

    ID3D12PipelineState* pso = nullptr;

    MeshHandle cube_mesh = {};
    BufferHandle cb_handle = {};
    ResourceLayoutHandle resource_layout = {};
    DrawData mesh_transform = {};

protected:
    void init() override final
    {

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            ResourceLayoutDesc layout_desc{
                { ResourceLayoutEntryType::CONSTANT_BUFFER, ShaderVisibility::VERTEX },
            };

            resource_layout = renderer.create_resource_layout(layout_desc);
        }

        ID3DBlob* vertex_shader = nullptr;
        ID3DBlob* pixel_shader = nullptr;

    #ifdef _DEBUG
        // Enable better shader debugging with the graphics debugging tools.
        UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #else
        UINT compile_flags = 0;
    #endif // _DEBUG

        // Create the Pipeline State Object
        {
            // Compile the shader
            // Todo: Provide interface for compiling shaders
            ID3DBlob* error = nullptr;

            HRESULT result = D3DCompileFromFile(L"shaders/basic.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, &error);
            if (error) {
                const char* error_string = (char*)error->GetBufferPointer();
                size_t len = std::strlen(error_string);
                std::wstring wc(len, L'#');
                mbstowcs(&wc[0], error_string, len);
                debug_print(wc);
            }
            DXCall(result);

            result = D3DCompileFromFile(L"shaders/basic.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, &error);
            if (error) {
                print_blob(error);
            }
            DXCall(result);

            // Create the vertex input layout
            D3D12_INPUT_ELEMENT_DESC input_element_desc[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            D3D12_RASTERIZER_DESC rasterizer_desc = { };
            rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
            rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
            rasterizer_desc.FrontCounterClockwise = TRUE;
            rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            rasterizer_desc.DepthClipEnable = TRUE;
            rasterizer_desc.MultisampleEnable = FALSE;
            rasterizer_desc.AntialiasedLineEnable = FALSE;
            rasterizer_desc.ForcedSampleCount = 0;
            rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

            D3D12_BLEND_DESC blend_desc = { };
            blend_desc.AlphaToCoverageEnable = FALSE;
            blend_desc.IndependentBlendEnable = FALSE;
            const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc =
            {
                FALSE,FALSE,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                D3D12_LOGIC_OP_NOOP,
                D3D12_COLOR_WRITE_ENABLE_ALL,
            };
            for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
                blend_desc.RenderTarget[i] = default_render_target_blend_desc;

            // Create a pipeline state object description, then create the object
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { input_element_desc, _countof(input_element_desc) };
            psoDesc.pRootSignature = renderer.root_signatures[resource_layout.idx];
            psoDesc.VS.BytecodeLength = vertex_shader->GetBufferSize();
            psoDesc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
            psoDesc.PS.BytecodeLength = pixel_shader->GetBufferSize();
            psoDesc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
            psoDesc.RasterizerState = rasterizer_desc;
            psoDesc.BlendState = blend_desc;
            psoDesc.DepthStencilState.DepthEnable = FALSE;
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = renderer.swap_chain.format;
            psoDesc.SampleDesc.Count = 1;

            renderer.device_context.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        }

        renderer.begin_upload();

        // Create the vertex buffer.
        {
            // Define the geometry for a triangle.

            constexpr float cube_positions[] = {
                -0.5f,  0.5f, -0.5f, // +Y (top face)
                 0.5f,  0.5f, -0.5f,
                 0.5f,  0.5f,  0.5f,
                -0.5f,  0.5f,  0.5f,
                -0.5f, -0.5f,  0.5f,  // -Y (bottom face)
                 0.5f, -0.5f,  0.5f,
                 0.5f, -0.5f, -0.5f,
                -0.5f, -0.5f, -0.5f,
            };

            constexpr u32 cube_colors[] = {
                    0xff00ff00, // +Y (top face)
                    0xff00ffff,
                    0xffffffff,
                    0xffffff00,
                    0xffff0000, // -Y (bottom face)
                    0xffff00ff,
                    0xff0000ff,
                    0xff000000,
            };

            constexpr u16 cube_indices[] = {
                2, 1, 0,
                3, 2, 0,
                5, 1, 2,
                5, 6, 1,
                4, 3, 0,
                7, 4, 0,
                1, 7, 0,
                6, 7, 1,
                4, 2, 3,
                4, 5, 2,
                7, 5, 4,
                7, 6, 5
            };

            MeshDesc mesh_desc{};
            mesh_desc.index_buffer_desc.usage = BufferUsage::INDEX;
            mesh_desc.index_buffer_desc.type = BufferType::DEFAULT;
            mesh_desc.index_buffer_desc.byte_size = sizeof(cube_indices);
            mesh_desc.index_buffer_desc.stride = sizeof(cube_indices[0]);
            mesh_desc.index_buffer_desc.data = (void*)cube_indices;

            mesh_desc.vertex_buffer_descs[0] = {
                    BufferUsage::VERTEX,
                    BufferType::DEFAULT,
                    sizeof(cube_positions),
                    3 * sizeof(cube_positions[0]),
                    (void*)(cube_positions)
            };
            mesh_desc.vertex_buffer_descs[1] = {
               BufferUsage::VERTEX,
               BufferType::DEFAULT,
               sizeof(cube_colors),
               sizeof(cube_colors[0]),
               (void*)(cube_colors)
            };

            cube_mesh = renderer.create_mesh(mesh_desc);
        }

        renderer.end_upload();

        mesh_transform.model_view_transform = identity_mat44();
        set_translation(mesh_transform.model_view_transform, vec3{ 0.0f, 0.0f, -2.0f });
        mesh_transform.projection_matrix = perspective_projection(
            float(width) / float(height),
            deg_to_rad(65.0f),
            0.1f, // near
            100.0f // far
        );

        // Create constant buffer
        {
            BufferDesc cb_desc = {};
            cb_desc.byte_size = sizeof(DrawData);
            cb_desc.data = &mesh_transform;
            cb_desc.stride = 0;
            cb_desc.type = BufferType::DEFAULT;
            cb_desc.usage = BufferUsage::CONSTANT | BufferUsage::DYNAMIC;

            cb_handle = renderer.create_buffer(cb_desc);
        }
    }

    void shutdown() override final
    {
        pso->Release();
    }

    void update(const zec::TimeData& time_data) override final
    {
        clear_color[2] = 0.5f * sinf(float(time_data.elapsed_seconds_f)) + 0.5f;

        quaternion q = from_axis_angle(vec3{ 0.0f, 1.0f, -1.0f }, time_data.delta_seconds_f);
        rotate(mesh_transform.model_view_transform, q);

        renderer.update_buffer(cb_handle, &mesh_transform, sizeof(mesh_transform));
    }

    void render() override final
    {
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        D3D12_RECT scissor_rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        dx12::RenderTexture& render_target = renderer.swap_chain.back_buffers[renderer.current_frame_idx];

        // TODO: Provide handles for referencing render targets
        // TODO: Provide interface for clearing a render target
        renderer.cmd_list->ClearRenderTargetView(render_target.rtv, clear_color, 0, nullptr);

        dx12::Buffer& constant_buffer = renderer.buffers[cb_handle];
        renderer.set_active_resource_layout(resource_layout);
        renderer.cmd_list->SetGraphicsRootConstantBufferView(0, constant_buffer.gpu_address + (constant_buffer.size * renderer.current_frame_idx));
        renderer.cmd_list->RSSetViewports(1, &viewport);
        renderer.cmd_list->RSSetScissorRects(1, &scissor_rect);
        renderer.cmd_list->OMSetRenderTargets(1, &render_target.rtv, false, nullptr);
        renderer.cmd_list->SetPipelineState(pso);
        renderer.draw_mesh(cube_mesh);
    }

    void before_reset() override final
    { }

    void after_reset() override final
    { }
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HelloWorldApp app{};
    return app.run();
}