#include "tracked_allocator.h"
#include "../utils/assert.h"
#include "../utils/memory.h"

namespace zec
{
    SimpleAllocator::SimpleAllocator(const size_t max_capacity)
        : max_capacity{ max_capacity }
    {
    }

    SimpleAllocator::~SimpleAllocator()
    {
        if (ptr != nullptr)
        {
            ASSERT_MSG(num_allocations_issued == 0, "Attempting destruction while num_allocations_issued is not zero, it's likely we haven't returned all the transient allocations we created.");
            zec::memory::free_mem(ptr);
            ptr = nullptr;
        }
    }

    void SimpleAllocator::initialize()
    {
        ASSERT(ptr == nullptr);
        ptr = reinterpret_cast<uint8_t*>(zec::memory::alloc(max_capacity));
    }

    void SimpleAllocator::reset()
    {
        ASSERT_MSG(num_allocations_issued == 0, "Attempting to reset while num_allocations_issued is not zero, it's likely we haven't returned all the transient allocations we created.");
        num_allocations_issued = 0;
        allocated_bytes = 0;
    }

    Allocation SimpleAllocator::alloc(const size_t alloc_size)
    {
        ASSERT_MSG(ptr != nullptr, "This allocator has not been initialized correctly.");

        bool capacity_exceeded = alloc_size + allocated_bytes > max_capacity;

        if (capacity_exceeded)
        {
            ASSERT_FAIL("Attempting to allocate %u bytes with only %u bytes remaining, consider increasing size of this allocator! Doubling capacity now.");
            size_t old_capacity = max_capacity;
            uint8_t* old_ptr = ptr;

            max_capacity *= 2u;
            ptr = static_cast<uint8_t*>(zec::memory::alloc(max_capacity));
            ASSERT(ptr != nullptr);
            zec::memory::copy(ptr, old_ptr, old_capacity);
            zec::memory::free_mem(old_ptr);
        }

        num_allocations_issued++;
        Allocation allocation = {
            .byte_size = alloc_size,
            .ptr = ptr + allocated_bytes
        };
        allocated_bytes += alloc_size;

        return allocation;
    }

    void SimpleAllocator::release(Allocation& allocation)
    {
        bool owns_allocation = allocation.ptr >= ptr && allocation.ptr < ptr + allocated_bytes;
        if (!owns_allocation)
        {
            ASSERT_FAIL("Attempting to release allocation not created by this allocator, skipping.");
            return;
        }
        
        --num_allocations_issued;
        ASSERT_MSG(num_allocations_issued >= 0, "Released too many resources! Likely due to you releasing allocations multiple times");

        allocation.ptr = nullptr;
        allocation.byte_size = 0;
    }
}