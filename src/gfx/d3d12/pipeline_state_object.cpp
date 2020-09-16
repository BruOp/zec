#include "pch.h"
#include "pipeline_state_object.h"

namespace zec
{
    namespace dx12
    {

        D3D12_FILL_MODE to_d3d_fill_mode(const FillMode fill_mode)
        {
            switch (fill_mode) {
            case FillMode::WIREFRAME:
                return D3D12_FILL_MODE_WIREFRAME;
            case FillMode::SOLID:
            default:
                return D3D12_FILL_MODE_SOLID;
            }
        }

        D3D12_CULL_MODE to_d3d_cull_mode(const CullMode cull_mode)
        {
            switch (cull_mode) {
            case CullMode::FRONT_CW:
            case CullMode::FRONT_CCW:
                return D3D12_CULL_MODE_FRONT;
            case CullMode::BACK_CW:
            case CullMode::BACK_CCW:
                return D3D12_CULL_MODE_BACK;
            default:
                return D3D12_CULL_MODE_NONE;
            }
        }

        DXGI_FORMAT to_d3d_format(const BufferFormat format)
        {
            switch (format) {
            case BufferFormat::UINT16:
                return DXGI_FORMAT_R16_UINT;
            case BufferFormat::UINT32:
                return DXGI_FORMAT_R32_UINT;
            case BufferFormat::UNORM8_2:
                return DXGI_FORMAT_R8G8_UNORM;
            case BufferFormat::UNORM16_2:
                return DXGI_FORMAT_R16G16_UNORM;
            case BufferFormat::FLOAT_2:
                return DXGI_FORMAT_R32G32_FLOAT;
            case BufferFormat::FLOAT_3:
                return DXGI_FORMAT_R32G32B32_FLOAT;
            case BufferFormat::UINT8_4:
                //case BufferFormat::R8G8B8A8_UINT:              
                return DXGI_FORMAT_R8G8B8A8_UINT;
            case BufferFormat::UNORM8_4:
                //case BufferFormat::R8G8B8A8_UNORM:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case BufferFormat::UINT16_4:
                return DXGI_FORMAT_R16G16B16A16_UINT;
            case BufferFormat::UNORM16_4:
                return DXGI_FORMAT_R16G16B16A16_UNORM;
            case BufferFormat::HALF_4:
                return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case BufferFormat::FLOAT_4:
                return DXGI_FORMAT_R32G32B32A32_FLOAT;

            case BufferFormat::UNORM8_4_SRGB:
                //case BufferFormat::R8G8B8A8_UNORM_SRGB:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            case BufferFormat::INVALID:
            default:
                throw std::runtime_error("Cannot transform invalid format to DXGI format");
            }
        }

        D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(const RasterStateDesc& desc)
        {
            BOOL cull_ccw = (desc.cull_mode == CullMode::FRONT_CCW || desc.cull_mode == CullMode::BACK_CCW);
            D3D12_RASTERIZER_DESC rasterizer_desc = { };
            rasterizer_desc.FillMode = to_d3d_fill_mode(desc.fill_mode);
            rasterizer_desc.CullMode = to_d3d_cull_mode(desc.cull_mode);
            rasterizer_desc.FrontCounterClockwise = cull_ccw;
            rasterizer_desc.DepthBias = desc.depth_bias;
            rasterizer_desc.DepthBiasClamp = desc.depth_bias_clamp;
            rasterizer_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
            rasterizer_desc.DepthClipEnable = desc.flags & RasterizerFlags::DEPTH_CLIP_ENABLED;
            rasterizer_desc.MultisampleEnable = desc.flags & RasterizerFlags::MULTISAMPLE;
            rasterizer_desc.AntialiasedLineEnable = desc.flags & RasterizerFlags::ANTIALIASED_LINES;
            rasterizer_desc.ForcedSampleCount = 0;
            rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
            return rasterizer_desc;
        };

        inline D3D12_BLEND to_d3d_blend(const Blend blend)
        {
            switch (blend) {
            case (Blend::ZERO):
                return D3D12_BLEND_ZERO;
            case (Blend::ONE):
                return D3D12_BLEND_ONE;
            case (Blend::SRC_COLOR):
                return D3D12_BLEND_SRC_COLOR;
            case (Blend::INV_SRC_COLOR):
                return D3D12_BLEND_INV_SRC_COLOR;
            case (Blend::SRC_ALPHA):
                return D3D12_BLEND_SRC_ALPHA;
            case (Blend::INV_SRC_ALPHA):
                return D3D12_BLEND_INV_SRC_ALPHA;
            case (Blend::DEST_ALPHA):
                return D3D12_BLEND_DEST_ALPHA;
            case (Blend::INV_DEST_ALPHA):
                return D3D12_BLEND_INV_DEST_ALPHA;
            case (Blend::DEST_COLOR):
                return D3D12_BLEND_DEST_COLOR;
            case (Blend::INV_DEST_COLOR):
                return D3D12_BLEND_INV_DEST_COLOR;
            case (Blend::SRC_ALPHA_SAT):
                return D3D12_BLEND_SRC_ALPHA_SAT;
            case (Blend::BLEND_FACTOR):
                return D3D12_BLEND_BLEND_FACTOR;
            case (Blend::INV_BLEND_FACTOR):
                return D3D12_BLEND_INV_BLEND_FACTOR;
            case (Blend::SRC1_COLOR):
                return D3D12_BLEND_SRC1_COLOR;
            case (Blend::INV_SRC1_COLOR):
                return D3D12_BLEND_INV_SRC1_COLOR;
            case (Blend::SRC1_ALPHA):
                return D3D12_BLEND_SRC1_ALPHA;
            case (Blend::INV_SRC1_ALPHA):
                return D3D12_BLEND_INV_SRC1_ALPHA;
            default:
                throw std::runtime_error("Cannot convert invalid blend type");
            }
        }

        D3D12_BLEND_OP to_d3d_blend_op(const BlendOp blend_op)
        {
            switch (blend_op) {
            case (BlendOp::ADD):
                return D3D12_BLEND_OP_ADD;
            case (BlendOp::SUBTRACT):
                return D3D12_BLEND_OP_SUBTRACT;
            case (BlendOp::REV_SUBTRACT):
                return D3D12_BLEND_OP_REV_SUBTRACT;
            case (BlendOp::MIN):
                return D3D12_BLEND_OP_MIN;
            case (BlendOp::MAX):
                return D3D12_BLEND_OP_MAX;
            default:
                throw std::runtime_error("Cannot convert invalid blend op");
            }
        }

        D3D12_RENDER_TARGET_BLEND_DESC  to_d3d_blend_desc(const RenderTargetBlendDesc& desc)
        {
            //d3d12_blend_mode
            return  D3D12_RENDER_TARGET_BLEND_DESC{
                desc.blend_mode == BlendMode::OPAQUE, FALSE,
                to_d3d_blend(desc.src_blend), to_d3d_blend(desc.dst_blend), to_d3d_blend_op(desc.blend_op),
                to_d3d_blend(desc.src_blend_alpha), to_d3d_blend(desc.dst_blend_alpha), to_d3d_blend_op(desc.blend_op_alpha),
                D3D12_LOGIC_OP_NOOP,
                D3D12_COLOR_WRITE_ENABLE_ALL
            };
        }

        D3D12_COMPARISON_FUNC to_d3d_comparison_func(const ComparisonFunc comparison_func)
        {
            switch (comparison_func) {
            case ComparisonFunc::LESS:
                return D3D12_COMPARISON_FUNC_LESS;
            case ComparisonFunc::LESS_OR_EQUAL:
                return D3D12_COMPARISON_FUNC_LESS_EQUAL;
            case ComparisonFunc::EQUAL:
                return D3D12_COMPARISON_FUNC_EQUAL;
            case ComparisonFunc::GREATER_OR_EQUAL:
                return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            case ComparisonFunc::GREATER:
                return D3D12_COMPARISON_FUNC_GREATER;
            case ComparisonFunc::ALWAYS:
                return D3D12_COMPARISON_FUNC_ALWAYS;
            case ComparisonFunc::NEVER:
            default:
                return D3D12_COMPARISON_FUNC_NEVER;
            }
        }

        D3D12_STENCIL_OP to_d3d_stencil_op(const StencilOp stencil_op)
        {
            switch (stencil_op) {
            case StencilOp::KEEP:
                return D3D12_STENCIL_OP_KEEP;
            case StencilOp::ZERO:
                return D3D12_STENCIL_OP_ZERO;
            case StencilOp::REPLACE:
                return D3D12_STENCIL_OP_REPLACE;
            case StencilOp::INCR_SAT:
                return D3D12_STENCIL_OP_INCR_SAT;
            case StencilOp::DECR_SAT:
                return D3D12_STENCIL_OP_DECR_SAT;
            case StencilOp::INVERT:
                return D3D12_STENCIL_OP_INVERT;
            case StencilOp::INCR:
                return D3D12_STENCIL_OP_INCR;
            case StencilOp::DECR:
                return D3D12_STENCIL_OP_DECR;
            default:
                return D3D12_STENCIL_OP_ZERO;
            }
        }

        D3D12_FILL_MODE getD3DFillMode(const FillMode mode)
        {
            switch (mode) {
            case FillMode::WIREFRAME:
                return D3D12_FILL_MODE_WIREFRAME;
            case FillMode::SOLID:
            default:
                return D3D12_FILL_MODE_SOLID;
            }
        }

        D3D12_DEPTH_STENCILOP_DESC to_d3d_stencil_op_desc(const StencilFuncDesc& func_desc)
        {
            return D3D12_DEPTH_STENCILOP_DESC{
                to_d3d_stencil_op(func_desc.on_stencil_fail),
                to_d3d_stencil_op(func_desc.on_stencil_pass_depth_fail),
                to_d3d_stencil_op(func_desc.on_stencil_pass_depth_pass),
                to_d3d_comparison_func(func_desc.comparison_func),
            };
        }

        D3D12_DEPTH_STENCIL_DESC to_d3d_depth_stencil_desc(const DepthStencilDesc& depthDesc)
        {
            D3D12_DEPTH_STENCIL_DESC desc = CD3DX12_DEPTH_STENCIL_DESC{};
            desc.DepthEnable = depthDesc.depth_cull_mode != ComparisonFunc::OFF;
            desc.DepthFunc = to_d3d_comparison_func(depthDesc.depth_cull_mode);

            desc.DepthWriteMask = depthDesc.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.StencilEnable = depthDesc.stencil_enabled;
            desc.StencilReadMask = depthDesc.stencil_read_mask;
            desc.StencilWriteMask = depthDesc.stencil_write_mask;
            desc.FrontFace = to_d3d_stencil_op_desc(depthDesc.stencil_front_face);
            desc.BackFace = to_d3d_stencil_op_desc(depthDesc.stencil_back_face);

            return desc;
        }

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

        ID3D12PipelineState* create_pipeline_state_object(const PipelineStateObjectDesc& desc, ID3D12Device* device, ID3D12RootSignature* root_signature)
        {
            ASSERT(is_valid(desc.resource_layout));

            ID3D12PipelineState* pipeline = nullptr;

            if (desc.used_stages & PIPELINE_STAGE_COMPUTE) {
                ASSERT(desc.used_stages == PIPELINE_STAGE_COMPUTE);

                ID3DBlob* compute_shader = compile_shader(desc.shader_file_path.c_str(), PIPELINE_STAGE_COMPUTE);

                D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
                pso_desc.CS.BytecodeLength = compute_shader->GetBufferSize();
                pso_desc.CS.pShaderBytecode = compute_shader->GetBufferPointer();
                pso_desc.pRootSignature = root_signature;
                DXCall(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));
                compute_shader->Release();

                return pipeline;
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
            pso_desc.NumRenderTargets = 1;
            pso_desc.SampleDesc.Count = 1;

            for (size_t i = 0; i < ARRAY_SIZE(desc.rtv_formats); i++) {
                if (desc.rtv_formats[i] == BufferFormat::INVALID) break;
                pso_desc.RTVFormats[i] = to_d3d_format(desc.rtv_formats[i]);
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

            DXCall(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline)));

            if (vertex_shader != nullptr) vertex_shader->Release();
            if (pixel_shader != nullptr) pixel_shader->Release();

            return pipeline;
        }
    }
}