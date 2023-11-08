#pragma once
#include <unordered_map>
#include <d3d12.h>

#include "core/ring_buffer.h"
#include "gfx/rhi_public_resources.h"

namespace D3D12MA
{
    class Allocation;
}

namespace DirectX
{
    class ScratchImage;
}

namespace std
{
    template<>
    struct hash<zec::rhi::CommandContextHandle>
    {
        size_t operator()(const zec::rhi::CommandContextHandle& handle) const noexcept
        {
            return size_t(handle.idx);
        }
    };

}

namespace zec::rhi::dx12
{
    class UploadContextStore
    {

    public:
        struct Upload
        {
            ID3D12Resource* resource = nullptr;
            D3D12MA::Allocation* allocation = nullptr;
        };

        UploadContextStore() = default;
        ~UploadContextStore()
        {
            ASSERT(upload_contexts.size() == 0);
        };

        // Only call after a flush
        void destroy();

        inline void push(const CommandContextHandle handle, ID3D12Resource* resource, D3D12MA::Allocation* allocation)
        {
            upload_contexts[handle].push_back({ resource, allocation });
        };

        inline bool has_staged_uploads(const CommandContextHandle cmd_ctx) const
        {
            return upload_contexts.contains(cmd_ctx);
        }

        inline VirtualArray<Upload>* get_staged_uploads(const CommandContextHandle cmd_ctx)
        {
            return &upload_contexts[cmd_ctx];
        }

        void clear_staged_uploads(const CommandContextHandle handle)
        {
            upload_contexts.erase(handle);
        };

    private:
        std::unordered_map<CommandContextHandle, VirtualArray<Upload>> upload_contexts;
    };
}
