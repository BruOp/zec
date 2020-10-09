#include "pch.h"
#include "descriptor_heap.h"
#include "globals.h"
#include "dx_utils.h"


namespace zec::dx12::DescriptorUtils
{
    u16 get_heap_idx(ResourceLayoutRangeUsage usage)
    {
        switch (usage) {
        case zec::ResourceLayoutRangeUsage::READ:
        case zec::ResourceLayoutRangeUsage::WRITE:
            return u16(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        case zec::ResourceLayoutRangeUsage::RENDER_TARGET:
            return u16(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        case zec::ResourceLayoutRangeUsage::DEPTH_STENCIL:
            return u16(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        case zec::ResourceLayoutRangeUsage::SAMPLER:
            return u16(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        case zec::ResourceLayoutRangeUsage::UNUSED:
        default:
            throw std::runtime_error("There is not heap for unused usage...");
        }
    }

    void init(ID3D12Device* device, DescriptorHeap& heap, const DescriptorHeapDesc& desc)
    {
        ASSERT(desc.size > 0 && desc.size <= desc.MAX_SIZE);

        heap.num_allocated = 0;
        heap.capacity = desc.size;
        heap.is_shader_visible = desc.type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        heap.num_per_frame_ranges = heap.is_shader_visible ? RENDER_LATENCY : 1;

        u32 total_num_descriptors = heap.capacity * heap.num_per_frame_ranges;
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

        heap.cpu_starts[0] = heap.heap->GetCPUDescriptorHandleForHeapStart();

        if (heap.is_shader_visible) {
            heap.gpu_starts[0] = heap.heap->GetGPUDescriptorHandleForHeapStart();
        }

        heap.descriptor_size = device->GetDescriptorHandleIncrementSize(desc.type);

        for (size_t i = 1; i < heap.num_per_frame_ranges; i++) {
            heap.cpu_starts[i] = heap.cpu_starts[0];
            heap.cpu_starts[i].ptr += i * heap.capacity * heap.descriptor_size;

            if (heap.is_shader_visible) {
                heap.gpu_starts[i] = heap.gpu_starts[0];
                heap.gpu_starts[i].ptr += i * heap.capacity * heap.descriptor_size;
            }
        }
    }

    void destroy(DescriptorHeap& heap)
    {
        ASSERT(heap.num_allocated == 0);
        heap.heap->Release();
    }

    DescriptorRangeHandle allocate_range(const DescriptorRangeDesc& range_desc)
    {
        u16 heap_idx = get_heap_idx(range_desc.usage);
        DescriptorHeap& heap = g_descriptor_heaps[heap_idx];
        // First check if there is enough room on the deadlist:
        ASSERT(range_desc.size < heap.capacity - heap.num_allocated);
        DescriptorHeapRange range{
            .size = 0,
            .capacity = range_desc.size,
            .offset = heap.num_allocated,
            .dead_list = {}
        };
        heap.num_allocated += range.capacity;

        size_t range_idx = heap.ranges.push_back(range);
        ASSERT(range_idx < UINT16_MAX);

        return DescriptorRangeHandle{
            .heap_idx = heap_idx,
            .idx = range_idx,
        };
    }

    void free_range(const DescriptorRangeHandle& range_handle)
    {
        DescriptorHeap& heap = g_descriptor_heaps[range_handle.heap_idx];
        DescriptorHeapRange& range = heap.ranges[range_handle.idx];

        // If last range in heap
        // Then we can just adjust the 

        // If not, add it to the dead list
        // If so, "merge" the ranges
        // If not
    }

    u32 allocate_descriptor(DescriptorHeapRange& range)
    {
        if (range.size < range.capacity) {
            // TODO: Atomically increment and return size
            return range.size++;
        }
        else {
            throw std::runtime_error("TODO: Use our freelist here");
        }
    }

    u32 allocate_descriptor(DescriptorHeap& heap)
    {
        if (heap.num_allocated < heap.capacity) {
            return heap.num_allocated++;
        }
        else {
            throw std::runtime_error("TODO: Use our freelist here");
        }
    }

    void free_descriptor(DescriptorHeapRange& range, const u32 descriptor_idx)
    {
        // TODO: Allow freeing of descriptors from ranges
    }
    void free_descriptor(DescriptorHeap& heap, const u32 descriptor_idx)
    {
        // TODO: Allow freeing of descriptors from ranges
    }
    void free_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE heap_type, const u32 descriptor_idx)
    {
        free_descriptor(g_descriptor_heaps[heap_type], descriptor_idx);
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
            destroy(heap);
        }
    }

    u32 allocate_descriptor(D3D12_DESCRIPTOR_HEAP_TYPE heap_type)
    {
        ASSERT(heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        return allocate_descriptor(g_descriptor_heaps[heap_type]);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];
        ASSERT(index < heap.num_allocated);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.cpu_starts[g_current_frame_idx];
        handle.ptr += heap.descriptor_size * index;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index)
    {
        ASSERT(type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        const DescriptorHeap& heap = g_descriptor_heaps[type];
        ASSERT(index < heap.num_allocated);
        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_starts[g_current_frame_idx];
        handle.ptr += heap.descriptor_size * index;
        return handle;
    }
}