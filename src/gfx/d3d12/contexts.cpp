#include "pch.h"
#include "contexts.h"

namespace zec
{
    namespace dx12
    {
        void UploadManager::init(const UploadContextDesc& desc)
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

        void UploadManager::begin_upload()
        {
            ASSERT(cmd_list != nullptr);

            // Check if we should discard any resources
            if (current_idx != 0) {
                PendingUploadInfo pending_upload_info = pending_upload_infos[current_idx];
                wait(fence, pending_upload_info.fence_value);
                // Release all the resources we know we've finished uploading
                for (size_t i = pending_upload_info.start_idx; i < pending_upload_info.start_idx + pending_upload_info.size; i++) {
                    PendingUpload pending_upload = pending_uploads.pop_front();
                    pending_upload.resource->Release();
                    pending_upload.allocation->Release();
                }
            }

            ID3D12CommandAllocator* cmd_allocator = cmd_allocators[current_idx];
            cmd_allocator->Reset();
            cmd_list->Reset(cmd_allocator, nullptr);
        }

        void UploadManager::queue_upload(const BufferDesc& desc, ID3D12Resource* destination_resource)
        {
            ASSERT(desc.data != nullptr);
            // Create upload resource and schedule copy

            PendingUpload upload{ };
            const u64 upload_buffer_size = GetRequiredIntermediateSize(destination_resource, 0, 1);
            D3D12MA::ALLOCATION_DESC upload_alloc_desc = {};
            upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            DXCall(allocator->CreateResource(
                &upload_alloc_desc,
                &CD3DX12_RESOURCE_DESC::Buffer(desc.byte_size),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                NULL,
                &upload.allocation,
                IID_PPV_ARGS(&upload.resource)
            ));

            pending_uploads.push_back(upload);
            ++pending_size;

            void* mapped_ptr = nullptr;
            DXCall(upload.resource->Map(0, NULL, &mapped_ptr));
            memory::copy(mapped_ptr, desc.data, desc.byte_size);
            upload.resource->Unmap(0, NULL);

            cmd_list->CopyBufferRegion(destination_resource, 0, upload.resource, 0, desc.byte_size);
        }

        void UploadManager::end_upload()
        {
            ASSERT(cmd_list != nullptr);
            //cmd_list->ResourceBarrier(barriers.size, barriers.data);
            cmd_list->Close();

            ID3D12CommandList* command_lists[] = { cmd_list };
            cmd_queue->ExecuteCommandLists(1, command_lists);

            PendingUploadInfo& info = pending_upload_infos[current_idx];
            info.fence_value = ++current_fence_value;
            info.size = pending_size;
            info.start_idx = pending_start;

            signal(fence, cmd_queue, current_fence_value);

            // Reset values for next frame;
            pending_start += pending_size;
            pending_size = 0;
            current_idx = (current_idx + 1) % RENDER_LATENCY;
        }

        void UploadManager::destroy()
        {
            wait(fence, current_fence_value);

            cmd_list->Release();
            cmd_list = nullptr;

            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                cmd_allocators[i]->Release();
                cmd_allocators[i] = nullptr;
                pending_upload_infos[i] = { };
            }

            for (size_t i = 0; i < pending_uploads.size(); i++) {
                PendingUpload upload = pending_uploads.pop_front();
                upload.resource->Release();
                upload.allocation->Release();
                upload = {};
            }
            // Fence should get cleaned up by Fence Manager
            // TODO: Add interface to "return" fences and other resources
        }
    }
}
