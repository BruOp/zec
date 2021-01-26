#include "pch.h"
#include "gfx/gfx.h"
#include "buffers.h"
#include "globals.h"
#include "dx_utils.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec::gfx::dx12::buffer_utils
{
    void init(Buffer& buffer, const BufferDesc& desc, D3D12MA::Allocator* allocator)
    {
        ASSERT(desc.byte_size != 0);
        ASSERT(desc.usage != u16(RESOURCE_USAGE_UNUSED));

        buffer.info.size = desc.byte_size;
        buffer.info.stride = desc.stride;
        // Create Buffer
        D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_STATES initial_resource_state = D3D12_RESOURCE_STATE_COMMON;

        u16 vertex_or_index = u16(RESOURCE_USAGE_VERTEX) | u16(RESOURCE_USAGE_INDEX);
        // There are three different sizes to keep track of here:
        // desc.bytes_size  => the minimum size (in bytes) that we desire our buffer to occupy
        // buffer.info.size=> the above size that's been potentially grown to match alignment requirements
        // alloc_size       => If the buffer is used "dynamically" then we actually allocate enough memory
        //                     to update different regions of the buffer for different frames
        //                     (so RENDER_LATENCY * buffer.info.size)

        if (desc.usage & vertex_or_index) {
            // ASSERT it's only one or the other
            ASSERT((desc.usage & vertex_or_index) != vertex_or_index);
            ASSERT(desc.data != nullptr);
        }

        if (desc.usage & RESOURCE_USAGE_VERTEX) {
            buffer.info.size = align_to(buffer.info.size, dx12::VERTEX_BUFFER_ALIGNMENT);
        }
        else if (desc.usage & RESOURCE_USAGE_INDEX) {
            buffer.info.size = align_to(buffer.info.size, dx12::INDEX_BUFFER_ALIGNMENT);
        }
        else if (desc.usage & RESOURCE_USAGE_CONSTANT) {
            buffer.info.size = align_to(buffer.info.size, dx12::CONSTANT_BUFFER_ALIGNMENT);
        }

        size_t alloc_size = buffer.info.size;

        if (desc.usage & RESOURCE_USAGE_DYNAMIC) {
            heap_type = D3D12_HEAP_TYPE_UPLOAD;
            initial_resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
            alloc_size = RENDER_LATENCY * buffer.info.size;
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
        buffer.info.gpu_address = buffer.resource->GetGPUVirtualAddress();

        if (desc.usage & RESOURCE_USAGE_SHADER_READABLE) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
            buffer.srv = descriptor_utils::allocate_descriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, &cpu_handle);

            bool is_structered = desc.type == BufferType::STRUCTURED;
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                .Format = DXGI_FORMAT_UNKNOWN,
                .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Buffer = {
                    .FirstElement = 0,
                    .NumElements = desc.byte_size / desc.stride,
                    .StructureByteStride = desc.stride,
                },
            };

            g_device->CreateShaderResourceView(buffer.resource, &srv_desc, cpu_handle);
        }

        if (desc.usage & RESOURCE_USAGE_DYNAMIC) {
            buffer.info.cpu_accessible = true;
            buffer.resource->Map(0, nullptr, &buffer.info.cpu_address);
        }
    }

    BufferHandle push_back(BufferList& buffer_list, const Buffer& buffer)
    {
        ASSERT(buffer_list.resources.size < UINT32_MAX);
        u32 idx = u32(buffer_list.resources.push_back(buffer.resource));
        buffer_list.allocations.push_back(buffer.allocation);
        buffer_list.infos.push_back(buffer.info);
        buffer_list.srvs.push_back(buffer.srv);
        buffer_list.uavs.push_back(buffer.uav);
        return { idx };
    }

    void update(BufferList& buffer_list, const BufferHandle buffer, const void* data, const u64 data_size, const u64 frame_idx)
    {
        const BufferInfo& buffer_info = buffer_utils::get_buffer_info(buffer_list, buffer);
        ASSERT(buffer_info.cpu_accessible);

        const auto cpu_address = buffer_utils::get_cpu_address(buffer_list, buffer, frame_idx);

        memory::copy(
            reinterpret_cast<u8*>(cpu_address),
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

        Buffer buffer = {};

        buffer_utils::init(buffer, desc, g_allocator);

        BufferHandle handle = buffer_utils::push_back(g_buffers, buffer);

        if (desc.usage & RESOURCE_USAGE_DYNAMIC && desc.data != nullptr) {
            for (size_t i = 0; i < RENDER_LATENCY; i++) {
                buffer_utils::update(g_buffers, handle, desc.data, desc.byte_size, i);
            }
        }
        else if (desc.data != nullptr) {
            g_upload_manager.queue_upload(desc, buffer.resource);
        }

        return handle;
    }

    u32 get_shader_readable_index(const BufferHandle handle)
    {
        return buffer_utils::get_srv_index(g_buffers, handle);
    };

    u32 get_shader_writable_index(const BufferHandle handle)
    {
        return buffer_utils::get_uav_index(g_buffers, handle);
    };

    void update(const BufferHandle buffer_id, const void* data, u64 byte_size)
    {
        ASSERT(is_valid(buffer_id));
        buffer_utils::update(g_buffers, buffer_id, data, byte_size, g_current_frame_idx);
    }

    void set_debug_name(const BufferHandle handle, const wchar* name)
    {
        ID3D12Resource* resource = g_buffers.resources[handle.idx];
        resource->SetName(name);
    }
}