#pragma once
#include "pch.h"
#include "core/array.h"

namespace zec
{
    template <typename T, typename Handle>
    class ResourceArray : public Array<T>
    {
    public:
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