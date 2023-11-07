#pragma once
#include "../utils/memory.h"

namespace zec
{
    //template<size_t capacity>
    //class FixedLinearAllocator
    //{
    //public:
    //    FixedLinearAllocator() = default;
    //    ~FixedLinearAllocator() = default;

    //    FixedLinearAllocator(FixedLinearAllocator& other) = delete;
    //    FixedLinearAllocator& operator=(FixedLinearAllocator& other) = delete;

    //    void* allocate(size_t num_bytes)
    //    {
    //        ASSERT(offset + num_bytes < capacity);
    //        void* offset_pointer = ptr + offset;
    //        offset += num_bytes;
    //        return offset_pointer;
    //    }

    //private:
    //    // In bytes
    //    size_t offset = 0;
    //    u8 ptr[capacity] = {};
    //};

    //class LinearAllocator
    //{
    //public:
    //    LinearAllocator(size_t max_capacity) : capacity(max_capacity), offset(0)
    //    {
    //        ptr = reinterpret_cast<u8*>(zec::memory::alloc(capacity));
    //    };

    //    ~LinearAllocator()
    //    {
    //        zec::memory::free_mem(ptr);
    //    };

    //    LinearAllocator(LinearAllocator& other) = delete;
    //    LinearAllocator& operator=(LinearAllocator& other) = delete;

    //    void* allocate(size_t num_bytes)
    //    {
    //        ASSERT(offset + num_bytes < capacity);
    //        void* offset_pointer = ptr + offset;
    //        offset += num_bytes;
    //        return offset_pointer;
    //    }

    //private:
    //    // In bytes
    //    const size_t capacity = 0;
    //    size_t offset = 0;
    //    u8* ptr = nullptr;
    //};
}