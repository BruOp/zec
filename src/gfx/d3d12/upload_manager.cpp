#include "pch.h"
#include "upload_manager.h"
#include "dx_utils.h"
#include "globals.h"

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
            fence = desc.fence_manager->create_fence(0);
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

        void UploadManager::queue_upload(const TextureUploadDesc& texture_upload_desc, ID3D12Resource* resource)
        {
            const size_t upload_buffer_size = GetRequiredIntermediateSize(resource, 0, static_cast<u32>(texture_upload_desc.num_subresources));

            Upload upload{ };
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

            UpdateSubresources(
                cmd_list,
                resource,
                upload.resource,
                0,
                0,
                static_cast<u32>(texture_upload_desc.num_subresources),
                texture_upload_desc.subresources
            );

            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                resource,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            g_cmd_list->ResourceBarrier(1, &barrier);
        }
    }
}
