#pragma once
#include "pch.h"
#include "gfx/constants.h"
#include "gfx/public.h"
#include "wrappers.h"
#include "resource_managers.h"

namespace zec
{
    namespace dx12
    {
        struct UploadContextDesc
        {
            ID3D12Device* device = nullptr;
            D3D12MA::Allocator* allocator = nullptr;
            ID3D12CommandQueue* cmd_queue = nullptr;
            FenceManager* fence_manager = nullptr;
        };


        class UploadManager
        {
            struct PendingUploadInfo
            {
                u64 fence_value;
                u64 start_idx;
                u64 size;
            };

            struct PendingUpload
            {
                ID3D12Resource* resource = nullptr;
                D3D12MA::Allocation* allocation = nullptr;
            };

        public:
            void init(const UploadContextDesc& desc);
            void destroy();

            void begin_upload();
            void queue_upload(const BufferDesc& buffer_desc, ID3D12Resource* destination_resource);
            void end_upload();

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
            u64 pending_start;

            RingBuffer<PendingUpload> pending_uploads = {};
            PendingUploadInfo pending_upload_infos[RENDER_LATENCY] = {};
        };

        struct GraphicsContext
        {

        };

        struct ComputeContext
        {

        };
    }
}
