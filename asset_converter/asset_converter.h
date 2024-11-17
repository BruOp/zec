#include "core/zec_types.h"

namespace zec
{
    class IAllocator;
}

namespace zec::assets
{
    struct MeshAsset;
}

namespace zec::rhi
{
    enum struct MeshAttribute : u8;
}

namespace zec::asset_converter
{
    enum LoaderFlags : u32
    {
        GLTF_LOADING_FLAG_NONE = 0,
        // We're loading a glb
        GLTF_LOADING_BINARY_FORMAT = (1 << 0),
    };

    constexpr u32 get_attr_index_by_type(const rhi::MeshAttribute mesh_attribute);
}
