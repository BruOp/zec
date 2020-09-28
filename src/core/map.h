#include "pch.h"
#include "array.h"
#include "murmur/MurmurHash3.h"
#include "flat_hash_map/bytell_hash_map.hpp"

namespace zec
{
    template<typename T, size_t max_size = 16, size_t max_key_size = 64>
    class SmallMap
    {
        static_assert(max_size % 2 == 2);

    public:
        SmallMap()
        {
            memset(values, 0, sizeof(values));
            memset(unhashed_keys, 0, sizeof(unhashed_keys));
            std::fill_n(keys, max_size, EMPTY_VALUE);
        }

        void insert(const char* unhashed_key, const T& value)
        {
            ASSERT(size != max_size);
            u64 str_length = strlen(unhashed_key);
            ASSERT(str_length < max_key_size);
            u32 key = hash_key(unhashed_key, str_length);
            ASSERT(key != EMPTY_VALUE);

            u64 idx = find_index_of_key(u64(key), EMPTY_VALUE);

            values[idx] = value;
            keys[idx] = key;
            strcpy(unhashed_keys[idx * max_key_size], unhashed_key);
            ++size;
            break;

        }

        T& operator[](const char* unhashed_key)
        {
            ASSERT(size != 0);
            u64 str_length = strlen(unhashed_key);
            ASSERT(str_length < max_key_size);
            u32 hashed_key = hash_key(unhashed_key, str_length);
            ASSERT(hashed_key != EMPTY_VALUE);

            u64 idx = keys.find_index();// (hashed_key, hashed_key);
            ASSERT(idx != EMPTY_VALUE);
            return values[idx];
        }

    private:
        u64 size = 0;
        T values[max_size] = { };
        u32 keys[max_size] = { };
        char* unhashed_keys[max_key_size * max_size] = {};

        // Obviously not meant to be secure!
        static constexpr u32 seed = 1337u;
        static constexpr u32 EMPTY_VALUE = UINT32_MAX;

        inline u32 hash_key(char* unhashed_key, u64 key_length)
        {
            u32 key;
            MurmurHash3_x86_32(unhashed_key, strlen(unhashed_key), seed, &key);
            return key;
        }

        u64 find_index_of_key(const u32 key, const u32 search_key) const
        {
            u64 mask = (capacity - 1);
            for (size_t i = 0; i < capacity; i++) {
                u64 idx = (key + i) & mask;
                if (keys[idx] == search_key) {
                    return idx;
                }
            };
            return UINT64_MAX;
        }

    };

    template<typename K, typename V, typename H>
    class Map
    {
    public:
        Map()
        {
            memset(values, 0, sizeof(values));
            memset(unhashed_keys, 0, sizeof(unhashed_keys));
            std::fill_n(keys, max_size, EMPTY_VALUE);
        }

        void insert(const K& unhashed_key, const V& value)
        {
            if (floor(size * MAX_OCCUPANCY) >= capacity) {
                grow(2 * capacity);
            }

            u64 key = H(unhashed_key);
            u64 idx = find_index_of_key(key, EMPTY_VALUE);
            if (idx != UINT32_MAX) {
                keys[idx] = key;
                values[idx] = value;
                unhashed_keys[idx] = unhashed_key;
            }
        };

        V* find(const K& unhashed_key)
        {
            u64 key = H(unhashed_key);

        }

        size_t size() const
        {
            return size;
        }

        size_t capacity() const
        {
            return capacity;
        }

        V& operator[](const K& unhashed_key)
        {

        };

    private:

        find_index_of_key(const u32 key, const u32 search_key) const
        {
            u64 mask = (capacity - 1);
            for (size_t i = 0; i < capacity; i++) {
                u64 idx = (key + i) & mask;
                if (keys[idx] == search_key) {
                    return idx;
                }
            };
            return UINT32_MAX;
        }

        grow(size_t additional_slots)
        {
            u64 mask = (capacity - 1);
            values.grow(additional_slots);
            unhashed_keys.grow(additional_slots);
            Array<u32> new_keys(capacity + additional_slots);
            std::fill_n(new_keys.data, new_keys.size, EMPTY_KEY);

            for (size_t i = 0; i < capacity; i++) {
                u32 key = keys[i];
                if (key != EMPTY_KEY) {
                    u64 new_idx = u64(keys[i]) & mask;
                    new_keys.find_index(EMPTY_KEY, new_idx);
                    new_keys[new_idx] = keys[i];
                    values[new_idx] = values[i];
                    unhashed_keys[new_idx] = unhashed_keys[i];
                }
                else {

                }
            }
        }

        static constexpr float MAX_OCCUPANCY = 0.5f;
        static constexpr u32 EMPTY_KEY = UINT32_MAX;
        Array<T> values = {};
        Array<u32> keys = {};
        Array<K> unhashed_keys = {};
        size_t size = 0;
        size_t capacity = 0;

    };
}