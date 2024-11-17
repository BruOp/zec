#include "asset_lib.h"
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include <core/allocators.hpp>
#include <utils/utils.h>

namespace zec::assets
{
    void release(IAllocator& allocator, AssetFile& asset)
    {
        allocator.free(asset.components_blob.ptr);
        allocator.free(asset.binary_blob.ptr);
        asset.components_blob = {};
        asset.binary_blob = {};
    }

    void release(IAllocator& allocator, MeshAsset& asset)
    {
        allocator.free(asset.components_blob.ptr);
        allocator.free(asset.index_buffers.ptr);
        for (size_t i = 0; i < ARRAY_SIZE(asset.vertex_buffers); i++)
        {
            allocator.free(asset.vertex_buffers[i].ptr);
        }
        asset = MeshAsset{};
    }

    // Header format (version 0)
    struct MeshAssetHeader
    {
        u32 version = 0;
        u32 num_component_descs = 0;
        size_t components_blob_byte_size = 0;
        size_t index_buffer_byte_size = 0;
        size_t vertex_buffers_byte_sizes[size_t(rhi::MeshAttribute::COUNT)] = {};
        size_t texture_paths_buffer_byte_size = 0;
    };

    template<typename TDesc>
    void add_component_desc(
        const TypedArrayView<TDesc>& desc_array,
        const AssetComponentType type,
        FixedArray<AssetComponentDesc, size_t(AssetComponentType::Count)>& component_desc_array,
        size_t& component_desc_offset)
    {
        component_desc_array.push_back({
            .type = type,
            .num_components = u32(desc_array.get_size()),
            .byte_offset = component_desc_offset
        });
        component_desc_offset += desc_array.get_byte_size();
    }

    ZecResult save_binary_file(const char* path, const MeshAsset& asset)
    {
        // COMPONENT DESCS
        FixedArray<AssetComponentDesc, size_t(AssetComponentType::Count)> component_descs{};

        size_t component_desc_offset = 0;
        add_component_desc(asset.render_nodes, AssetComponentType::RenderNode, component_descs, component_desc_offset);
        add_component_desc(asset.transform_nodes, AssetComponentType::TransformNode, component_descs, component_desc_offset);
        add_component_desc(asset.meshes, AssetComponentType::Mesh, component_descs, component_desc_offset);
        add_component_desc(asset.aabbs, AssetComponentType::AABB, component_descs, component_desc_offset);
        add_component_desc(asset.submeshes, AssetComponentType::Submesh, component_descs, component_desc_offset);
        add_component_desc(asset.materials, AssetComponentType::Material, component_descs, component_desc_offset);
        add_component_desc(asset.textures, AssetComponentType::Texture, component_descs, component_desc_offset);

        std::ofstream outfile;
        outfile.open(std::filesystem::path{ path }, std::ios::binary | std::ios::out);

        MeshAssetHeader header = {
            .version = asset.version,
            .num_component_descs = u32(component_descs.size),
            .components_blob_byte_size = asset.components_blob.byte_size,
            .index_buffer_byte_size = asset.index_buffers.byte_size,
            .texture_paths_buffer_byte_size = asset.texture_paths.byte_size,
        };
        for (size_t i = 0; i < ARRAY_SIZE(asset.vertex_buffers); i++)
        {
            header.vertex_buffers_byte_sizes[i] = asset.vertex_buffers[i].byte_size;
        }
        outfile.write(reinterpret_cast<const char*>(&header), sizeof(header));

        outfile.write(reinterpret_cast<const char*>(&component_descs.data), component_descs.get_byte_size());

        // BINARY DATA
        outfile.write(reinterpret_cast<const char*>(asset.components_blob.ptr), asset.components_blob.byte_size);
        outfile.write(reinterpret_cast<const char*>(asset.index_buffers.ptr), asset.index_buffers.byte_size);
        for (size_t i = 0; i < ARRAY_SIZE(asset.vertex_buffers); i++)
        {
            outfile.write(reinterpret_cast<const char*>(asset.vertex_buffers[i].ptr), asset.vertex_buffers[i].byte_size);
        }
        outfile.write(reinterpret_cast<const char*>(asset.texture_paths.ptr), asset.texture_paths.byte_size);

        outfile.close();
        return ZecResult::SUCCESS;
    }

    ZecResult load_binary_file(const char* path, IAllocator& allocator, MeshAsset& asset)
    {
        // TODO: Gracefully handle loading incorrect data
        // TODO: Add some sort of key to ensure we're actually loading the type of file we expect
        // TODO: Add debug names for each entry
        // TOOD: Handle multiple Asset types, not just Meshes

        std::ifstream infile;
        infile.open(path, std::ios::binary | std::ios::in);
        if (infile.fail())
        {
            return ZecResult::FAILURE;
        }
        MeshAssetHeader header = {};

        infile.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (infile.fail())
        {
            return ZecResult::FAILURE;
        }
        asset.version = header.version;

        asset.components_blob = { .byte_size = header.components_blob_byte_size, .ptr = allocator.allocate(header.components_blob_byte_size) };
        asset.index_buffers = { .byte_size = header.index_buffer_byte_size, .ptr = allocator.allocate(header.index_buffer_byte_size) };
        for (size_t i = 0; i < ARRAY_SIZE(asset.vertex_buffers); i++)
        {
            if (header.vertex_buffers_byte_sizes[i] > 0)
            {
                asset.vertex_buffers[i] = {
                    .byte_size = header.vertex_buffers_byte_sizes[i],
                    .ptr = allocator.allocate(header.vertex_buffers_byte_sizes[i]) };
            }
        }
        asset.texture_paths = { .byte_size = header.texture_paths_buffer_byte_size, .ptr = allocator.allocate(header.texture_paths_buffer_byte_size) };
        FixedArray<AssetComponentDesc, size_t(AssetComponentType::Count)> component_descs{};
        ASSERT(component_descs.capacity() == header.num_component_descs);
        for (size_t i = 0; i < component_descs.capacity(); i++)
        {
            AssetComponentDesc component_desc{};
            infile.read(reinterpret_cast<char*>(&component_desc), sizeof(component_desc));
            component_descs.push_back(component_desc);
        }

        infile.read(reinterpret_cast<char*>(asset.components_blob.ptr), header.components_blob_byte_size);
        infile.read(reinterpret_cast<char*>(asset.index_buffers.ptr), header.index_buffer_byte_size);
        for (size_t i = 0; i < ARRAY_SIZE(asset.vertex_buffers); i++)
        {
            if (header.vertex_buffers_byte_sizes[i] > 0)
            {
                infile.read(reinterpret_cast<char*>(asset.vertex_buffers[i].ptr), header.vertex_buffers_byte_sizes[i]);
            }
        }
        infile.read(reinterpret_cast<char*>(asset.texture_paths.ptr), header.texture_paths_buffer_byte_size);

        infile.close();

        // Process desc components
        for (const auto& component_desc : component_descs)
        {
            switch (component_desc.type)
            {
            case AssetComponentType::RenderNode:
                ASSERT(asset.render_nodes.get_size() == 0);
                asset.render_nodes = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::TransformNode:
                ASSERT(asset.transform_nodes.get_size() == 0);
                asset.transform_nodes = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::Mesh:
                ASSERT(asset.meshes.get_size() == 0);
                asset.meshes = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::AABB:
                ASSERT(asset.aabbs.get_size() == 0);
                asset.aabbs = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::Submesh:
                ASSERT(asset.submeshes.get_size() == 0);
                asset.submeshes = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::Material:
                ASSERT(asset.materials.get_size() == 0);
                asset.materials = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            case AssetComponentType::Texture:
                ASSERT(asset.textures.get_size() == 0);
                asset.textures = { component_desc.num_components, static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            //case AssetComponentType::Sampler:
                //ASSERT(asset.samplers.get_size()=!= 0);
                //asset.samplers = component_desc.num_components, { static_cast<u8*>(asset.components_blob.ptr) + component_desc.byte_offset };
                break;
            default:
                ASSERT_FAIL("We don't support this component type! Is it Invalid?");
                break;
            }
        }
        return ZecResult::SUCCESS;
    }
}

