#pragma once
#include "gfx/rhi.h"

struct IDxcBlob;

namespace zec::rhi::dx12::shader_utils
{
    ZecResult compile_shader(const ShaderCompilationDesc& desc, const PipelineStage stage, IDxcBlob** out_blob, std::string& errors);
}
