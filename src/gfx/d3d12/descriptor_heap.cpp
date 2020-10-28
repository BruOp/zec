#include "pch.h"
#include "descriptor_heap.h"
#include "globals.h"
#include "dx_utils.h"


namespace zec::dx12::DescriptorUtils
{
    void init(ID3D12Device* device, DescriptorHeap& heap, const DescriptorHeapDesc& desc)
    {
        ASSERT(desc.size > 0 && desc.size <= desc.MAX_SIZE);

        heap.num_allocated = 0;
        heap.capacity = desc.size;
        heap.is_shader_visible = desc.type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        u32 total_num_descriptors = heap.capacity;
        D3D12_DESCRIPTOR_HEAP_DESC d3d_desc{ };
        d3d_desc.NumDescriptors = total_num_descriptors;
        d3d_desc.Type = desc.type;
        d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (heap.is_shader_visible) {
            d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }


        DXCall(device->CreateDescriptorHeap(
            &d3d_desc, IID_PPV_ARGS(&heap.heap)
        ));

        heap.cpu_start = heap.heap->GetCPUDescriptorHandleForHeapStart();

        if (heap.is_shader_visible) {
            heap.gpu_start = heap.heap->GetGPUDescriptorHandleForHeapStart();
        }

        heap.descriptor_size = device->GetDescriptorHandleIncrementSize(desc.type);
    }

    void destroy(DescriptorHeap& heap)
    {
        ASSERT(heap.num_allocated == 0);
        heap.heap->Release();
    }

    DescriptorHandle allocate_descriptor(DescriptorHeap& heap, D3D12_CPU_DESCRIPTOR_HANDLE* in_handle)
    {
        // First check if there is an entry in the dead list:
        if (heap.dead_list.size > 0) {
            ++heap.num_allocated;
            return heap.dead_list.pop_back();
        }

        ASSERT(heap.capacity - heap.num_allocated > 0);
        in_handle->ptr = heap.cpu_start.ptr + heap.descriptor_size * heap.num_allocated;
        return { u32(heap.num_allocated++) };
    }

    DescriptorHandle allocate_descriptor(const D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_CPU_DESCRIPTOR_HANDLE* handle)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        return allocate_descriptor(g_descriptor_heaps[type], handle);
    }

    void allocate_descriptors(DescriptorHeap& heap, DescriptorHandle** in_descriptors, D3D12_CPU_DESCRIPTOR_HANDLE** in_handles, const u32 num_handles)
    {
        // TODO: Obtain lock on the descriptor heap, since we need all the descriptors to be contiguous
    }

    void free_descriptor(DescriptorHeap& heap, const DescriptorHandle descriptor, u64 current_frame_idx)
    {
        if (is_valid(descriptor)) {
            heap.destruction_queue.push_back({ .handle = descriptor, .frame_index = u32(current_frame_idx) });
        }
    }

    void free_descriptor(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        free_descriptor(g_descriptor_heaps[type], descriptor_handle, g_current_frame_idx);
    }

    void process_destruction_queue(DescriptorHeap& heap, u64 current_frame_idx)
    {
        for (const auto element : heap.destruction_queue) {
            if (element.frame_index == current_frame_idx) {
                --heap.num_allocated;
                heap.dead_list.push_back(element.handle);
            }
        }
    }


    void init_descriptor_heaps()
    {
        // Create a heap of a specific type
        constexpr DescriptorHeapDesc descs[] = {
          {
            .size = DescriptorHeapDesc::MAX_SIZE,
            .type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
          },
          {
            .size = 128,
            .type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV
          },
          {
            .size = 128,
            .type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV
          },
          {
            .size = 128,
            .type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
          },
        };

        for (const DescriptorHeapDesc& desc : descs) {
            init(g_device, g_descriptor_heaps[size_t(desc.type)], desc);
        }
    }

    void destroy_descriptor_heaps()
    {
        for (auto& heap : g_descriptor_heaps) {
            process_destruction_queue(heap, g_current_frame_idx);
            destroy(heap);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.cpu_start;
        handle.ptr += heap.descriptor_size * descriptor_handle.idx;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];

        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_start;
        handle.ptr += heap.descriptor_size * descriptor_handle.idx;
        return handle;
    }
}