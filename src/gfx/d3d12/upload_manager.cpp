#include "pch.h"
#include "core/zec_math.h"
#include "upload_manager.h"
#include "dx_utils.h"
#include "globals.h"
#include "command_context.h"
#include "DirectXTex.h"

namespace zec::gfx::dx12
{
    UploadManager::~UploadManager()
    {
        ASSERT(num_pending_resources == 0);
        ASSERT(upload_queue.size() == 0);
    }

    void UploadManager::init(const UploadManagerDesc& desc)
    {
        type = desc.type;
        device = desc.device;
        allocator = desc.allocator;
    }

    void UploadManager::destroy()
    {
        gfx::cmd::flush_queue(CommandQueueType::COPY);

        size_t size = upload_queue.size();
        for (size_t i = 0; i < size; i++) {
            Upload upload = upload_queue.pop_front();
            upload.resource->Release();
            upload.allocation->Release();
            upload = {};
        }
        // Fence should get cleaned up by Fence Manager
        // TODO: Add interface to "return" fences and other resources
    }

    void UploadManager::begin_upload()
    {
        // Check if we should discard any resources
        // TODO: Consider moving this to a separate method that gets run once per frame?

        size_t batch_idx = 0;
        while (upload_batch_info.size() > 0) {
            UploadBatchInfo pending_batch_info = upload_batch_info.front();
            CmdReceipt receipt = { .queue_type = type, .fence_value = pending_batch_info.fence_value };
            if (gfx::cmd::check_status(receipt)) {
                upload_batch_info.pop_front();
                // Release all the resources we know we've finished uploading
                for (size_t i = 0; i < pending_batch_info.size; i++) {
                    Upload pending_upload = upload_queue.pop_front();
                    pending_upload.resource->Release();
                    pending_upload.allocation->Release();
                }
            }
            else {
                break;
            };
        }

        cmd_ctx = gfx::cmd::provision(type);
    }

    CmdReceipt UploadManager::end_upload()
    {
        ASSERT(is_valid(cmd_ctx));
        CmdReceipt receipt = gfx::cmd::return_and_execute(&cmd_ctx, 1);


        upload_batch_info.push_back({ .fence_value = receipt.fence_value, .size = num_pending_resources });

        // Reset values for next upload;
        cmd_ctx = INVALID_HANDLE;
        num_pending_resources = 0;

        return receipt;
    }

    void UploadManager::queue_upload(const BufferDesc& desc, ID3D12Resource* destination_resource)
    {
        ASSERT(desc.data != nullptr);
        // Create upload resource and schedule copy

        Upload upload{ };
        const u64 upload_buffer_size = GetRequiredIntermediateSize(destination_resource, 0, 1);
        D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
        D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
        upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        DXCall(allocator->CreateResource(
            &upload_alloc_desc,
            &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &upload.allocation,
            IID_PPV_ARGS(&upload.resource)
        ));

        upload_queue.push_back(upload);
        ++num_pending_resources;

        void* mapped_ptr = nullptr;
        DXCall(upload.resource->Map(0, NULL, &mapped_ptr));
        memory::copy(mapped_ptr, desc.data, desc.byte_size);
        upload.resource->Unmap(0, NULL);

        cmd_utils::get_command_list(cmd_ctx)->CopyBufferRegion(destination_resource, 0, upload.resource, 0, desc.byte_size);
    }

    void UploadManager::queue_upload(const TextureUploadDesc& texture_upload_desc, Texture& texture)
    {

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * texture_upload_desc.num_subresources);
        u32* num_rows = (u32*)_alloca(sizeof(u32) * texture_upload_desc.num_subresources);
        u64* row_sizes = (u64*)_alloca(sizeof(u64) * texture_upload_desc.num_subresources);

        u64 mem_size = 0;
        const D3D12_RESOURCE_DESC& d3d_desc = texture_upload_desc.d3d_texture_desc;

        g_device->GetCopyableFootprints(
            &d3d_desc,
            0,
            u32(texture_upload_desc.num_subresources),
            0,
            layouts, num_rows, row_sizes, &mem_size);

        // Get a GPU upload buffer
        Upload upload{ };
        D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
        upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(mem_size);
        DXCall(allocator->CreateResource(
            &upload_alloc_desc,
            &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            &upload.allocation,
            IID_PPV_ARGS(&upload.resource)
        ));

        upload_queue.push_back(upload);
        ++num_pending_resources;

        void* mapped_ptr = nullptr;
        DXCall(upload.resource->Map(0, NULL, &mapped_ptr));

        for (u64 array_idx = 0; array_idx < texture.info.array_size; ++array_idx) {

            for (u64 mip_idx = 0; mip_idx < texture.info.num_mips; ++mip_idx) {
                const u64 subresource_idx = mip_idx + (array_idx * texture.info.num_mips);

                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subresource_layout = layouts[subresource_idx];
                const u64 subresource_height = num_rows[subresource_idx];
                const u64 subresource_pitch = subresource_layout.Footprint.RowPitch;
                const u64 subresource_depth = subresource_layout.Footprint.Depth;
                u8* dst_subresource_mem = reinterpret_cast<u8*>(mapped_ptr) + subresource_layout.Offset;

                for (u64 z = 0; z < subresource_depth; ++z) {
                    const DirectX::Image* sub_image = texture_upload_desc.data->GetImage(mip_idx, array_idx, z);
                    ASSERT(sub_image != nullptr);
                    const u8* src_subresource_mem = sub_image->pixels;

                    for (u64 y = 0; y < subresource_height; ++y) {
                        memcpy(dst_subresource_mem, src_subresource_mem, zec::min(subresource_pitch, sub_image->rowPitch));
                        dst_subresource_mem += subresource_pitch;
                        src_subresource_mem += sub_image->rowPitch;
                    }
                }
            }
        }

        ID3D12GraphicsCommandList* cmd_list = cmd_utils::get_command_list(cmd_ctx);
        for (u64 subresource_idx = 0; subresource_idx < texture_upload_desc.num_subresources; ++subresource_idx) {
            D3D12_TEXTURE_COPY_LOCATION dst = { };
            dst.pResource = texture.resource;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = u32(subresource_idx);
            D3D12_TEXTURE_COPY_LOCATION src = { };
            src.pResource = upload.resource;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts[subresource_idx];

            cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }
}
