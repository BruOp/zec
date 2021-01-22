#pragma once
#include "pch.h"
#include "array.h"

namespace zec
{
    template<typename T, size_t Capacity>
    class FixedRingBuffer
    {
    public:
        FixedArray<T, Capacity> elements;
        u64 read_idx = 0;
        u64 write_idx = 0;

        FixedRingBuffer() :
            elements{ },
            read_idx{ 0 },
            write_idx{ 0 }
        {
            elements.size = Capacity; // Allows us to access all the elements using indexing
        }

        void push_back(T item)
        {
            ASSERT(remaining_capacity());
            const u64 idx = (write_idx) % Capacity;
            elements[idx] = item;
            ++write_idx;
        };

        T pop_front()
        {
            ASSERT(read_idx != write_idx);
            T element = elements[read_idx % Capacity];
            ++read_idx;
            return element;
        };

        inline T front()
        {
            ASSERT(write_idx > read_idx);
            return elements[read_idx % Capacity];
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
            return elements.capacity - write_idx + read_idx;
        }

        inline void reset()
        {
            read_idx = write_idx;
        };

    };

    template<typename T>
    class RingBuffer
    {
    public:
        T* data = nullptr;
        size_t capacity = 0;
        u64 read_idx = 0;
        u64 write_idx = 0;

        RingBuffer() : data{ nullptr }, capacity{ 0 }, read_idx{ 0 }, write_idx{ 0 } { };
        RingBuffer(size_t _capacity) : data{ nullptr }, capacity{ 0 }, read_idx{ 0 }, write_idx{ 0 }
        {
            grow(_capacity);
        };

        ~RingBuffer()
        {
            if (data != nullptr) {
                memory::free_mem(data);
            }
        }

        void push_back(T item)
        {
            if (write_idx - read_idx >= capacity) {
                grow(capacity != 0 ? 2 * capacity : 16);
            };
            const u64 idx = (write_idx) % capacity;
            data[idx] = item;
            ++write_idx;
        };

        T pop_front()
        {
            ASSERT(read_idx < write_idx);
            T element = data[read_idx % capacity];
            ++read_idx;
            return element;
        };

        inline T front()
        {
            ASSERT(read_idx < write_idx);
            return data[read_idx % capacity];
        }

        T back()
        {
            ASSERT(write_idx > read_idx);
            return data[write_idx - 1];
        };

        inline u64 size() const
        {
            return write_idx - read_idx;
        }

        inline u64 remaining_capacity() const
        {
            return capacity - write_idx + read_idx;
        }

        inline void reset()
        {
            read_idx = write_idx;
        };

        inline void grow(const size_t additional_slots)
        {
            size_t new_capacity = capacity + additional_slots;
            const SysInfo& sys_info = get_sys_info();
            size_t memory_required = (new_capacity * sizeof(T));
            size_t pages_required = size_t(ceil(double(memory_required) / double(sys_info.page_size)));

            T* new_data = reinterpret_cast<T*>(memory::alloc(pages_required * sys_info.page_size));
            size_t r = capacity != 0 ? read_idx % capacity : read_idx;
            size_t w = capacity != 0 ? write_idx % capacity : write_idx;

            if (w < r) {
                // The ring buffer is wrapping, need two copies
                size_t count = capacity - r;
                memcpy(new_data, &data[read_idx], count * sizeof(T));
                memcpy(new_data + count, data, (w + 1) * sizeof(T));
            }
            else if (w > r) {
                memcpy(new_data, &data[read_idx], (w - r) * sizeof(T));
            }
            read_idx = 0;
            write_idx = write_idx - read_idx;
            if (data) {
                memory::free_mem(data);
            }
            data = new_data;
            capacity = (pages_required * sys_info.page_size) / sizeof(T);
        }
    };
}