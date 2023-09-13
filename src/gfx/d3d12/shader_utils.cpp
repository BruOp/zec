#include "shader_utils.h"
#include "dx_utils.h"

namespace zec::rhi::dx12::shader_utils
{
    ZecResult compile_shader(const wchar* file_name, const PipelineStage stage, IDxcBlob** out_blob, std::string& errors)
    {
        Microsoft::WRL::ComPtr<IDxcUtils> dxc_utils;
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));

        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
        DXCall(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

        Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxc_include_handler;
        dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

        Microsoft::WRL::ComPtr<IDxcBlobEncoding> source_blob;
        {
            HRESULT hr = dxc_utils->LoadFile(file_name, nullptr, &source_blob);
            if (FAILED(hr))
            {
                errors = GetDXErrorStringUTF8(hr);
                return ZecResult::FAILURE;
            }
        }

        DxcBuffer source;
        source.Ptr = source_blob->GetBufferPointer();
        source.Size = source_blob->GetBufferSize();
        source.Encoding = DXC_CP_ACP;

        wchar entry[10] = L"";
        wchar target[10] = L"";
        ASSERT(stage != PIPELINE_STAGE_INVALID);
        if (stage == PIPELINE_STAGE_VERTEX) {
            lstrcpy(entry, L"VSMain");
            lstrcpy(target, L"vs_6_1");
        }
        else if (stage == PIPELINE_STAGE_PIXEL) {
            lstrcpy(entry, L"PSMain");
            lstrcpy(target, L"ps_6_1");
        }
        else if (stage == PIPELINE_STAGE_COMPUTE) {
            lstrcpy(entry, L"CSMain");
            lstrcpy(target, L"cs_6_1");
        }

        LPCWSTR dxc_args[] = {
            DXC_ARG_SKIP_OPTIMIZATIONS,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            L"-Qembed_debug",
            DXC_ARG_DEBUG,
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
        };

        Microsoft::WRL::ComPtr<IDxcCompilerArgs> compiler_args;
        DXCall(DxcCreateInstance(CLSID_DxcCompilerArgs, IID_PPV_ARGS(&compiler_args)));

        dxc_utils->BuildArguments(
            file_name,
            entry,
            target,
            dxc_args,
            _countof(dxc_args),
            nullptr,
            0,
            &compiler_args);

        Microsoft::WRL::ComPtr<IDxcResult> compile_result;
        HRESULT hr = compiler->Compile(
            &source, // pSource
            compiler_args->GetArguments(),
            compiler_args->GetCount(),
            dxc_include_handler.Get(),
            IID_PPV_ARGS(compile_result.GetAddressOf())
        ); // ppResult

        if (FAILED(hr))
        {
            errors = GetDXErrorStringUTF8(hr);
            return ZecResult::FAILURE;
        }

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        // Note that d3dcompiler would return null if no errors or warnings are present.
        // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
        if (pErrors != nullptr && pErrors->GetStringLength() != 0) {
            errors = get_string(pErrors.Get());
            return ZecResult::FAILURE;
        }

        compile_result->GetStatus(&hr);
        if (FAILED(hr))
        {
            errors = GetDXErrorStringUTF8(hr);
            return ZecResult::FAILURE;
        }

        compile_result->GetResult(out_blob);

        return ZecResult::SUCCESS;
    }
}