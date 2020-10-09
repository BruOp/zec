
#pragma once
#include "pch.h"
#include "utils/utils.h"
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
            memory::copy((void*)&data[size], (void*)&val, sizeof(T));
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

        void insert_grow(size_t idx, const T& value)
        {
            if (idx > capacity - 1) {
                grow(idx + 1);
            }
            memory::copy((void*)&data[idx], (void*)&value, sizeof(T));
        }

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
            if (new_capacity <= capacity) {
                debug_print(L"Array can only grow at the moment!");
                return capacity;
            }

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

    template<typename T>
    class Stack
    {
        Array<T> elements;
    };

    template<typename T>
    class RingBuffer
    {
    public:
        Array<T> elements;
        u64 read_idx = 0;
        u64 write_idx = 0;

        RingBuffer(size_t capacity) : elements{ capacity }, read_idx{ 0 }, write_idx{ 0 } {};
        RingBuffer() : RingBuffer(0) { };

        void push_back(T item)
        {
            if (write_idx - read_idx >= elements.size) {
                // We've run out of space
                elements.grow(elements.size ? elements.size : 16);
            };
            const u64 idx = (write_idx) % elements.size;
            elements[idx] = item;
            ++write_idx;
        };

        T pop_front()
        {
            ASSERT(read_idx != write_idx);
            T element = elements[read_idx % elements.size];
            ++read_idx;
            return element;
        };

        inline T front()
        {
            ASSERT(write_idx > read_idx);
            return elements[read_idx];
        }

        T back()
        {
            ASSERT(write_idx > read_idx);
            return elements[write_idx - 1];
        };

        inline u64 size() const
        {
            return write_idx - read_idx;
        }

        inline u64 remaining_capacity() const
        {
            return elements.size - write_idx + read_idx;
        }

        inline void reset()
        {
            read_idx = write_idx;
        };
    };
} // namespace zec
