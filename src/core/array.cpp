#include "pch.h"
#include "array.h"
#include "utils/memory.h"

namespace zec
{
    template<typename T>
    Array<T>::Array(const size_t cap) : data{ nullptr }, capacity{ 0 }, size{ 0 } {
        data = static_cast<T*>(memory::virtual_reserve((void*)data, g_GB));
        grow(cap);
    }

    template<typename T>
    Array<T>::Array() : Array{ 0 } { };

    template<typename T>
    Array<T>::~Array()
    {
        if (data != nullptr) {
            memory::virtual_free((void*)data);
        }
    }

    template<typename T>
    size_t Array<T>::push_back(const T& val)
    {
        if (size >= capacity) {
            reserve(capacity + 1);
        }
        memory::copy((void*)&data[size++], (void*)&val, sizeof(T));
        return size;
    };

    template<typename T>
    size_t Array<T>::grow(size_t additional_slots)
    {
        if (additional_slots + size > capacity) {
            reserve(additional_slots + size);
        }
        size += additional_slots;
        return size;
    };

    template<typename T>
    size_t Array<T>::reserve(size_t new_capacity)
    {
        ASSERT_MSG(new_capacity > capacity, "Array can only grow at the moment!");
        const SysInfo& sys_info = get_sys_info();
        void* currentEnd = (void*)(data + capacity);
        size_t additional_memory_required = (new_capacity - capacity) * sizeof(T);
        // Grows the new memory required to the next page boundary
        size_t additional_pages_required = size_t(ceil(double(additional_memory_required) / double(sys_info.page_size)));
        additional_memory_required = additional_pages_required * sys_info.page_size;
        // Commit the new page(s) of our reserved virtual memory
        memory::virtual_commit(currentEnd, additional_memory_required);
        // Update capacity to reflect new size
        capacity += additional_memory_required / sizeof(T);
        // Since capacity is potentiall larger than just old_capacity + additional_slots, we return it
        return capacity;
    }
}