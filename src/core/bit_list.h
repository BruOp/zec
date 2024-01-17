#pragma once
#include "array.h"

namespace zec
{
    class BitList
    {
    public:
        // Number of bits per word
        typedef u32 WordType;
        static constexpr size_t k_bits_per_word = sizeof(WordType) * 8;
        static constexpr size_t k_word_bytesize = sizeof(WordType);

        BitList() = default;
        ~BitList() = default;

        // TODO: Should we just support copy construction/assignment instead of the explicit copy function?
        UNCOPIABLE(BitList);

        void init(zec::IAllocator* allocator, const size_t capacity);

        void shutdown()
        {
            array.shutdown();
        }

        bool is_bit_set(size_t index) const;
        WordType get_word(size_t word_index) const;
        WordType& get_word(size_t word_index);
        size_t get_word_count() const { return array.get_size(); };
        size_t get_bit_size() const { return array.get_size() * k_bits_per_word; };
        size_t get_num_set_bits() const { return num_set_bits; };

        size_t grow_bit_count(size_t additional_entries);

        void set_entry(size_t index, bool state);

        void copy(const BitList& other, zec::IAllocator* allocator);

        bool operator==(const BitList& other) const;

    private:
        size_t bit_capacity = 0;
        size_t num_set_bits = 0;
        zec::Array<WordType> array{};
    };
}