#include "pch.h"
#include "descriptor_heap.h"
#include "globals.h"
#include "dx_utils.h"


namespace zec::gfx::dx12::descriptor_utils
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
        ASSERT(heap.num_free == heap.num_allocated);
        heap.heap->Release();
    }

    DescriptorRangeHandle allocate_descriptors(DescriptorHeap& heap, const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[])
    {
        // TODO: wrap this all in a mutex lock?
        DescriptorRangeHandle handle{};
        // First check if there is an entry in the dead list that satisfies our requirements:
        for (size_t i = 0; i < heap.dead_list.size; ++i) {
            DescriptorRangeHandle free_range = heap.dead_list[i];
            if (get_count(free_range) == count) {
                // Swap with the end, so that we don't have holes
                heap.dead_list[i] = heap.dead_list.pop_back();
                handle = free_range;
                heap.num_free -= get_count(handle);
                break;
            }
            else if (get_count(free_range) > count) {
                // Grab a chunk of the free range
                heap.dead_list[i] = encode(get_offset(free_range) + count, get_count(free_range) - count);
                handle = free_range;
                heap.num_free -= get_count(handle);
                break;
            }
        }

        // Allocate from the end of the heap if the free list didn't yield anything
        if (!is_valid(handle)) {
            ASSERT(heap.num_allocated + count <= heap.capacity);
            handle = encode(heap.num_allocated, count);
            // Fill in_handles;
            heap.num_allocated += count;
        }

        for (size_t i = 0; i < count; i++) {
            in_handles[i].ptr = heap.cpu_start.ptr + heap.descriptor_size * (get_offset(handle) + i);
        }
        // We don't need to increment num_allocated since we've grabbed these from the free list.
        return handle;
    }

    DescriptorRangeHandle allocate_descriptors(const D3D12_DESCRIPTOR_HEAP_TYPE type, const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[])
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        return allocate_descriptors(g_descriptor_heaps[type], count, in_handles);
    }


    void free_descriptors(DescriptorHeap& heap, const DescriptorRangeHandle descriptor, u64 current_frame_idx)
    {
        if (is_valid(descriptor)) {
            heap.destruction_queue.push_back({ .handle = descriptor, .frame_index = u32(current_frame_idx) });
        }
    }

    void free_descriptors(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_handle)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        free_descriptors(g_descriptor_heaps[type], descriptor_handle, g_current_frame_idx);
    }


    void process_destruction_queue(DescriptorHeap& heap, u64 current_frame_idx)
    {
        for (const auto element : heap.destruction_queue) {
            if (element.frame_index == current_frame_idx) {
                heap.num_free += get_count(element.handle);
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

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_handle, size_t local_offset)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.cpu_start;
        handle.ptr += heap.descriptor_size * (get_offset(descriptor_handle) + local_offset);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorRangeHandle descriptor_handle, size_t local_offset)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];

        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_start;
        handle.ptr += heap.descriptor_size * (get_offset(descriptor_handle) + local_offset);
        return handle;
    }
}