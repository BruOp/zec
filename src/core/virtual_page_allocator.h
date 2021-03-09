#pragma once
#include "utils/memory.h"
#include "utils/sys_info.h"

namespace zec
{
    class VirtualPageAllocator
    {
    public:
        VirtualPageAllocator(const size_t max_capacity = 4096) :
            page_size{ get_sys_info().page_size },
            max_capacity{ page_size* ((max_capacity + page_size - 1) / page_size) }
        {
            data = (u8*)memory::virtual_reserve(nullptr, max_capacity);
        };
        ~VirtualPageAllocator()
        {
            if (data != nullptr) {
                memory::virtual_free(data);
                data = nullptr;
                bytes_provided = 0;
                num_pages_allocated = 0;
            }
        }

        void* allocate(size_t byte_size)
        {
            size_t diff = num_pages_allocated * page_size - bytes_provided;
            u8* current_end = data + (num_pages_allocated * page_size);
            if (diff < byte_size) {
                size_t num_additional_pages = ((byte_size - diff) + page_size - 1) / page_size;
                memory::virtual_commit(current_end, page_size * num_additional_pages);
                num_pages_allocated += num_additional_pages;
            }
            bytes_provided += byte_size;
            return data + bytes_provided;
        }

        u8* data = nullptr;
        const size_t page_size = 0;
        const size_t max_capacity = 0;
        size_t bytes_provided = 0;
        size_t num_pages_allocated = 0;
    };
}
