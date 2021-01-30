#pragma once
#include "gfx/gfx.h"
#include "dx_utils.h"
#include "dx_helpers.h"

namespace zec::gfx::dx12::shader_utils
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
