#include "pch.h"
#include "descriptor_heap.h"
#include "dx_utils.h"


namespace zec::gfx::dx12::descriptor_utils
{
    DescriptorRangeHandle allocate_descriptors(DescriptorHeap& heap, const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[])
    {
        // TODO: wrap this all in a mutex lock?
        DescriptorRangeHandle handle{};
        // First check if there is an entry in the dead list that satisfies our requirements:
        for (size_t i = 0; i < heap.dead_list.size; ++i) {
            DescriptorRangeHandle free_range = heap.dead_list[i];
            if (get_count(free_range) == count) {
                // Swap with the end, so that we don't have holes
                if (heap.dead_list.size == 1) {
                    heap.dead_list.pop_back();
                }
                else {
                    heap.dead_list[i] = heap.dead_list.pop_back();
                }
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
}