#pragma once
#include "zec_types.h"

// Based on the allocator classes provided by Marco Castorina & Gabriel Sassone
// See: https://github.com/PacktPublishing/Mastering-Graphics-Programming-with-Vulkan/blob/main/source/raptor/foundation/memory.hpp

namespace zec
{

    struct MemoryStatistics
    {
        size_t bytes_allocated = 0;
        size_t total_capacity = 0;
        u32 allocation_count = 0;

        void add(size_t additional_allocated_mem)
        {
            if (additional_allocated_mem > 0)
            {
                bytes_allocated += additional_allocated_mem;
                ++allocation_count;
            }
        };
    };

    class IAllocator
    {
    public:
        virtual ~IAllocator() {};

        virtual void* allocate(size_t size, size_t alignment = 1) = 0;
        virtual void free(void* pointer) = 0;
    };

    class HeapAllocator : public IAllocator
    {
    public:
        ~HeapAllocator() override;

        void init(size_t size);
        void shutdown();

        void debug_ui();

        void* allocate(size_t size, size_t alignment) override;
        void free(void* pointer) override;

    private:
        // Depends on the TLSF Library
        void* tlsf_handle = nullptr;
        void* ptr = nullptr;
        size_t bytes_allocated = 0;
        size_t total_capacity = 0;
    };

    class LinearAllocator : public IAllocator
    {
    public:
        ~LinearAllocator() override;

        void init(size_t size);
        void shutdown();

        void* allocate(size_t size, size_t alignment) override;
        void free(void* pointer) override;

        void clear();
    private:

        u8* ptr = nullptr;
        size_t bytes_allocated = 0;
        size_t total_capacity = 0;
    };

    class StackAllocator : public IAllocator
    {
    public:
        ~StackAllocator() override;

        void init(size_t size);
        void shutdown();

        void* allocate(size_t size, size_t alignment) override;
        void free(void* pointer) override;

        size_t get_substack_offset();
        void free_substack(size_t offset);

        void clear();
    private:
        u8* ptr = nullptr;
        size_t bytes_allocated = 0;
        size_t total_capacity = 0;
    };

    // Basically just a Linear Allocate that uses virtual memory space
    class StretchyAllocator : public IAllocator
    {
    public:
        ~StretchyAllocator() override;

        void init(size_t initial_size);
        void shutdown();

        void* allocate(size_t size, size_t alignment) override;
        void free(void* pointer) override;

        void clear();
    private:
        void grow(size_t size_to_grow_by);

        u8* ptr = nullptr;
        size_t bytes_allocated = 0;
        size_t total_capacity = 0;
    };
}