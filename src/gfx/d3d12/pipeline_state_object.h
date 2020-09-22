#pragma once
#include "pch.h"
#include "utils/utils.h"
#include "renderer.h"
#include "gfx/public_resources.h"

namespace zec
{
    namespace dx12
    {
        ID3D12PipelineState* create_pipeline_state_object(const PipelineStateObjectDesc& desc, ID3D12Device* device, ID3D12RootSignature* root_signature);
    }
}