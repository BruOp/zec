namespace zec 
{
    class IAllocator;
}

namespace zec::assets
{
    struct ModelAsset;
}

namespace zec::asset_converter
{
    enum LoaderFlags : u32
    {
        GLTF_LOADING_FLAG_NONE = 0,
        // We're loading a glb
        GLTF_LOADING_BINARY_FORMAT = (1 << 0),
    };

    void convert_from_gltf(const char* gltf_file_path, const LoaderFlags loaderFlags = GLTF_LOADING_FLAG_NONE, IAllocator& allocator, assets::ModelAsset& out_model_asset);
}
