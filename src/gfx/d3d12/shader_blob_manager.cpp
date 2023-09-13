#include "./shader_blob_manager.h"

#include <d3dx12/d3dx12.h>

#include <d3dcommon.h>

namespace zec::rhi::dx12
{
    IDxcBlob* cast_blob(CompiledShaderBlobs::Blob blob)
    {
        return reinterpret_cast<IDxcBlob*>(blob.blob);
    }

    void release_blobs(CompiledShaderBlobs& blobs)
    {
        if (blobs.vertex_shader)
        {
            reinterpret_cast<ID3DBlob*>(blobs.vertex_shader.blob)->Release();
            blobs.vertex_shader.blob = nullptr;
        }
        if (blobs.pixel_shader)
        {
            reinterpret_cast<ID3DBlob*>(blobs.pixel_shader.blob)->Release();
            blobs.pixel_shader.blob = nullptr;
        }
        if (blobs.compute_shader)
        {
            reinterpret_cast<ID3DBlob*>(blobs.compute_shader.blob)->Release();
            blobs.compute_shader.blob = nullptr;
        }
    }

    ShaderBlobsManager::~ShaderBlobsManager()
    {
        for (auto& blobs : shader_blobs)
        {
            ASSERT_MSG(!blobs.compute_shader && !blobs.pixel_shader && !blobs.vertex_shader, "You forgot to release shader blobs after creating them");
        }
    }

    const CompiledShaderBlobs* ShaderBlobsManager::get_blobs(const ShaderBlobsHandle handle) const
    {
        ASSERT(is_valid(handle));
        return &shader_blobs[handle];
    }

    ShaderBlobsHandle ShaderBlobsManager::store_blobs(CompiledShaderBlobs&& blob)
    {
        ShaderBlobsHandle handle{};
        if (free_list.size > 1)
        {
            handle.idx = free_list.pop_back();
            shader_blobs[handle] = blob;
        }
        else
        {
            handle.idx = shader_blobs.push_back(blob);
        }
        return handle;
    }

    void ShaderBlobsManager::release_blobs(ShaderBlobsHandle& handle)
    {
        ASSERT(is_valid(handle));
        ASSERT(
            shader_blobs[handle].compute_shader ||
            shader_blobs[handle].pixel_shader ||
            shader_blobs[handle].vertex_shader);
        dx12::release_blobs(shader_blobs[handle]);
        free_list.push_back(handle.idx);
        handle = INVALID_HANDLE;
    }
}