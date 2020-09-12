#include "pch.h"
#include "array.h"
#include "murmur/MurmurHash3.h"

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
            ASSERT(key != INVALID_VALUE);

            u64 idx = find_index_of_key(u64(key), INVALID_VALUE);
            ASSERT(idx != INVALID_VALUE);

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

            u64 idx = find_index_of_key(hashed_key, hashed_key);
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

        u64 find_index_of_key(const u64 starting_idx, const u32 search_key)
        {
            constexpr u64 mask = max_size - 1;
            for (size_t i = 0; i < max_size; ++i) {
                u64 idx = (hashed_key + i) & max;
                if (keys[idx] == search_key) {
                    return idx;
                }
            }
            return UINT32_MAX;
        }
    };

    //template<typename T, size_t max_key_size = 64>
    //class Map
    //{
    //    Array<T> values = {};
    //    Array<u32> keys = {};
    //    Array<char[64]> unhashed_keys = {};
    //};
}