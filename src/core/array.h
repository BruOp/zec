
#pragma once
#include "pch.h"
#include "utils/utils.h"
#include "utils/memory.h"

namespace zec
{
    static constexpr size_t g_GB = 1024 * 1024 * 1024;

    struct ArrayView
    {
        u32 offset = 0;
        u32 size = 0;
    };

    template<typename T, size_t Capacity>
    class FixedArray
    {
    public:
        T data[Capacity] = {};
        const size_t capacity = Capacity;
        size_t size = 0;

        FixedArray() = default;

        UNCOPIABLE(FixedArray);
        UNMOVABLE(FixedArray);

        T* begin()
        {
            return data;
        }

        T* end()
        {
            return data + size;
        }
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
            ASSERT(size < capacity);
            T[size] = val;
            return size++;
        };

        T pop_back()
        {
            ASSERT(size > 0);
            return data[--size];
        }

        template<typename ...Args>
        size_t create_back(Args... args)
        {
            ASSERT(size < capacity);
            data[size] = T{ args... };
            return size++;
        };

        void empty()
        {
            memset((void*)data, 0, size * sizeof(T));
            size = 0;
        }

        size_t find_index(const T value_to_compare, const size_t starting_idx = 0)
        {
            for (size_t i = starting_idx; i < size; i++) {
                if (data[i] == value_to_compare) {
                    return i;
                }
            }
            return UINT64_MAX;
        }
    };

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

        T* begin()
        {
            return data;
        }

        T* end()
        {
            return data + size;
        }

        UNCOPIABLE(Array);
        UNMOVABLE(Array);

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
            data[size] = val;
            return size++;
        };

        T pop_back()
        {
            ASSERT(size > 0);
            return data[--size];
        }

        template<typename ...Args>
        size_t create_back(Args... args)
        {
            if (size >= capacity) {
                reserve(capacity + 1);
            }
            data[size] = T{ args... };
            return size++;
        };

        // Grow both reserves and increases the size, so new items are indexable
        size_t grow(size_t additional_slots)
        {
            if (additional_slots + size > capacity) {
                reserve(additional_slots + size);
            }
            size += additional_slots;
            return size;
        };

        // Reserve grows the allocation, but does not increase the size.
        // Do not try to index into the new space though, push to increase size first
        // or use `grow` instead
        size_t reserve(size_t new_capacity)
        {
            if (new_capacity < capacity) {
                debug_print(L"Array can only grow at the moment!");
                return capacity;
            }

            const SysInfo& sys_info = get_sys_info();
            size_t pages_used = size_t(ceil(double(capacity * sizeof(T)) / double(sys_info.page_size)));
            size_t current_size = pages_used * sys_info.page_size;
            void* current_end = (void*)(u64(data) + current_size);

            size_t additional_memory_required = (new_capacity * sizeof(T)) - current_size;
            // Grows the new memory required to the next page boundary
            size_t additional_pages_required = size_t(ceil(double(additional_memory_required) / double(sys_info.page_size)));
            additional_memory_required = additional_pages_required * sys_info.page_size;

            // Commit the new page(s) of our reserved virtual memory
            memory::virtual_commit(current_end, additional_memory_required);
            // Update capacity to reflect new size
            capacity = (current_size + additional_memory_required) / sizeof(T);
            // Since capacity is potentiall larger than just old_capacity + additional_slots, we return it
            return capacity;
        }

        void empty()
        {
            // I don't think this is actually necessary
            // memset((void*)data, 0, size * sizeof(T));
            size = 0;
        }

        size_t find_index(const T value_to_compare, const size_t starting_idx = 0)
        {
            for (size_t i = starting_idx; i < size; i++) {
                if (data[i] == value_to_compare) {
                    return i;
                }
            }
            return UINT64_MAX;
        }
    };

    template <typename T>
    class ManagedArray
    {
        /*
        This is basically equivalent to std::vector, with a few key exceptions:
        - it's guaranteed to never move since we're using virtual alloc to allocate about 1 GB per array (in Virtual memory)
        */
    public:
        T* data = nullptr;
        size_t capacity = 0;
        size_t size = 0;

        ManagedArray() : ManagedArray{ 0 } { };
        ManagedArray(size_t cap) : data{ nullptr }, capacity{ 0 }, size{ 0 } {
            data = static_cast<T*>(memory::virtual_reserve((void*)data, g_GB));
            grow(cap);
        }

        ~ManagedArray()
        {
            for (size_t i = 0; i < size; i++) {
                data[i].~T();
            }
            if (data != nullptr) {
                memory::virtual_free((void*)data);
            }
        };

        T* begin()
        {
            return data;
        }

        T* end()
        {
            return data + size;
        }

        UNCOPIABLE(ManagedArray);
        UNMOVABLE(ManagedArray);

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
            data[size] = new(data + size) T(val);
            return size++;
        };

        void pop_back()
        {
            ASSERT(size > 0);
            T& val = data[--size];
            val.~T();
        }

        template<typename ...Args>
        size_t create_back(Args... args)
        {
            if (size >= capacity) {
                reserve(capacity + 1);
            }
            data[size] = new(data + size) T{ args... };
            return size++;
        };

        // Grow both reserves and increases the size, so new items are indexable
        size_t grow(size_t additional_slots)
        {
            size_t new_size = size + additional_slots;
            if (additional_slots + size > capacity) {
                reserve(new_size);
            }
            for (size_t i = size; i < new_size; i++) {
                data[i] = new(data + i) T();
            }
            size = new_size;
            return size;
        };

        // Reserve grows the allocation, but does not increase the size.
        // Do not try to index into the new space though, push to increase size first
        // or use `grow` instead
        size_t reserve(size_t new_capacity)
        {
            if (new_capacity < capacity) {
                debug_print(L"Array can only grow at the moment!");
                return capacity;
            }

            const SysInfo& sys_info = get_sys_info();
            size_t pages_used = size_t(ceil(double(capacity * sizeof(T)) / double(sys_info.page_size)));
            size_t current_size = pages_used * sys_info.page_size;
            void* current_end = (void*)(u64(data) + current_size);

            size_t additional_memory_required = (new_capacity * sizeof(T)) - current_size;
            // Grows the new memory required to the next page boundary
            size_t additional_pages_required = size_t(ceil(double(additional_memory_required) / double(sys_info.page_size)));
            additional_memory_required = additional_pages_required * sys_info.page_size;

            // Commit the new page(s) of our reserved virtual memory
            memory::virtual_commit(current_end, additional_memory_required);
            // Update capacity to reflect new size
            capacity = (current_size + additional_memory_required) / sizeof(T);
            // Since capacity is potentiall larger than just old_capacity + additional_slots, we return it
            return capacity;
        }

        void empty()
        {
            for (size_t i = 0; i < size; i++) {
                data[i].~T();
            }
            size = 0;
        }

        size_t find_index(const T value_to_compare, const size_t starting_idx = 0)
        {
            for (size_t i = starting_idx; i < size; i++) {
                if (data[i] == value_to_compare) {
                    return i;
                }
            }
            return UINT64_MAX;
        }
    };


} // namespace zec
