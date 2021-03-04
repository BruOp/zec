#pragma once
#include "pch.h"
#include "utils/crc_hash.h"

#define PASS_RESOURCE_IDENTIFIER(x) { .id = ctcrc32(x), .name = x }

namespace clustered
{
    struct ResourceIdentifier
    {
        u64 id;
        const char* name;
    };

    namespace PassResources
    {
        constexpr ResourceIdentifier DEPTH_TARGET = PASS_RESOURCE_IDENTIFIER("depth");
        constexpr ResourceIdentifier HDR_TARGET = PASS_RESOURCE_IDENTIFIER("hdr");
        constexpr ResourceIdentifier SDR_TARGET = PASS_RESOURCE_IDENTIFIER("sdr");
        constexpr ResourceIdentifier SPOT_LIGHT_INDICES = PASS_RESOURCE_IDENTIFIER("spot light_indices");
        constexpr ResourceIdentifier POINT_LIGHT_INDICES = PASS_RESOURCE_IDENTIFIER("point light_indices");
    }
}

#undef PASS_RESOURCE_IDENTIFIER
