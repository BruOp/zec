#include "allocators.hpp"
#include <tlsf/tlsf.h>

#include "../utils/memory.h"
#include "array.h"

namespace zec
{
    static void exit_walker(void* ptr, size_t size, int used, void* user);
    static void imgui_walker(void* ptr, size_t size, int used, void* user);

    void exit_walker(void* ptr, size_t size, int used, void* user)
    {
        MemoryStatistics* stats = static_cast<MemoryStatistics*>(user);
        stats->add(used ? size : 0);

        if (used)
        {
            // TODO LOG: Found active allocation %p, %llu\n", ptr, size);
        }
    }

    void HeapAllocator::init(size_t size_in_bytes)
    {
        total_capacity = size_in_bytes;
        ptr = memory::alloc(size_in_bytes);
        bytes_allocated = 0;

        tlsf_handle = tlsf_create_with_pool(ptr, total_capacity);

        // TODO: LOG this?
    };

    void HeapAllocator::shutdown()
    {
        MemoryStatistics stats {
            .bytes_allocated = 0,
            .total_capacity = total_capacity
        };
        pool_t pool = tlsf_get_pool(tlsf_handle);
        tlsf_walk_pool(pool, exit_walker, (void*)&stats);

        if (stats.bytes_allocated)
        {
            // TODO: LOG shutdown failure
        }
        else
        {
            // TODO: LOG successful shutdown
        }
    }

    void HeapAllocator::debug_ui()
    {
        // TODO: Should probably be like a friend class
        // TODO: Print out stats using imgui
    }

#define HEAP_ALLOCATOR_STATS
    void* HeapAllocator::allocate(size_t size, size_t alignment)
    {
#if defined(HEAP_ALLOCATOR_STATS)
        void* allocated_memory = alignment == 1
                                              ? tlsf_malloc(tlsf_handle, size)
                                              : tlsf_memalign(tlsf_handle, alignment, size);
        size_t actual_size = tlsf_block_size(allocated_memory);
        bytes_allocated += actual_size;

        return allocated_memory;
#else
        return tlsf_malloc(tlsf_handle, size);
#endif
    }

    void HeapAllocator::free(void* pointer)
    {
#if defined(HEAP_ALLOCATOR_STATS)
        size_t actual_size = tlsf_block_size(pointer);
        bytes_allocated -= actual_size;
#endif
        tlsf_free(tlsf_handle, ptr);
    }

    LinearAllocator::~LinearAllocator()
    {
        ASSERT(bytes_allocated == 0);
        ASSERT(ptr == nullptr);
    }

    void LinearAllocator::init(size_t size)
    {
        ptr = static_cast<u8*>(memory::alloc(size));
        total_capacity = size;
        bytes_allocated = 0;
    }

    void LinearAllocator::shutdown()
    {
        clear();
        memory::free_mem(static_cast<void*>(ptr));
    }

    void* LinearAllocator::allocate(size_t size, size_t alignment)
    {
        ASSERT(size > 0);

        const size_t aligned_offset = memory::align(bytes_allocated, alignment);
        ASSERT(aligned_offset < total_capacity);
        const size_t new_bytes_allocated = aligned_offset + size;
        ASSERT(new_bytes_allocated < total_capacity);
        bytes_allocated = new_bytes_allocated;
        return ptr + aligned_offset;
    }

    void LinearAllocator::free(void* ptr)
    {
        // We don't actually free individual items from our linear allocator
    }

    void LinearAllocator::clear()
    {
        bytes_allocated = 0;
    }

    StackAllocator::~StackAllocator()
    {
        ASSERT(bytes_allocated == 0);
        ASSERT(ptr == nullptr);
    }

    void StackAllocator::init(size_t size)
    {
        ptr = static_cast<u8*>(memory::alloc(size));
        bytes_allocated = 0;
        total_capacity = size;
    }

    void StackAllocator::shutdown()
    {
        memory::free_mem(ptr);
    }

    void* StackAllocator::allocate(size_t size, size_t alignment)
    {
        ASSERT(size > 0);

        const size_t aligned_offset = memory::align(size, alignment);
        ASSERT(aligned_offset < size);
        const size_t new_bytes_allocated = aligned_offset + size;
        ASSERT(new_bytes_allocated < total_capacity);
        bytes_allocated = new_bytes_allocated;

        return ptr + aligned_offset;
    }

    void StackAllocator::free(void* pointer_to_free)
    {
        ASSERT(static_cast<u8*>(pointer_to_free) >= ptr);
        ASSERT_MSG(static_cast<u8*>(pointer_to_free) < ptr + total_capacity, "Attempted to free a pointer outside the allocator's range");
        ASSERT_MSG(static_cast<u8*>(pointer_to_free) < ptr + bytes_allocated, "Attempted to free a pointer that was likely already freed");

        const size_t size_at_ptr = static_cast<u8*>(pointer_to_free) - ptr;

        bytes_allocated = size_at_ptr;
    }

    size_t StackAllocator::get_substack_offset()
    {
        return bytes_allocated;
    }

    void StackAllocator::free_substack(size_t offset)
    {
        ASSERT(offset < bytes_allocated);
        bytes_allocated = offset;
    }

    void StackAllocator::clear()
    {
        bytes_allocated = 0;
    }

    StretchyAllocator::~StretchyAllocator()
    {
        ASSERT(ptr == nullptr);
        ASSERT(bytes_allocated = 0);
    }

    void StretchyAllocator::init(size_t initial_size)
    {
        ASSERT(ptr == nullptr);
        ASSERT(bytes_allocated == 0);

        ptr = static_cast<u8*>(memory::virtual_reserve((void*)ptr, g_GB));
        grow(initial_size);
    }

    void StretchyAllocator::shutdown()
    {
        ASSERT(ptr != nullptr);
        ASSERT(bytes_allocated != 0);
    }

    void* StretchyAllocator::allocate(size_t size, size_t alignment)
    {
        ASSERT(size > 0);

        const size_t aligned_offset = memory::align(bytes_allocated, alignment);
        const size_t new_bytes_allocated = aligned_offset + size;
        if (new_bytes_allocated > total_capacity)
        {
            grow(new_bytes_allocated - total_capacity);
        }
        bytes_allocated = new_bytes_allocated;
        return ptr + aligned_offset;
    }

    void StretchyAllocator::free(void* pointer)
    {
        // We don't really do anything here
    }

    void StretchyAllocator::clear()
    {
        bytes_allocated = 0;
    }

    void StretchyAllocator::grow(size_t size_to_grow_by)
    {
        if (total_capacity < size_to_grow_by + bytes_allocated)
        {
            const SysInfo& sys_info = get_sys_info();
            ASSERT(total_capacity % sys_info.page_size == 0u);
            size_t pages_used = size_t(total_capacity / sys_info.page_size);
            u8* current_end = ptr + total_capacity;
            
            // Round up to the pages required
            size_t additional_pages_required = (size_to_grow_by + sys_info.page_size - 1u) / sys_info.page_size;
            size_t additional_capacity_required = additional_pages_required * sys_info.page_size;
            memory::virtual_commit(current_end, additional_capacity_required);
            total_capacity += additional_capacity_required;
        }
    }
}