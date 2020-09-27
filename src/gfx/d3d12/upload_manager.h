#pragma once
#include "pch.h"
#include "gfx/constants.h"
#include "gfx/public_resources.h"
#include "wrappers.h"
#include "resource_managers.h"

namespace DirectX
{
    class ScratchImage;
}

namespace zec
{
    namespace dx12
    {
        struct UploadManagerDesc
        {
            ID3D12Device* device = nullptr;
            D3D12MA::Allocator* allocator = nullptr;
            ID3D12CommandQueue* cmd_queue = nullptr;
            FenceManager* fence_manager = nullptr;
        };

        struct TextureUploadDesc
        {
            DirectX::ScratchImage* data = nullptr;
            D3D12_RESOURCE_DESC d3d_texture_desc = {};
            u64 num_subresources = 0;
            bool is_cube_map = false;
        };

        class UploadManager
        {
            struct UploadBatchInfo
            {
                u64 fence_value;
                u64 size;
            };

            struct Upload
            {
                ID3D12Resource* resource = nullptr;
                D3D12MA::Allocation* allocation = nullptr;
            };

        public:
            void init(const UploadManagerDesc& desc);
            void destroy();

            void begin_upload();
            void end_upload();

            void queue_upload(const BufferDesc& buffer_desc, ID3D12Resource* destination_resource);
            void queue_upload(const TextureUploadDesc& texture_upload_desc, Texture& texture);

            Fence fence = { };
            u64 current_fence_value = 0;

        private:
            // Not Owned
            ID3D12CommandQueue* cmd_queue = nullptr;
            D3D12MA::Allocator* allocator;
            // Owned
            ID3D12GraphicsCommandList* cmd_list = nullptr;
            ID3D12CommandAllocator* cmd_allocators[RENDER_LATENCY] = {};
            u64 current_idx;
            u64 pending_size;

            RingBuffer<Upload> upload_queue = {};
            UploadBatchInfo upload_batch_info[RENDER_LATENCY] = {};
        };
    }
}
