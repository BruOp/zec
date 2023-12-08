#include "bit_list.h"

namespace zec
{
    void BitList::init(zec::IAllocator* allocator, const size_t capacity)
    {
        size_t initial_capacity = (capacity + k_bits_per_word - 1) / k_bits_per_word;
        bit_capacity = initial_capacity * k_bits_per_word;
        array.init(allocator, initial_capacity);
    }

    bool BitList::is_bit_set(size_t index) const
    {
        WordType mask = (1u << u32(index % k_bits_per_word));
        WordType word = array[index / k_bits_per_word];
        return (word & mask);
    }

    BitList::WordType BitList::get_word(size_t word_index) const
    {
        return array[word_index];
    }

    BitList::WordType& BitList::get_word(size_t word_index)
    {
        return array[word_index];
    }

    size_t BitList::grow_bit_count(size_t additional_entries)
    {
        size_t num_additional_words = (additional_entries + k_bits_per_word - 1) / k_bits_per_word;
        size_t new_word_count = array.grow(num_additional_words);
        // Initialize our bits to zero
        memset(&array[array.get_size() - num_additional_words], 0, num_additional_words * k_word_bytesize);
        return new_word_count * k_bits_per_word;
    }

    void BitList::set_entry(size_t index, bool state)
    {
        WordType element = array[index / k_bits_per_word];
        WordType n = index % k_bits_per_word;
        bool bit_is_set = (element & (1u << n)) == (1u << n);
        // This is a bit messed up, might have to change to something more sensible lol
        num_set_bits = num_set_bits
            + (~WordType(bit_is_set) & WordType(state)) // Set bit count decreases if bit was previously set but now unset
            - (WordType(bit_is_set) & ~WordType(state));// Set bit count increases if bit was previously unset but now set
        array[index / k_bits_per_word] = (element & ~(1u << n)) | (WordType(state) << n);
    }

    void BitList::copy(const BitList& other, zec::IAllocator* allocator)
    {
        bit_capacity = other.bit_capacity;
        num_set_bits = other.num_set_bits;
        array.init(allocator, other.bit_capacity / 8);
        array.grow(other.array.get_size());
        zec::memory::copy(array.begin(), other.array.begin(), other.array.get_byte_size());
    }
}