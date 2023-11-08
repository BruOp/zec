#pragma once
#include "core/zec_types.h"

namespace zec::render_graph
{
    struct ResourceIdentifier
    {
        u32 identifier = UINT32_MAX;

        bool operator==(const ResourceIdentifier& other) const = default;

        bool is_valid()
        {
            return identifier != UINT32_MAX;
        }
    };

    using BufferId = ResourceIdentifier;
    using TextureId = ResourceIdentifier;
    using ResourceLayoutId = ResourceIdentifier;
    using PipelineId = ResourceIdentifier;
}

template<>
struct std::hash<zec::render_graph::ResourceIdentifier>
{
    size_t operator()(const zec::render_graph::ResourceIdentifier& id) const
    {
        return static_cast<size_t>(id.identifier);
    }
};
