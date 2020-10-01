#include "pch.h"
#include "gfx/gfx.h"
#include "dx_utils.h"
#include "dx_helpers.h"
#include "globals.h"

namespace zec
{
    using namespace dx12;

    namespace dx12
    {
        ID3DBlob* compile_shader(const wchar* file_name, PipelineStage stage)
        {
            ID3DBlob* error = nullptr;
            ID3DBlob* blob = nullptr;

        #ifdef _DEBUG
            // Enable better shader debugging with the graphics debugging tools.
            UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #else
            UINT compile_flags = 0;
        #endif // _DEBUG

            char entry[10] = "";
            char target[10] = "";
            ASSERT(stage != PIPELINE_STAGE_INVALID);
            if (stage == PIPELINE_STAGE_VERTEX) {
                strcpy(entry, "VSMain");
                strcpy(target, "vs_5_1");
            }
            else if (stage == PIPELINE_STAGE_PIXEL) {
                strcpy(entry, "PSMain");
                strcpy(target, "ps_5_1");
            }
            else if (stage == PIPELINE_STAGE_COMPUTE) {
                strcpy(entry, "CSMain");
                strcpy(target, "cs_5_1");
            }

            HRESULT result = D3DCompileFromFile(file_name, nullptr, nullptr, entry, target, compile_flags, 0, &blob, &error);
            if (error) {
                const char* error_string = (char*)error->GetBufferPointer();
                size_t len = std::strlen(error_string);
                std::wstring wc(len, L'#');
                mbstowcs(&wc[0], error_string, len);
                debug_print(wc);
            }
            DXCall(result);
            return blob;
        }
    }

    PipelineStateHandle create_pipeline_state_object(const PipelineStateObjectDesc& desc)
    {
        ASSERT(is_valid(desc.resource_layout));

        ID3D12RootSignature* root_signature = g_root_signatures[desc.resource_layout];
        ID3D12PipelineState* pipeline = nullptr;

        if (desc.used_stages & PIPELINE_STAGE_COMPUTE) {
            ASSERT(desc.used_stages == PIPELINE_STAGE_COMPUTE);

            ID3DBlob* compute_shader = compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_COMPUTE);

            D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
            pso_desc.CS.BytecodeLength = compute_shader->GetBufferSize();
            pso_desc.CS.pShaderBytecode = compute_shader->GetBufferPointer();
            pso_desc.pRootSignature = root_signature;
            DXCall(g_device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));
            compute_shader->Release();

            return { g_pipelines.push_back(pipeline) };
        }

        // Create the input assembly desc
        std::string semantic_names[MAX_NUM_MESH_VERTEX_BUFFERS];
        D3D12_INPUT_ELEMENT_DESC d3d_elements[MAX_NUM_MESH_VERTEX_BUFFERS];
        u32 num_input_elements = 0;
        {
            for (size_t i = 0; i < MAX_NUM_MESH_VERTEX_BUFFERS; i++) {
                D3D12_INPUT_ELEMENT_DESC& d3d_desc = d3d_elements[i];
                const auto& input_entry = desc.input_assembly_desc.elements[i];
                if (input_entry.attribute_type == MESH_ATTRIBUTE_INVALID) {
                    break;
                }

                switch (input_entry.attribute_type) {
                case MESH_ATTRIBUTE_POSITION:
                    semantic_names[i] = "POSITION";
                    break;
                case MESH_ATTRIBUTE_NORMAL:
                    semantic_names[i] = "NORMAL";
                    break;
                case MESH_ATTRIBUTE_TEXCOORD:
                    semantic_names[i] = "TEXCOORD";
                    break;
                case MESH_ATTRIBUTE_BLENDINDICES:
                    semantic_names[i] = "BLENDINDICES";
                    break;
                case MESH_ATTRIBUTE_BLENDWEIGHTS:
                    semantic_names[i] = "BLENDWEIGHT";
                    break;
                case MESH_ATTRIBUTE_COLOR:
                    semantic_names[i] = "COLOR";
                    break;
                default:
                    throw std::runtime_error("Unsupported mesh attribute type!");
                }

                d3d_desc.SemanticName = semantic_names[i].c_str();
                d3d_desc.SemanticIndex = input_entry.semantic_index;
                d3d_desc.Format = to_d3d_format(input_entry.format);
                d3d_desc.InputSlot = input_entry.input_slot;
                d3d_desc.AlignedByteOffset = input_entry.aligned_byte_offset;
                d3d_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA; // TODO: Support instanced data
                d3d_desc.InstanceDataStepRate = 0;
                num_input_elements++;
            };
            ASSERT(num_input_elements > 0);
        }

        // Create a pipeline state object description, then create the object
        D3D12_RASTERIZER_DESC rasterizer_desc = to_d3d_rasterizer_desc(desc.raster_state_desc);

        const auto& blend_desc = desc.blend_state;
        D3D12_BLEND_DESC d3d_blend_desc = { };
        d3d_blend_desc.AlphaToCoverageEnable = blend_desc.alpha_to_coverage_enable;
        d3d_blend_desc.IndependentBlendEnable = blend_desc.independent_blend_enable;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            d3d_blend_desc.RenderTarget[i] = to_d3d_blend_desc(desc.blend_state.render_target_blend_descs[i]);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.InputLayout = { d3d_elements, num_input_elements };
        pso_desc.pRootSignature = root_signature;
        pso_desc.RasterizerState = rasterizer_desc;
        pso_desc.BlendState = d3d_blend_desc;
        pso_desc.DepthStencilState = to_d3d_depth_stencil_desc(desc.depth_stencil_state);
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.SampleDesc.Count = 1;
        if (desc.depth_buffer_format != BufferFormat::INVALID) {
            pso_desc.DSVFormat = to_d3d_format(desc.depth_buffer_format);
        }
        for (size_t i = 0; i < ARRAY_SIZE(desc.rtv_formats); i++) {
            if (desc.rtv_formats[i] == BufferFormat::INVALID) break;
            pso_desc.RTVFormats[i] = to_d3d_format(desc.rtv_formats[i]);
            pso_desc.NumRenderTargets = i + 1;
        }

        ID3DBlob* vertex_shader = nullptr;
        ID3DBlob* pixel_shader = nullptr;
        if (desc.used_stages & PIPELINE_STAGE_VERTEX) {
            vertex_shader = compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_VERTEX);
            pso_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
            pso_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
        }
        if (desc.used_stages & PIPELINE_STAGE_PIXEL) {
            pixel_shader = compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_PIXEL);
            pso_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();
            pso_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
        }

        DXCall(g_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));

        if (vertex_shader != nullptr) vertex_shader->Release();
        if (pixel_shader != nullptr) pixel_shader->Release();

        return { g_pipelines.push_back(pipeline) };
    }
}