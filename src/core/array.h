#pragma once
#include "pch.h"
#include "utils/assert.h"
#include "utils/memory.h"

namespace zec
{
    static constexpr size_t g_GB = 1024 * 1024 * 1024;

    template <typename T>
    class Array
    {
        /*
        This is basically equivalent to std::vector, with a few key exceptions:
        - ONLY FOR POD TYPES! We don't initialize or destroy the elements and use things like memcpy for copies
        - it's guaranteed to never move since we're using virtual alloc to allocate about 1 GB per array (in Virtual memory)
        */
    public:
        T* data = nullptr;
        size_t capacity = 0;
        size_t size = 0;

        Array() : Array{ 0 } { };
        Array(size_t cap) : data{ nullptr }, capacity{ 0 }, size{ 0 } {
            data = static_cast<T*>(memory::virtual_reserve((void*)data, g_GB));
            grow(cap);
        }

        ~Array()
        {
            if (data != nullptr) {
                memory::virtual_free((void*)data);
            }
        };

        inline T& operator[](size_t idx)
        {
            ASSERT_MSG(idx < size, "Cannot access elements beyond Array size.");
            return data[idx];
        };

        inline const T& operator[](size_t idx) const
        {
            ASSERT_MSG(idx < size, "Cannot access elements beyond Array size.");
            return data[idx];
        };

        size_t push_back(const T& val)
        {
            if (size >= capacity) {
                reserve(capacity + 1);
            }
            memory::copy((void*)&data[size++], (void*)&val, sizeof(T));
            return size;
        };

        template<typename ...Args>
        size_t create_back(Args... args)
        {
            if (size >= capacity) {
                reserve(capacity + 1);
            }
            data[size++] = T{ args... };
            return size;
        };

        size_t grow(size_t additional_slots)
        {
            if (additional_slots + size > capacity) {
                reserve(additional_slots + size);
            }
            size += additional_slots;
            return size;
        };

        size_t reserve(size_t new_capacity)
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
    };
} // namespace zec
