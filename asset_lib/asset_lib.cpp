#include "asset_lib.h"
#include <vector>
#include <string>
#include <fstream>

#include <core/tracked_allocator.h>
#include <utils/utils.h>

namespace zec::assets
{
    void release(IAllocator& allocator, AssetFile& asset)
    {
        asset.num_components = 0;
        allocator.release(asset.components_blob);
        allocator.release(asset.binary_blob);
    }
    
    // Header format (version 0)
    // version   - uint32_t (4 bytes)
    // num_components   - uint32_t (4 bytes)
    // components_size - size_t (8 bytes)
    // blob_size - size_t (8 bytes)

    ZecResult save_binary_file(const char* path, const AssetFile& asset)
    {
        std::ofstream outfile;
        outfile.open(path, std::ios::binary | std::ios::out);

        outfile.write(reinterpret_cast<const char*>(&asset.version), sizeof(asset.version));
        outfile.write(reinterpret_cast<const char*>(&asset.num_components), sizeof(asset.num_components));
        outfile.write(reinterpret_cast<const char*>(&asset.components_blob.byte_size), sizeof(asset.components_blob.byte_size));
        outfile.write(reinterpret_cast<const char*>(&asset.binary_blob.byte_size), sizeof(asset.binary_blob.byte_size));
        
        outfile.write(reinterpret_cast<const char*>(asset.components_blob.ptr), asset.components_blob.byte_size);
        outfile.write(reinterpret_cast<const char*>(asset.binary_blob.ptr), asset.binary_blob.byte_size);

        outfile.close();
        return ZecResult::SUCCESS;
    }
    
    ZecResult load_binary_file(const char* path, IAllocator& allocator, AssetFile& asset)
    {
        std::ifstream infile;
        infile.open(path, std::ios::binary | std::ios::in);

        infile.read(reinterpret_cast<char*>(&asset.version), sizeof(asset.version));
        infile.read(reinterpret_cast<char*>(&asset.num_components), sizeof(asset.num_components));
        infile.read(reinterpret_cast<char*>(&asset.components_blob.byte_size), sizeof(asset.components_blob.byte_size));
        infile.read(reinterpret_cast<char*>(&asset.binary_blob.byte_size), sizeof(asset.binary_blob.byte_size));

        asset.components_blob = allocator.alloc(asset.components_blob.byte_size);
        asset.binary_blob = allocator.alloc(asset.binary_blob.byte_size);

        infile.read(reinterpret_cast<char*>(asset.components_blob.ptr), asset.components_blob.byte_size);
        infile.read(reinterpret_cast<char*>(asset.binary_blob.ptr), asset.binary_blob.byte_size);

        infile.close();
        return ZecResult::SUCCESS;
    }
}

