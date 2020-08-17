#include "catch2/catch.hpp"
#include "core/array.h"

struct Vector
{
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};
boolean operator==(const Vector& left, const Vector& right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

TEST_CASE("Array can be initialized with a size")
{
    size_t size = 1024;
    zec::Array<uint32_t> array{ size };
    REQUIRE(array.size == size);
    REQUIRE(array.capacity == size);
}

TEST_CASE("Array can be initialized without a size")
{
    zec::Array<uint32_t> array{};
    REQUIRE(array.size == 0);
    REQUIRE(array.capacity == 0);
}

TEST_CASE("Array elements can be access using square bracket operator")
{
    size_t size = 1024;
    zec::Array<uint32_t> array{ size };
    uint32_t value = array[0];
    REQUIRE(value == 0);
}

TEST_CASE("Elements can be pushed back into the array")
{
    const uint32_t el = 10;
    zec::Array<uint32_t> array{};
    array.push_back(el);
    REQUIRE(array.size == 1);
    REQUIRE(array[array.size - 1] == el);
}

TEST_CASE("Elements can be created at the back of the array")
{


    zec::Array<Vector> array{ };
    array.create_back(1u, 2u, 3u);
    const Vector expected{ 1u, 2u, 3u };
    REQUIRE(array.size == 1);
    REQUIRE(array[array.size - 1] == expected);
}

TEST_CASE("Array can grown")
{
    zec::Array<uint32_t> array{ 1024 };
    array.grow(1000);
    REQUIRE(array.size == 2024);
    // Capacity reflects the amount of memory we have available before we need to allocate a new page
    REQUIRE(array.capacity == 2048);
}
