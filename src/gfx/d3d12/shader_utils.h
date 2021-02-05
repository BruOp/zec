#pragma once
#include "gfx/gfx.h"
#include "dx_utils.h"
#include "dx_helpers.h"
#include <filesystem>
#include <dxcompiler/dxcapi.h>


namespace zec::gfx::dx12::shader_utils
{
    IDxcBlob* compile_shader(const wchar* file_name, PipelineStage stage)
    {
        Microsoft::WRL::ComPtr<IDxcUtils> dxc_utils;
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));

        //Microsoft::WRL::ComPtr<IDxcLibrary> library;
        //DXCall(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library)));

        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
        DXCall(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

        //
        // Create default include handler. (You can create your own...)
        //
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxc_include_handler;
        dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

        IDxcBlobEncoding* source_blob;
        DXCall(dxc_utils->LoadFile(file_name, nullptr, &source_blob));
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
        DXCall(hr);
        //
        // Print errors if present.
        //
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        // Note that d3dcompiler would return null if no errors or warnings are present.  
        // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
        if (pErrors != nullptr && pErrors->GetStringLength() != 0) {
            std::wstring error_string = make_string(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());
            debug_print(error_string);
        }

        if (SUCCEEDED(hr))
            compile_result->GetStatus(&hr);

        DXCall(hr);
        ////
        //// Save pdb.
        ////
        //Microsoft::WRL::ComPtr<IDxcBlob> pdb = nullptr;
        //Microsoft::WRL::ComPtr<IDxcBlobUtf16> pdb_name = nullptr;
        //compile_result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdb_name);
        //{
        //    FILE* fp = NULL;
        //    std::filesystem::path file_path{ L"../.build/bin/Debug/x64/clustered_forward/" };
        //    file_path /= pdb_name->GetStringPointer();
        //    // Note that if you don't specify -Fd, a pdb name will be automatically generated. Use this file name to save the pdb so that PIX can find it quickly.
        //    _wfopen_s(&fp, file_path.c_str(), L"wb");
        //    fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
        //    fclose(fp);
        //}

        IDxcBlob* code;
        compile_result->GetResult(&code);

        source_blob->Release();

        return code;
    }
}
