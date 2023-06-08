#pragma once
#include <malloc.h>
#include <memory.h>

#include "utils/assert.h"
#include "utils/sys_info.h"
#include "core/zec_types.h"

namespace zec
{
    namespace memory
    {
        enum struct AllocationType : u32
        {
            COMMIT = 0,
            RESERVE = 1
        };

        inline void copy(void* dest, void* src, size_t size) {
            memcpy(dest, src, size);
        }

        inline size_t align(size_t size, size_t alignment) {
            const size_t alignment_mask = alignment - 1;
            return (size + alignment_mask) & ~alignment_mask;
        }

        inline void* alloc(size_t size)
        {
            ASSERT(size > 0);
            return ::malloc(size);
        };

        inline void free_mem(void* ptr)
        {
            ASSERT(ptr != nullptr);
            ::free(ptr);
        };

        void* virtual_alloc(void* ptr, size_t byte_size, AllocationType alloc_type);

        inline void* virtual_reserve(void* ptr, size_t byte_size)
        {
            return virtual_alloc(ptr, byte_size, AllocationType::RESERVE);
        };

        inline void* virtual_commit(void* ptr, size_t byte_size)
        {
            return virtual_alloc(ptr, byte_size, AllocationType::COMMIT);
        };

        void virtual_free(void* ptr);

        inline void* copy(void* dest, const void* src, size_t size)
        {
            return memcpy(dest, src, size);
        }
    }
}
