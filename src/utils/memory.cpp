#include "memory.h"
#include <windows.h>
#include <stdexcept>

namespace zec::memory
{
    DWORD alloc_type_to_win_alloc_type(AllocationType alloc_type)
    {
        switch (alloc_type) 	{
        case zec::memory::AllocationType::COMMIT:
            return MEM_COMMIT;
        case zec::memory::AllocationType::RESERVE:
            return MEM_RESERVE;
        default:
            throw std::invalid_argument("Cannot handle this type of allocation type");
        }
    };

    void* virtual_alloc(void* ptr, size_t byte_size, AllocationType alloc_type)
    {
        const SysInfo& sys_info = get_sys_info();
        ASSERT(u64(ptr) % sys_info.page_size == 0);
        ASSERT(byte_size % sys_info.page_size == 0);
        return VirtualAlloc(ptr, byte_size, alloc_type_to_win_alloc_type(alloc_type), PAGE_READWRITE);
    }

    void* virtual_commit(void* ptr, size_t byte_size);

    void virtual_free(void* ptr)
    {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }

}
