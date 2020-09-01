#pragma once
#include "pch.h"
#include "array.h"

namespace zec
{
    template<typename T>
    class RingBuffer
    {
    public:
        u64 push_back(T value)
        {
            ASSERT(write_idx != read_idx + capacity);
            array[write_idx % capacity] = value;
            return write_idx++;
        }

        T pop_front()
        {
            ASSERT(read_idx != write_idx);
            return read_idx
        }


        T& operator[](const u64 idx)
        {
            ASSERT(idx < end);
            return array[idx % array.capacity];
        }
        const T& operator[](const u64 idx) const
        {
            ASSERT(idx < end);
            return array[idx % array.capacity];
        }

        u64 capacity;
    private:
        u64 read_idx;
        u64 write_idx;
        Array<T> array;
    };
}