#include "app.h"
#include "utils/exceptions.h"
#include "gfx/d3d12/globals.h"

// TODO: Remove this once no longer using DXCall directly
using namespace zec;

struct Vertex
{
    float position[3] = {};
    u32 color = 0x000000ff;
};

class HelloWorldApp : public zec::App
{
public:
    HelloWorldApp() : App{ L"Hello World!" } { }

    float clear_color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

    ID3D12RootSignature* root_signature = nullptr;
    ID3D12PipelineState* pso = nullptr;
    ID3D12Resource* vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

protected:
    void init() override final
    {

        // Create a root signature consisting of a descriptor table with a single CBV.
        {
            D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
            root_signature_desc.NumParameters = 0;
            root_signature_desc.pParameters = nullptr;
            root_signature_desc.NumStaticSamplers = 0;
            root_signature_desc.pStaticSamplers = nullptr;
            root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ID3DBlob* signature;
            ID3DBlob* error;
            DXCall(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            DXCall(zec::dx12::device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
            signature != nullptr && signature->Release();
            error != nullptr && error->Release();
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
            DXCall(D3DCompileFromFile(L"shaders/basic.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compile_flags, 0, &vertex_shader, nullptr));
            DXCall(D3DCompileFromFile(L"shaders/basic.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compile_flags, 0, &pixel_shader, nullptr));

            // Create the vertex input layout
            D3D12_INPUT_ELEMENT_DESC input_element_desc[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            D3D12_RASTERIZER_DESC rasterizer_desc = { };
            rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
            rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
            rasterizer_desc.FrontCounterClockwise = FALSE;
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
            psoDesc.pRootSignature = root_signature;
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
            psoDesc.RTVFormats[0] = zec::dx12::swap_chain.format;
            psoDesc.SampleDesc.Count = 1;

            dx12::device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        }


        // Create the vertex buffer.
        {
            float aspect_ratio = float(width) / float(height);
            // Define the geometry for a triangle.
            Vertex triangle_vertices[] =
            {
                { { 0.0f, 0.25f * aspect_ratio, 0.0f }, 0xff0000ff },
                { { 0.25f, -0.25f * aspect_ratio, 0.0f }, 0x00ff00ff },
                { { -0.25f, -0.25f * aspect_ratio, 0.0f }, 0x0000ffff }
            };

            const u32 vertex_buffer_size = sizeof(triangle_vertices);

            // Note: using upload heaps to transfer static data like vert buffers is not 
            // recommended. Every time the GPU needs it, the upload heap will be marshalled 
            // over. Please read up on Default Heap usage. An upload heap is used here for 
            // code simplicity and because there are very few verts to actually transfer.
            D3D12_HEAP_PROPERTIES heap_properties{ };
            heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
            heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heap_properties.CreationNodeMask = 1;
            heap_properties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resource_desc{ };
            resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            resource_desc.Alignment = 0;
            resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resource_desc.Width = vertex_buffer_size;
            resource_desc.Height = 1;
            resource_desc.DepthOrArraySize = 1;
            resource_desc.MipLevels = 1;
            resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
            resource_desc.SampleDesc.Count = 1;
            resource_desc.SampleDesc.Quality = 0;


            DXCall(dx12::device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &resource_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&vertex_buffer)));

            // Copy the triangle data to the vertex buffer.
            UINT8* vertex_data_begin;
            D3D12_RANGE read_range{ 0, 0 };
            DXCall(vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin)));
            memcpy(vertex_data_begin, triangle_vertices, vertex_buffer_size);
            vertex_buffer->Unmap(0, nullptr);

            // Initialize the vertex buffer view.
            vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
            vertex_buffer_view.StrideInBytes = sizeof(Vertex);
            vertex_buffer_view.SizeInBytes = vertex_buffer_size;
        }
    }

    void shutdown() override final
    {
        vertex_buffer->Release();
        pso->Release();
        root_signature->Release();
    }

    void update(const zec::TimeData& time_data) override final
    {
        clear_color[2] = 0.5f * sinf(float(time_data.elapsed_seconds_f)) + 0.5f;
    }

    void render() override final
    {
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
        D3D12_RECT scissor_rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        zec::dx12::RenderTexture& render_target = zec::dx12::swap_chain.back_buffers[zec::dx12::current_frame_idx];

        // TODO: Provide interface for creating barriers like this
        D3D12_RESOURCE_BARRIER barrier{  };
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = render_target.texture.resource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;


        zec::dx12::cmd_list->ResourceBarrier(1, &barrier);

        // TODO: Provide handles for referencing render targets
        // TODO: Provide interface for clearing a render target
        zec::dx12::cmd_list->ClearRenderTargetView(render_target.rtv, clear_color, 0, nullptr);

        zec::dx12::cmd_list->SetGraphicsRootSignature(root_signature);
        zec::dx12::cmd_list->RSSetViewports(1, &viewport);
        zec::dx12::cmd_list->RSSetScissorRects(1, &scissor_rect);
        zec::dx12::cmd_list->OMSetRenderTargets(1, &render_target.rtv, false, nullptr);
        zec::dx12::cmd_list->SetPipelineState(pso);
        zec::dx12::cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        zec::dx12::cmd_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
        zec::dx12::cmd_list->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER present_barrier{  };
        present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        present_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        present_barrier.Transition.pResource = render_target.texture.resource;
        present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        zec::dx12::cmd_list->ResourceBarrier(1, &present_barrier);
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