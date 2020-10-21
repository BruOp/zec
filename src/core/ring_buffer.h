#pragma once
#include "pch.h"
#include "array.h"

namespace zec
{
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
            return elements[read_idx % elements.size];
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
}