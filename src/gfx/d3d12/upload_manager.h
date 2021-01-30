//#pragma once
//#include "pch.h"
//#include "core/ring_buffer.h"
//#include "gfx/constants.h"
//#include "gfx/public_resources.h"
//#include "wrappers.h"
//#include "textures.h"
//#include "buffers.h"
//
//
//namespace DirectX
//{
//    class ScratchImage;
//}
//
//namespace zec::gfx::dx12
//{
//    struct UploadManagerDesc
//    {
//        CommandQueueType type;
//        ID3D12Device* device;
//        D3D12MA::Allocator* allocator;
//    };
//
//    struct TextureUploadDesc
//    {
//        DirectX::ScratchImage* data = nullptr;
//        D3D12_RESOURCE_DESC d3d_texture_desc = {};
//        u64 num_subresources = 0;
//        bool is_cube_map = false;
//    };
//
//    class UploadManager
//    {
//        struct UploadBatchInfo
//        {
//            u64 fence_value;
//            u64 size;
//        };
//
//        struct Upload
//        {
//            ID3D12Resource* resource = nullptr;
//            D3D12MA::Allocation* allocation = nullptr;
//        };
//
//    public:
//        UploadManager() = default;
//        ~UploadManager();
//
//        void init(const UploadManagerDesc& desc);
//        void destroy();
//
//        void begin_upload();
//        CmdReceipt end_upload();
//
//        void queue_upload(const BufferDesc& buffer_desc, ID3D12Resource* destination_resource);
//        void queue_upload(const TextureUploadDesc& texture_upload_desc, Texture& texture);
//
//    private:
//
//        CommandQueueType type = CommandQueueType::COPY;
//        // Non-owning
//        ID3D12Device* device;
//        D3D12MA::Allocator* allocator;
//
//        CommandContextHandle cmd_ctx = {};
//
//        // Owned
//        u64 num_pending_resources;
//
//        static constexpr u64 MAX_IN_FLIGHT_UPLOAD_BATCHES = 8; // Totally arbitrary
//        RingBuffer<Upload> upload_queue = {};
//        FixedRingBuffer<UploadBatchInfo, MAX_IN_FLIGHT_UPLOAD_BATCHES> upload_batch_info = {};
//    };
//}
