#pragma once
#include "core/array.h"
#include "core/zec_types.h"

namespace zec
{
    template <typename T, typename Handle>
    class ResourceArray : public Array<T>
    {
    public:
        T& operator[](const u64 i)
        {
            return this->data[i];
        }
        const T& operator[](const u64 i) const
        {
            return this->data[i];
        }

        T& operator[](const Handle handle)
        {
            return this->data[handle.idx];
        }
        const T& operator[](const Handle handle) const
        {
            return this->data[handle.idx];
        }
    };
}