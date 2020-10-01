#include "catch2/catch.hpp"
#include "core/Map.h"

struct ResourceHandle
{
    u32 idx;
};

struct Resource
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Hasher
{
    u32 operator()(const ResourceHandle handle) { return handle.idx; };
};

bool operator==(const Resource& a, const Resource& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

TEST_CASE("Can insert items into map")
{
    zec::Map<ResourceHandle, Resource, Hasher> map{};
    ResourceHandle handle = { 17 };
    Resource resource = { 1.0f, 2.0f, 3.0f };
    map.insert(handle, resource);
    REQUIRE(map.size() == 1);
    REQUIRE(map[handle] == resource);
}

TEST_CASE("Conflicts are resolved peacefully")
{
    // This is highly implementation specific, but let's try to insert two keys that we know will overlap
    zec::Map<ResourceHandle, Resource, Hasher> map{};
    ResourceHandle handle1 = { 16 };
    ResourceHandle handle2 = { handle1.idx + map.capacity() };
    Resource resource1 = { 1.0f, 2.0f, 3.0f, 4.0f };
    Resource resource2 = { 5.0f, 2.0f, 3.0f, 4.0f };
    map.insert(handle1, resource1);
    map.insert(handle2, resource2);
    REQUIRE(map.size() == 2);
    REQUIRE(map[handle1] == resource1);
    REQUIRE(map[handle2] == resource2);
};

TEST_CASE("Map will grow once max occupancy is exceeded")
{
    zec::Map<ResourceHandle, Resource, Hasher> map{};
    size_t N = map.capacity();
    for (size_t i = 0; i < N; i++) {
        map.insert({ N + i }, Resource{ float(i) });
    }
    REQUIRE(map.capacity() == N * 2);
};

TEST_CASE("Items still accessible after growth")
{
    zec::Map<ResourceHandle, Resource, Hasher> map{};
    size_t N = map.capacity();
    for (size_t i = 0; i < N; i++) {
        map.insert({ N + i }, Resource{ float(i) });
    }
    for (size_t i = 0; i < map.capacity(); i++) {
        REQUIRE(map[{ i + map.capacity() }] == Resource{ float(i) });
    }
};
