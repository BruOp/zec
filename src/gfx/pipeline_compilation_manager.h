#pragma once

#include "gfx.h"
#include "core/array.h"
#include <string.h>

namespace zec::gfx
{
    RESOURCE_HANDLE(PipelineCompilationHandle);

    class PipelineCompilationManager
    {
        struct RecompilationData
        {
            ShaderCompilationDesc blobs_desc;
            ResourceLayoutDesc resource_layout_desc;
            PipelineStateObjectDesc pipeline_desc;
        };

        struct Handles
        {
            ResourceLayoutHandle resource_layout_handle;
            PipelineStateHandle pso_handle;
        };

    public:
        PipelineCompilationManager();
        ~PipelineCompilationManager();

        PipelineCompilationHandle create_pipeline(
            const wchar* name,
            const ShaderCompilationDesc& shader_blob_desc,
            const ResourceLayoutDesc& resource_layout_desc,
            const PipelineStateObjectDesc& pipeline_desc,
            std::string& inout_errors);

        ZecResult recompile_pipeline(const wchar* name, const PipelineCompilationHandle index, std::string& inout_errors);
    private:
        Array<RecompilationData> descs;
        Array<Handles> handles;
    };
}