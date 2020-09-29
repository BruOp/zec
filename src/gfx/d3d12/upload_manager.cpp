#include "pch.h"
#include "core/zec_math.h"
#include "upload_manager.h"
#include "dx_utils.h"
#include "globals.h"
#include "DirectXTex.h"

namespace zec
{
    namespace dx12
    {
        void UploadManager::init(const UploadManagerDesc& desc)
        {
            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                DXCall(desc.device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_COPY,
                    IID_PPV_ARGS(&cmd_allocators[i])
                ));
            }
            DXCall(desc.device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COPY,
                cmd_allocators[0],
                nullptr,
                IID_PPV_ARGS(&cmd_list)
            ));
            cmd_list->Close();
            allocator = desc.allocator;
            cmd_queue = desc.cmd_queue;
            fence = create_fence(*desc.fences, 0);
        }

        void UploadManager::destroy()
        {
            wait(fence, current_fence_value);

            cmd_list->Release();
            cmd_list = nullptr;

            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                cmd_allocators[i]->Release();
                cmd_allocators[i] = nullptr;
                upload_batch_info[i] = { };
            }

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
            ASSERT(cmd_list != nullptr);

            // Check if we should discard any resources
            if (current_idx != 0) {
                UploadBatchInfo pending_upload_info = upload_batch_info[current_idx];
                wait(fence, pending_upload_info.fence_value);
                // Release all the resources we know we've finished uploading
                for (size_t i = 0; i < pending_upload_info.size; i++) {
                    Upload pending_upload = upload_queue.pop_front();
                    pending_upload.resource->Release();
                    pending_upload.allocation->Release();
                }
            }

            ID3D12CommandAllocator* cmd_allocator = cmd_allocators[current_idx];
            cmd_allocator->Reset();
            cmd_list->Reset(cmd_allocator, nullptr);
        }

        void UploadManager::end_upload()
        {
            ASSERT(cmd_list != nullptr);
            //cmd_list->ResourceBarrier(barriers.size, barriers.data);
            cmd_list->Close();

            ID3D12CommandList* command_lists[] = { cmd_list };
            cmd_queue->ExecuteCommandLists(1, command_lists);

            UploadBatchInfo& info = upload_batch_info[current_idx];
            info.fence_value = ++current_fence_value;
            info.size = pending_size;

            signal(fence, cmd_queue, current_fence_value);

            // Reset values for next frame;
            pending_size = 0;
            current_idx = (current_idx + 1) % RENDER_LATENCY;
        }

        void UploadManager::queue_upload(const BufferDesc& desc, ID3D12Resource* destination_resource)
        {
            ASSERT(desc.data != nullptr);
            // Create upload resource and schedule copy

            Upload upload{ };
            const u64 upload_buffer_size = GetRequiredIntermediateSize(destination_resource, 0, 1);
            D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
            upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            DXCall(allocator->CreateResource(
                &upload_alloc_desc,
                &CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                NULL,
                &upload.allocation,
                IID_PPV_ARGS(&upload.resource)
            ));

            upload_queue.push_back(upload);
            ++pending_size;

            void* mapped_ptr = nullptr;
            DXCall(upload.resource->Map(0, NULL, &mapped_ptr));
            memory::copy(mapped_ptr, desc.data, desc.byte_size);
            upload.resource->Unmap(0, NULL);

            cmd_list->CopyBufferRegion(destination_resource, 0, upload.resource, 0, desc.byte_size);
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
            DXCall(allocator->CreateResource(
                &upload_alloc_desc,
                &CD3DX12_RESOURCE_DESC::Buffer(mem_size),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                NULL,
                &upload.allocation,
                IID_PPV_ARGS(&upload.resource)
            ));

            upload_queue.push_back(upload);
            ++pending_size;

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

            for (u64 subresource_idx = 0; subresource_idx < texture_upload_desc.num_subresources; ++subresource_idx) {
                D3D12_TEXTURE_COPY_LOCATION dst = { };
                dst.pResource = texture.resource;
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = u32(subresource_idx);
                D3D12_TEXTURE_COPY_LOCATION src = { };
                src.pResource = upload.resource;
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = layouts[subresource_idx];
                src.PlacedFootprint.Offset += upload.allocation->GetOffset();

                cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            }
        }
    }
}
