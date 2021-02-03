#include "pch.h"
#include "descriptor_heap.h"
#include "dx_utils.h"


namespace zec::gfx::dx12
{
    DescriptorRangeHandle DescriptorHeap::allocate_descriptors(const size_t count, D3D12_CPU_DESCRIPTOR_HANDLE in_handles[])
    {
        // TODO: wrap this all in a mutex lock?
        DescriptorRangeHandle handle{};
        // First check if there is an entry in the dead list that satisfies our requirements:
        for (size_t i = 0; i < dead_list.size; ++i) {
            DescriptorRangeHandle free_range = dead_list[i];
            if (free_range.get_count() == count) {
                // Swap with the end, so that we don't have holes
                if (dead_list.size == 1) {
                    dead_list.pop_back();
                }
                else {
                    dead_list[i] = dead_list.pop_back();
                }
                handle = free_range;
                num_free -= handle.get_count();
                break;
            }
            else if (free_range.get_count() > count) {
                // Grab a chunk of the free range
                dead_list[i] = DescriptorRangeHandle::encode(free_range.get_offset() + count, free_range.get_count() - count);
                handle = free_range;
                num_free -= handle.get_count();
                break;
            }
        }

        // Allocate from the end of the heap if the free list didn't yield anything
        if (!is_valid(handle)) {
            ASSERT(num_allocated + count <= capacity);
            handle = DescriptorRangeHandle::encode(num_allocated, count);
            // Fill in_handles;
            num_allocated += count;
        }

        for (size_t i = 0; i < count; i++) {
            in_handles[i] = get_cpu_descriptor_handle(handle, i);
        }
        // We don't need to increment num_allocated since we've grabbed these from the free list.
        return handle;
    }
}