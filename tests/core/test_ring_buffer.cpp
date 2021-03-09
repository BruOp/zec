#include "catch2/catch.hpp"
#include "core/ring_buffer.h"

TEST_CASE("Ring Buffer can be initialized with a size")
{
    size_t size = 1024;
    zec::RingBuffer<uint32_t> ring{ size };
    REQUIRE(ring.size() == 0);
    REQUIRE(ring.remaining_capacity() == size);
}

TEST_CASE("Ring buffer can push elements into the back")
{
    zec::RingBuffer<uint32_t> ring{ 128 };
    ring.push_back(1u);
    REQUIRE(ring.data[0] == 1u);
}

TEST_CASE("Ring buffer can pop from front")
{
    const uint32_t el = 10u;
    zec::RingBuffer<uint32_t> ring{ 128 };

    ring.push_back(el);
    ring.push_back(20u);

    REQUIRE(ring.pop_front() == el);
}

TEST_CASE("Ring buffer front can be accessed")
{
    zec::RingBuffer<uint32_t> ring{ 128 };
    ring.push_back(1u);
    ring.push_back(2u);

    REQUIRE(ring.front() == 1u);
}

TEST_CASE("Ring buffer back can be accessed")
{
    zec::RingBuffer<uint32_t> ring{ 128 };
    ring.push_back(1u);
    ring.push_back(2u);

    REQUIRE(ring.back() == 2u);
}

TEST_CASE("An ring buffer will double in size when full")
{
    zec::RingBuffer<uint32_t> ring{ 8 };
    size_t old_capacity = ring.capacity;
    for (size_t i = 0; i <= old_capacity; i++) {
        ring.push_back(i + 1);
    }
    REQUIRE(ring.capacity == old_capacity * 2);
}

