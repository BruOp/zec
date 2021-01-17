
#pragma once
#include "pch.h"
#include "utils/assert.h"
#include "utils/sys_info.h"

namespace zec
{
    namespace memory
    {
        enum struct AllocationType : u32
        {
            COMMIT = MEM_COMMIT,
            RESERVE = MEM_RESERVE
        };

        inline void* alloc(size_t size)
        {
            ASSERT(size > 0);
            return ::malloc(size);
        };

        inline void free(void* ptr)
        {
            ASSERT(ptr != nullptr);
            ::free(ptr);
        };

        inline void* virtual_alloc(void* ptr, size_t byte_size, AllocationType alloc_type)
        {
            const SysInfo& sys_info = get_sys_info();
            ASSERT(u64(ptr) % sys_info.page_size == 0);
            ASSERT(byte_size % sys_info.page_size == 0);
            return VirtualAlloc(ptr, byte_size, DWORD(alloc_type), PAGE_READWRITE);
        }

        inline void* virtual_reserve(void* ptr, size_t byte_size)
        {
            return virtual_alloc(ptr, byte_size, AllocationType::RESERVE);
        }

        inline void* virtual_commit(void* ptr, size_t byte_size)
        {
            return virtual_alloc(ptr, byte_size, AllocationType::COMMIT);
        }

        inline void virtual_free(void* ptr)
        {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }

        inline void* copy(void* dest, const void* src, size_t size)
        {
            return memcpy(dest, src, size);
        }
    }
}
