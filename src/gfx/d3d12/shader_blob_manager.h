#pragma once
#include "./core/array.h"
#include "./core/zec_types.h"

#include "../resource_array.h"
#include "../public_resources.h"

struct IDxcBlob;

namespace zec::gfx::dx12
{
    struct  CompiledShaderBlobs
    {
        struct Blob
        {
            void* blob = nullptr;

            operator bool() const
            {
                return blob != nullptr;
            }
        };

        Blob vertex_shader = {};
        Blob pixel_shader = {};
        Blob compute_shader = {};
    };

    IDxcBlob* cast_blob(CompiledShaderBlobs::Blob blob);

    class ShaderBlobsManager
    {
    public:
        UNCOPIABLE(ShaderBlobsManager);
        UNMOVABLE(ShaderBlobsManager);

        ShaderBlobsManager() = default;
        ~ShaderBlobsManager();

        const CompiledShaderBlobs* get_blobs(const ShaderBlobsHandle handle) const;

        ShaderBlobsHandle store_blobs(CompiledShaderBlobs&& blob);

        void release_blobs(ShaderBlobsHandle& handle);

    private:
        Array<uint32_t> free_list = {};
        ResourceArray<CompiledShaderBlobs, ShaderBlobsHandle> shader_blobs = {};
    };
}