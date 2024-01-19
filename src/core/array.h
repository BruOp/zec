#pragma once
#include <math.h>
#include <type_traits>
#include <initializer_list>

#include "utils/assert.h"
#include "utils/memory.h"
#include "allocators.hpp"
namespace zec
{
    static constexpr size_t g_GB = 1024 * 1024 * 1024;

    struct ArrayView
    {
        u32 size = 0;
        u32 offset = 0;

        u32 get_size() { return size; };
    };

    // Only for POD types
    template<typename T>
    class TypedArrayView
    {
    public:
        TypedArrayView() = default;
        template<size_t N>
        TypedArrayView(const T(&arr)[N]) : data(arr), size(N) { };
        TypedArrayView(const size_t size, T* ptr) : data(ptr), size(size) {};
        TypedArrayView(const size_t size, void* ptr) : data(static_cast<T*>(ptr)), size(size) {};

        TypedArrayView(const TypedArrayView<T>& other) = default;
        TypedArrayView& operator=(const TypedArrayView<T>& other) = default;
        TypedArrayView(TypedArrayView<T>&& other) = default;
        TypedArrayView& operator=(TypedArrayView<T>&& other) = default;

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

        bool operator==(const TypedArrayView<T>& other) const = default;

        inline size_t get_size() const { return size; }
        size_t get_byte_size() const { return sizeof(T) * size; }

        T* begin() { return data; }
        const T* begin() const { return data; }

        T* end() { return data + size; }
        const T* end() const { return data + size; }

    private:
        size_t size = 0;
        T* data = nullptr;
    };

    template<typename T, size_t Capacity>
    class FixedArray
    {
        static_assert(std::is_trivially_copyable<T>::value&& std::is_trivially_destructible<T>::value);
    public:
        T data[Capacity] = {};
        size_t size = 0;

        FixedArray() = default;
        constexpr FixedArray(std::initializer_list<T> init_list) : FixedArray()
        {
            for (const T& element : init_list)
            {
                data[size++] = element;
            };
        };

        FixedArray(const FixedArray<T, Capacity>& other) = default;
        //{
        //    memory::copy(data, other.data, other.size * sizeof(T));
        //};

        FixedArray& operator=(const FixedArray<T, Capacity>& other) = default;
        //{
        //    size = other.size;
        //    memory::copy(data, other.data, other.size * sizeof(T));
        //};

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
            ASSERT(size < Capacity);
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
            ASSERT(size < Capacity);
            data[size] = T{ args... };
            return size++;
        };

        // Grow both reserves and increases the size, so new items are indexable
        size_t grow(size_t additional_slots)
        {
            ASSERT(additional_slots + size > Capacity);
            size += additional_slots;
            return size;
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

        size_t get_byte_size() const
        {
            return sizeof(T) * size;
        }

        inline size_t capacity() { return Capacity; };
    };

    template <typename T>
    class VirtualArray
    {
        /*
        This is basically equivalent to std::vector, with a few key exceptions:
        - ONLY FOR POD TYPES! We don't initialize or destroy the elements and use things like memcpy for copies
        - it's guaranteed to never move since we're using virtual alloc to allocate about 1 GB per array (in Virtual memory)
        */

        static_assert(std::is_trivially_copyable<T>::value);
        static_assert(std::is_trivially_destructible<T>::value);
    public:
        T* data = nullptr;
        size_t capacity = 0;
        size_t size = 0;

        VirtualArray() : VirtualArray{ 0 } { };
        VirtualArray(size_t cap) : data{ nullptr }, capacity{ 0 }, size{ 0 } {
            data = static_cast<T*>(memory::virtual_reserve((void*)data, g_GB));
            grow(cap);
        }

        ~VirtualArray()
        {
            if (data != nullptr) {
                memory::virtual_free((void*)data);
            }
        };

        T* begin() { return data; }
        const T* begin() const { return data; }

        T* end() { return data + size; }
        const T* end() const { return data + size; }

        VirtualArray(VirtualArray& other) = delete;
        VirtualArray& operator=(VirtualArray& other) = delete;

        VirtualArray(VirtualArray&& other) = default;
        VirtualArray& operator=(VirtualArray&& other) = default;

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
                // Array can only grow at the moment!
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

        size_t get_size() const { return size; };

        size_t get_byte_size() const
        {
            return sizeof(T) * size;
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

        ManagedArray(ManagedArray& other) = delete;
        ManagedArray& operator=(ManagedArray& other) = delete;

        ManagedArray(ManagedArray&& other) = default;
        ManagedArray& operator=(ManagedArray&& other) = default;

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
            new(data + size) T{ args... };
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
                new(data + i) T();
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
                // Array can only grow at the moment!
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

        inline size_t get_size() const { return size; }
        size_t get_byte_size() const
        {
            return sizeof(T) * size;
        }
    };

    template <typename T>
    class Array
    {
        /*
        This is basically equivalent to std::vector, with a few key exceptions:
        - ONLY FOR POD TYPES! We don't initialize or destroy the elements and use things like memcpy for copies
        - Unlike VirtualArray, we can't assume this doesn't move around.
        */
        static_assert(std::is_trivially_copyable<T>::value);
        static_assert(std::is_trivially_destructible<T>::value);
    public:
        Array() { };
        Array(size_t in_max_capacity) : max_capacity(in_max_capacity) { };

        ~Array()
        {
            if (data != nullptr) {
                ASSERT_FAIL("We should've already called shutdown!");
            }
        };

        UNCOPIABLE(Array);
        UNMOVABLE(Array);

        // Note that we'll be holding a reference to in_allocator!
        // If you've set a max capacity, feel free to omit initial capacity
        void init(IAllocator* in_allocator, const size_t inital_capacity = 0)
        {
            ASSERT(size == 0 && capacity == 0 && allocator == nullptr && data == nullptr);
            allocator = in_allocator;
            if (inital_capacity == 0 && max_capacity > 0)
            {
                reserve(max_capacity);
            }
            else
            {
                ASSERT(inital_capacity > 0);
                reserve(inital_capacity);
            }
        };

        void shutdown()
        {
            if (capacity > 0)
            {
                ASSERT(data != nullptr);
                allocator->free(data);
                data = nullptr;
                capacity = 0;
            }
            ASSERT(data == nullptr);
            size = 0;
        };

        T* begin() { return data; }
        const T* begin() const { return data; }

        T* end() { return data + size; }
        const T* end() const { return data + size; }

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

        // Grow increases the size AND if needed, the capacity. The former means that items are indexable
        size_t grow(size_t additional_slots)
        {
            if (additional_slots + size > capacity) {
                reserve(additional_slots + size);
            }
            size += additional_slots;
            return size;
        };

        // Reserve grows the allocation, but does not increase the size.
        // Do not try to index into the new space, push to increase size first
        // or use `grow` instead
        size_t reserve(size_t new_capacity)
        {
            if (new_capacity < capacity) {
                // Array can only grow at the moment!
                return capacity;
            }
            if (max_capacity > 0 && new_capacity > max_capacity)
            {
                ASSERT_FAIL("Cannot grow past max capacity %u, new capacity requested: %u!", max_capacity, new_capacity);
                return max_capacity;
            }
            ASSERT(capacity < UINT64_MAX); // Really should have a more sensible max but w/e

            // 1. We try to grow by a factor of 1.5
            // 2. If the requested capacity is larger, use that
            // 3. If we didn't do the above, make sure that our factor hasn't put us past the max capacity
            size_t capacity_to_alloc = capacity + (capacity / 2u);
            if (new_capacity > capacity_to_alloc)
            {
                capacity_to_alloc = new_capacity;
            }
            else if (max_capacity > 0 && capacity_to_alloc > max_capacity)
            {
                capacity_to_alloc = max_capacity;
            }

            size_t new_byte_size = capacity_to_alloc * sizeof(T);
            T* new_data = (T*)allocator->allocate(new_byte_size, alignof(T));
            if (capacity)
            {
                memory::copy(new_data, data, get_byte_size());
                allocator->free(data);
            }
            data = new_data;
            capacity = capacity_to_alloc;
            // Since capacity is potentiall larger than just old_capacity + additional_slots, we return it
            return capacity;
        }

        void empty()
        {
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

        size_t get_size() const { return size; };

        size_t get_byte_size() const
        {
            return sizeof(T) * size;
        }

        operator TypedArrayView<T>()
        {
            return TypedArrayView<T>{ get_size(), data };
        }
        operator TypedArrayView<const T>() const
        {
            return TypedArrayView<const T>{ get_size(), data };
        }

    private:
        inline constexpr static float growth_factor = 1.5f;
        IAllocator* allocator = nullptr;
        T* data = nullptr;
        size_t capacity = 0;
        const size_t max_capacity = 0;
        size_t size = 0;
    };

} // namespace zec
