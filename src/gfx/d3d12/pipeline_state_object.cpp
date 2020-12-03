#include "pch.h"
#include "gfx/gfx.h"
#include "dx_utils.h"
#include "dx_helpers.h"
#include "globals.h"

namespace zec::gfx::dx12
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

using namespace zec::gfx::dx12;

namespace zec::gfx::pipelines
{
    ResourceLayoutHandle create_resource_layout(const ResourceLayoutDesc& desc)
    {
        constexpr u64 MAX_DWORDS_IN_RS = 64;
        ASSERT(desc.num_constants < MAX_DWORDS_IN_RS);
        ASSERT(desc.num_constant_buffers < MAX_DWORDS_IN_RS / 2);
        ASSERT(desc.num_resource_tables < MAX_DWORDS_IN_RS);

        ASSERT(desc.num_constants < ARRAY_SIZE(desc.constants));
        ASSERT(desc.num_constant_buffers < ARRAY_SIZE(desc.constant_buffers));
        ASSERT(desc.num_resource_tables < ARRAY_SIZE(desc.tables));

        i8 remaining_dwords = i8(MAX_DWORDS_IN_RS);
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
        // In reality, inline descriptors use up 2 DWORDs so the limit is actually lower;
        D3D12_ROOT_PARAMETER root_parameters[MAX_DWORDS_IN_RS];

        u64 parameter_count = 0;
        for (u32 i = 0; i < desc.num_constants; i++) {
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count];
            parameter.ShaderVisibility = to_d3d_visibility(desc.constants[i].visibility);
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameter.Constants = {
                .ShaderRegister = i,
                .RegisterSpace = 0,
                .Num32BitValues = desc.constants[i].num_constants,
            };
            remaining_dwords -= 2 * desc.constants[i].num_constants;
            parameter_count++;
        }

        for (u32 i = 0; i < desc.num_constant_buffers; i++) {
            D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count];
            parameter.ShaderVisibility = to_d3d_visibility(desc.constant_buffers[i].visibility);
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            parameter.Descriptor = {
                .ShaderRegister = desc.num_constants + i, // Since the constants will immediately precede these
                .RegisterSpace = 0,
            };
            remaining_dwords--;
            parameter_count++;
        }


        // Tables -- Note that we actually map them to ranges
        if (desc.num_resource_tables > 0) {
            ASSERT(desc.num_resource_tables < ResourceLayoutDesc::MAX_ENTRIES);

            D3D12_DESCRIPTOR_RANGE srv_ranges[ResourceLayoutDesc::MAX_ENTRIES] = {};
            D3D12_DESCRIPTOR_RANGE uav_ranges[ResourceLayoutDesc::MAX_ENTRIES] = {};
            u32 srv_count = 0;
            u32 uav_count = 0;

            D3D12_ROOT_DESCRIPTOR_TABLE srv_table = {};
            D3D12_ROOT_DESCRIPTOR_TABLE uav_table = {};

            for (u32 i = 0; i < desc.num_resource_tables; i++) {
                const auto& table_desc = desc.tables[i];
                ASSERT(table_desc.usage != ResourceAccess::UNUSED);

                auto range_type = table_desc.usage == ResourceAccess::READ ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                // Since we only support bindless, they all start at a base shader register of zero and are in a different space.
                if (table_desc.usage == ResourceAccess::READ) {

                    srv_ranges[srv_count++] = {
                        .RangeType = range_type,
                        .NumDescriptors = table_desc.count,
                        .BaseShaderRegister = 0,
                        .RegisterSpace = i + 1,
                        .OffsetInDescriptorsFromTableStart = 0
                    };
                }
                else {
                    uav_ranges[uav_count++] = {
                        .RangeType = range_type,
                        .NumDescriptors = table_desc.count,
                        .BaseShaderRegister = 0,
                        .RegisterSpace = i + 1,
                        .OffsetInDescriptorsFromTableStart = 0
                    };
                }
            }

            if (srv_count > 0) {
                for (size_t srv_table_idx = 0; srv_table_idx < srv_count; srv_table_idx++) {
                    D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count++];
                    parameter.ShaderVisibility = to_d3d_visibility(ShaderVisibility::ALL);
                    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameter.DescriptorTable = {
                        .NumDescriptorRanges = 1,
                        .pDescriptorRanges = &srv_ranges[srv_table_idx],
                    };
                    remaining_dwords--;
                }
            }

            if (uav_count > 0) {
                for (size_t uav_table_idx = 0; uav_table_idx < uav_count; uav_table_idx++) {
                    D3D12_ROOT_PARAMETER& parameter = root_parameters[parameter_count++];
                    parameter.ShaderVisibility = to_d3d_visibility(ShaderVisibility::ALL);
                    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameter.DescriptorTable = {
                        .NumDescriptorRanges = uav_count,
                        .pDescriptorRanges = &uav_ranges[uav_table_idx],
                    };
                    remaining_dwords--;
                }
            }
        }

        ASSERT(remaining_dwords > 0);
        D3D12_STATIC_SAMPLER_DESC static_sampler_descs[ARRAY_SIZE(desc.static_samplers)] = { };

        for (size_t i = 0; i < desc.num_static_samplers; i++) {
            const auto& sampler_desc = desc.static_samplers[i];
            D3D12_STATIC_SAMPLER_DESC& d3d_sampler_desc = static_sampler_descs[i];
            d3d_sampler_desc.Filter = dx12::to_d3d_filter(sampler_desc.filtering);
            d3d_sampler_desc.AddressU = dx12::to_d3d_address_mode(sampler_desc.wrap_u);
            d3d_sampler_desc.AddressV = dx12::to_d3d_address_mode(sampler_desc.wrap_v);
            d3d_sampler_desc.AddressW = dx12::to_d3d_address_mode(sampler_desc.wrap_w);
            d3d_sampler_desc.MipLODBias = 0.0f;
            d3d_sampler_desc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
            d3d_sampler_desc.MinLOD = 0.0f;
            d3d_sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
            d3d_sampler_desc.ShaderRegister = sampler_desc.binding_slot;
            d3d_sampler_desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        root_signature_desc.pParameters = root_parameters;
        root_signature_desc.NumParameters = desc.num_constants + desc.num_constant_buffers + desc.num_resource_tables;
        root_signature_desc.NumStaticSamplers = desc.num_static_samplers;
        root_signature_desc.pStaticSamplers = static_sampler_descs;
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3D12RootSignature* root_signature = nullptr;

        ID3DBlob* signature;
        ID3DBlob* error;
        HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

        if (error != nullptr) {
            print_blob(error);
            error->Release();
            throw std::runtime_error("Failed to create root signature");
        }
        DXCall(hr);

        hr = g_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature));

        if (error != nullptr) {
            print_blob(error);
            error->Release();
            throw std::runtime_error("Failed to create root signature");
        }
        DXCall(hr);
        signature != nullptr && signature->Release();

        return g_root_signatures.push_back(root_signature);
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

    void set_debug_name(ResourceLayoutHandle handle, wchar* name)
    {
        ID3D12RootSignature* root_signature = g_root_signatures[handle];
        root_signature->SetName(name);
    }

    void set_debug_name(PipelineStateHandle handle, wchar* name)
    {
        ID3D12PipelineState* pso = g_pipelines[handle];
        pso->SetName(name);
    }
}