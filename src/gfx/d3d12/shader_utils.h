#pragma once
#include <filesystem>
#include <wrl.h>
#include <dxcompiler/dxcapi.h>

#include "gfx/rhi.h"
#include "dx_helpers.h"

namespace zec::rhi::dx12::shader_utils
{
    ZecResult compile_shader(const wchar* file_name, const PipelineStage stage, IDxcBlob** out_blob, std::string& errors);
}
