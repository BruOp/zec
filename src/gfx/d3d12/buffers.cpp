#include "pch.h"
#include "gfx/gfx.h"
#include "globals.h"
#include "dx_utils.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "resources.h"

namespace zec::gfx::dx12
{
    void init(Buffer& buffer, const BufferDesc& desc, D3D12MA::Allocator* allocator)
    {
        ASSERT(desc.byte_size != 0);
        ASSERT(desc.usage != u16(RESOURCE_USAGE_UNUSED));

        buffer.size = desc.byte_size;
        // Create Buffer
        D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_STATES initial_resource_state = D3D12_RESOURCE_STATE_COMMON;

        u16 vertex_or_index = u16(RESOURCE_USAGE_VERTEX) | u16(RESOURCE_USAGE_INDEX);
        // There are three different sizes to keep track of here:
        // desc.bytes_size  => the minimum size (in bytes) that we desire our buffer to occupy
        // buffer.size      => the above size that's been potentially grown to match alignment requirements
        // alloc_size       => If the buffer is used "dynamically" then we actually allocate enough memory
        //                     to update different regions of the buffer for different frames
        //                     (so RENDER_LATENCY * buffer.size)

        if (desc.usage & vertex_or_index) {
            // ASSERT it's only one or the other
            ASSERT((desc.usage & vertex_or_index) != vertex_or_index);
            ASSERT(desc.data != nullptr);
        }

        if (desc.usage & RESOURCE_USAGE_VERTEX) {
            buffer.size = align_to(buffer.size, dx12::VERTEX_BUFFER_ALIGNMENT);
        }
        else if (desc.usage & RESOURCE_USAGE_INDEX) {
            buffer.size = align_to(buffer.size, dx12::INDEX_BUFFER_ALIGNMENT);
        }
        else if (desc.usage & RESOURCE_USAGE_CONSTANT) {
            buffer.size = align_to(buffer.size, dx12::CONSTANT_BUFFER_ALIGNMENT);
        }

        size_t alloc_size = buffer.size;

        if (desc.usage & RESOURCE_USAGE_DYNAMIC) {
            heap_type = D3D12_HEAP_TYPE_UPLOAD;
            initial_resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
            alloc_size = RENDER_LATENCY * buffer.size;
        }

        D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(alloc_size);
        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.HeapType = heap_type;

        HRESULT res = (allocator->CreateResource(
            &alloc_desc,
            &resource_desc,
            initial_resource_state,
            NULL,
            &buffer.allocation,
            IID_PPV_ARGS(&buffer.resource)
        ));
        if (res == DXGI_ERROR_DEVICE_REMOVED) {
            debug_print(GetDXErrorString(g_device->GetDeviceRemovedReason()));
            DXCall(res);
        }
        DXCall(res);
        buffer.gpu_address = buffer.resource->GetGPUVirtualAddress();

        if (desc.usage & RESOURCE_USAGE_DYNAMIC) {
            buffer.cpu_accessible = true;
            buffer.resource->Map(0, nullptr, &buffer.cpu_address);
        }
    }

    void update_buffer(Buffer& buffer, const void* data, const u64 data_size, const u64 frame_idx)
    {
        ASSERT(buffer.cpu_accessible);

        memory::copy(
            reinterpret_cast<u8*>(buffer.cpu_address) + (frame_idx * buffer.size),
            data,
            data_size
        );
    }
}

using namespace zec::gfx::dx12;

namespace zec::gfx::buffers
{
    BufferHandle create(BufferDesc desc)
    {
        ASSERT(desc.byte_size != 0);
        ASSERT(desc.usage != RESOURCE_USAGE_UNUSED);

        BufferHandle handle = { g_buffers.create_back() };
        Buffer& buffer = g_buffers.get(handle);

        init(buffer, desc, g_allocator);

        if (desc.usage & RESOURCE_USAGE_DYNAMIC && desc.data != nullptr) {
            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                update_buffer(buffer, desc.data, desc.byte_size, i);
            }
        }
        else if (desc.data != nullptr) {
            g_upload_manager.queue_upload(desc, buffer.resource);
        }

        return handle;
    }

    void update(const BufferHandle buffer_id, const void* data, u64 byte_size)
    {
        Buffer& buffer = g_buffers[buffer_id];
        update_buffer(buffer, data, byte_size, g_current_frame_idx);
    }

    void set_debug_name(const BufferHandle handle, const wchar* name)
    {
        ID3D12Resource* resource = g_buffers[handle].resource;
        resource->SetName(name);
    }
}