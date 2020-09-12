#include "pch.h"
#include "resources.h"
#include "wrappers.h"
#include "utils/utils.h"

namespace zec
{
    namespace dx12
    {
        void init(Buffer& buffer, const BufferDesc& desc, D3D12MA::Allocator* allocator)
        {
            ASSERT(desc.byte_size != 0);
            ASSERT(desc.usage != BufferUsage::UNUSED);

            buffer.size = desc.byte_size;
            // Create Buffer
            D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_STATES initial_resource_state = D3D12_RESOURCE_STATE_COMMON;

            u16 vertex_or_index = BufferUsage::VERTEX | BufferUsage::INDEX;
            // There are three different sizes to keep track of here:
            // desc.bytes_size  => the minimum size (in bytes) that we desire our buffer to occupy
            // buffer.size      => the above size that's been potentially grown to match alignment requirements
            // alloc_size       => If the buffer is used "dynamically" then we actually allocate enough memory
            //                     to update different regions of the buffer for different frames
            //                     (so RENDER_LATENCY * buffer.size)
            size_t alloc_size = buffer.size;

            if (desc.usage & vertex_or_index) {
                // ASSERT it's only one or the other
                ASSERT((desc.usage & vertex_or_index) != vertex_or_index);
                ASSERT(desc.data != nullptr);
            }

            if (desc.usage & BufferUsage::VERTEX) {
                buffer.size = align_to(buffer.size, dx12::VERTEX_BUFFER_ALIGNMENT);
            }
            else if (desc.usage & BufferUsage::INDEX) {
                buffer.size = align_to(buffer.size, dx12::INDEX_BUFFER_ALIGNMENT);
            }
            else if (desc.usage & BufferUsage::CONSTANT) {
                buffer.size = align_to(buffer.size, dx12::CONSTANT_BUFFER_ALIGNMENT);
            }

            if (desc.usage & BufferUsage::DYNAMIC) {
                heap_type = D3D12_HEAP_TYPE_UPLOAD;
                initial_resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
                alloc_size = RENDER_LATENCY * buffer.size;
            }

            D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(alloc_size);
            D3D12MA::ALLOCATION_DESC alloc_desc = {};
            alloc_desc.HeapType = heap_type;

            DXCall(allocator->CreateResource(
                &alloc_desc,
                &resource_desc,
                initial_resource_state,
                NULL,
                &buffer.allocation,
                IID_PPV_ARGS(&buffer.resource)
            ));

            buffer.gpu_address = buffer.resource->GetGPUVirtualAddress();

            if (desc.usage & BufferUsage::DYNAMIC) {
                buffer.cpu_accessible = true;
                buffer.resource->Map(0, &CD3DX12_RANGE(0, 0), &buffer.cpu_address);
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
}
