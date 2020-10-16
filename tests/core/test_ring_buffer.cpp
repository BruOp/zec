#include "catch2/catch.hpp"
#include "core/ring_buffer.h"

TEST_CASE("Ring Buffer can be initialized with a size")
{
    size_t size = 1024;
    zec::RingBuffer<uint32_t> ring{ size };
    REQUIRE(ring.size() == 0);
    REQUIRE(ring.remaining_capacity() == size);
}

TEST_CASE("Ring Buffer can be initialized without a size")
{
    zec::RingBuffer<uint32_t> ring{};
    REQUIRE(ring.size() == 0);
    REQUIRE(ring.remaining_capacity() == 0);
}

TEST_CASE("Ring buffer will grow if it runs out of space")
{
    zec::RingBuffer<uint32_t> ring{ 0 };
    REQUIRE(ring.remaining_capacity() == 0);
    ring.push_back(10u);
    REQUIRE(ring.remaining_capacity() > 1);
}

TEST_CASE("Ring buffer can push elements into the back")
{
    zec::RingBuffer<uint32_t> ring{};
    ring.push_back(1u);
    REQUIRE(ring.elements[0] == 1u);
}

TEST_CASE("Ring buffer can pop from front")
{
    const uint32_t el = 10u;
    zec::RingBuffer<uint32_t> ring{};

    ring.push_back(el);
    ring.push_back(20u);

    REQUIRE(ring.pop_front() == el);
}

TEST_CASE("Ring buffer front can be accessed")
{
    size_t size = 1024;
    zec::RingBuffer<uint32_t> ring{ size };
    ring.push_back(1u);
    ring.push_back(2u);

    REQUIRE(ring.front() == 1u);
}

TEST_CASE("Ring buffer back can be accessed")
{
    size_t size = 1024;
    zec::RingBuffer<uint32_t> ring{ size };
    ring.push_back(1u);
    ring.push_back(2u);

    REQUIRE(ring.back() == 2u);

}

