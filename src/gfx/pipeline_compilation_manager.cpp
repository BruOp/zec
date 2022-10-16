#include "pipeline_compilation_manager.h"

#include <wchar.h>
#include <string>

namespace zec::gfx
{
    PipelineCompilationManager::PipelineCompilationManager()
    {

    }

    PipelineCompilationManager::~PipelineCompilationManager()
    {
    }

    PipelineCompilationHandle PipelineCompilationManager::create_pipeline(
        const wchar* name,
        const ShaderCompilationDesc& shader_blob_desc,
        const ResourceLayoutDesc& resource_layout_desc,
        const PipelineStateObjectDesc& pipeline_desc,
        std::string& inout_errors)
    {
        PipelineCompilationHandle out_handle{};
        ShaderBlobsHandle blobs_handle{};
        ZecResult res = gfx::shader_compilation::compile_shaders(shader_blob_desc, blobs_handle, inout_errors);

        if (res != ZecResult::SUCCESS)
        {
            return out_handle;
        }
        ResourceLayoutHandle resource_layout_id = gfx::pipelines::create_resource_layout(resource_layout_desc);
        PipelineStateHandle pipeline_id = gfx::pipelines::create_pipeline_state_object(blobs_handle, resource_layout_id, pipeline_desc, name);

        // Success!
        // Clean up after ourselves
        gfx::shader_compilation::release_blobs(blobs_handle);

        // Save this stuff for future recompilation
        out_handle.idx = descs.size;
        ASSERT(descs.size == handles.size);
        descs.push_back(
            {
                .blobs_desc = shader_blob_desc,
                .resource_layout_desc = resource_layout_desc,
                .pipeline_desc = pipeline_desc,
            });
        handles.push_back({
            .resource_layout_handle = resource_layout_id,
            .pso_handle = pipeline_id,
            });
        ASSERT(descs.size == handles.size);

        return out_handle;
    }

    ZecResult PipelineCompilationManager::recompile_pipeline(const wchar* name, const PipelineCompilationHandle compilation_handle, std::string& inout_errors)
    {
        ASSERT(compilation_handle.idx < descs.size);
        ASSERT(compilation_handle.idx < handles.size);

        const auto& aggregate_desc = descs[compilation_handle.idx];
        ShaderBlobsHandle blobs_handle{};
        ZecResult res = gfx::shader_compilation::compile_shaders(aggregate_desc.blobs_desc, blobs_handle, inout_errors);

        if (res == ZecResult::SUCCESS)
        {
            gfx::shader_compilation::release_blobs(blobs_handle);

            // Recompile pipeline id
            const ResourceLayoutHandle resource_layout_handle = handles[compilation_handle.idx].resource_layout_handle;
            const PipelineStateHandle old_pipeline_id = handles[compilation_handle.idx].pso_handle;

            // TODO: do we need to make this whole process async? How would that work?
            res = gfx::pipelines::recreate_pipeline_state_object(blobs_handle, resource_layout_handle, aggregate_desc.pipeline_desc, name, old_pipeline_id);
        }

        return res;
    }
}