#include "asset_converter.h"

#include <filesystem>
#include <utils/utils.h>
#include <asset_lib.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>
#include <DirectXTex.h>
#include "gfx/d3d12/dx_helpers.h"
#include "gfx/d3d12/dx_utils.h"
#include "gfx/rhi.h"


namespace zec::asset_converter
{
    bool noop_image_loader_callback(
        tinygltf::Image* image,
        const int image_idx,
        std::string* err,
        std::string* warn,
        int req_width,
        int req_height,
        const unsigned char* bytes,
        int size,
        void* user_data)
    {
        return true;
    };

    struct ChildParentPair
    {
        u32 child_idx = UINT32_MAX;
        u32 parent_idx = UINT32_MAX;
    };

    struct MeshArrayView
    {
        u32 offset = 0;
        u32 count = 1;
    };

    void traverse_scene_graph(const tinygltf::Model& model, const u32 parent_idx, const u32 node_idx, VirtualArray<ChildParentPair>& node_processing_list)
    {
        u32 new_parent_idx = u32(node_processing_list.push_back({ node_idx, parent_idx }));

        const tinygltf::Node& node = model.nodes[node_idx];
        for (i32 child_idx : node.children) {
            traverse_scene_graph(model, new_parent_idx, child_idx, node_processing_list);
        }
    }

    // Process the GLTF scene graph to get a flat list of nodes wherein the parent nodes
    // are always before the child nodes. We need to do this so we can compute global
    // transforms in a single pass.
    // The resulting list also provides a mapping between our_node_idx -> gltf_node_idx
    void flatten_gltf_scene_graph(const tinygltf::Model& model, VirtualArray<ChildParentPair>& node_processing_list)
    {
        u32 scene_idx = model.defaultScene >= 0 ? u32(model.defaultScene) : 0;
        ASSERT(model.scenes.size() > 0);
        const tinygltf::Scene& scene = model.scenes[scene_idx];
        VirtualArray<ChildParentPair> node_queue = {};
        node_processing_list.reserve(model.nodes.size());

        // Push root of scene tree into queue
        for (i32 root_idx : scene.nodes) {
            traverse_scene_graph(model, UINT32_MAX, root_idx, node_processing_list);
        }
    }


    void process_scene_graph(const tinygltf::Model& model, const VirtualArray<ChildParentPair>& node_processing_list, IAllocator& allocator, assets::MeshAsset& out_mesh_asset)
    {
        // Construct scene graph using node_processing_list
        const u64 num_nodes = node_processing_list.size;

        for (size_t node_idx = 0; node_idx < node_processing_list.size; node_idx++) {
            ChildParentPair pair = node_processing_list[node_idx];

            const tinygltf::Node& node = model.nodes[pair.child_idx];
            assets::TransformNode& transform_node = out_mesh_asset.transform_nodes[node_idx];
            transform_node.parent_transform = { pair.parent_idx };
            // Scale
            if (node.scale.size() > 0) {
                vec3& scale = transform_node.scale;
                for (size_t i = 0; i < node.scale.size(); i++) {
                    scale[i] = float(node.scale[i]);
                }
            }
            else {
                transform_node.scale = vec3{ 1.0f, 1.0f, 1.0f };
            }

            // Rotation
            if (node.rotation.size() > 0) {
                for (size_t i = 0; i < node.rotation.size(); i++) {
                    transform_node.rotation[i] = float(node.rotation[i]);
                }
            }
            else {
                transform_node.rotation = quaternion{ 0.f, 1.f, 0.f, 0.f };
            }

            // Translation
            if (node.translation.size() > 0) {
                for (size_t i = 0; i < node.translation.size(); i++) {
                    transform_node.translation[i] = float(node.translation[i]);
                }
            }
            else {
                transform_node.translation = vec3{};
            }
        }
    }

    void preprocess_texture(const std::filesystem::path& infile_path, std::filesystem::path& output_path)
    {
        DirectX::ScratchImage image;
        const auto& extension = infile_path.extension();

        if (extension.compare(L".png") == 0 || extension.compare(L".PNG") == 0) {
            DXCall(DirectX::LoadFromWICFile(infile_path.c_str(), DirectX::WIC_FLAGS_NO_16BPP, nullptr, image));
        }
        else if (extension.compare(L".jpg") == 0 || extension.compare(L".JPG") == 0) {
            DXCall(DirectX::LoadFromWICFile(infile_path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image));
        }
        else {
            throw std::runtime_error("Wasn't able to load file!");
        }

        DirectX::TexMetadata meta_data = image.GetMetadata();
        size_t min_length = min(meta_data.width, meta_data.height);
        u32 levels = u32(log2(min_length));
        DirectX::ScratchImage mip_chain;
        DXCall(DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, levels, mip_chain));
        meta_data = mip_chain.GetMetadata();
        DXCall(DirectX::SaveToDDSFile(mip_chain.GetImages(), mip_chain.GetImageCount(), meta_data, DirectX::DDS_FLAGS_NONE, output_path.wstring().c_str()));

        mip_chain.Release();
        image.Release();
    }

    void process_textures(const tinygltf::Model& model, const char* gltf_file_path, LinearAllocator& texture_path_allocator, const std::filesystem::path& output_folder, assets::MeshAsset& out_mesh_asset)
    {
        const std::filesystem::path file_path{ gltf_file_path };
        const std::filesystem::path folder_path = file_path.parent_path();

        const size_t num_unique_textures = model.images.size();

        for (size_t i = 0; i < num_unique_textures; i++)
        {
            const auto& image = model.images[i];
            std::filesystem::path local_path{ image.uri };
            std::filesystem::path image_path = folder_path / local_path;
            std::filesystem::path output_path = output_folder / local_path;
            output_path.replace_extension("DDS");
            preprocess_texture(image_path, output_path);

            std::wstring output_path_str = output_path.wstring();
            size_t length = output_path_str.length() + 1;
            wchar_t* path_ptr = static_cast<wchar_t*>(texture_path_allocator.allocate(length * sizeof(wchar_t), alignof(wchar_t)));
            memory::copy(path_ptr, output_path_str.c_str(), length * sizeof(wchar_t));

            out_mesh_asset.textures[i] = assets::TextureDesc{
                .path = {
                    .size = u32(length),
                    .offset = u32(path_ptr - static_cast<wchar_t*>(texture_path_allocator.get_ptr()))
                },
            };
        }

        out_mesh_asset.texture_paths = { .byte_size = texture_path_allocator.get_bytes_allocated(), .ptr = texture_path_allocator.get_ptr() };
    }

    template<typename Desc>
    TypedArrayView<Desc> allocate_array(IAllocator& allocator, size_t num_elements)
    {
        return { allocator.allocate(num_elements * sizeof(Desc)), num_elements };
    }

    template<typename ComponentData, assets::AssetComponentType component_type>
    void fill_component_desc_size_offset(size_t& running_byte_offset, size_t(&byte_sizes)[size_t(assets::AssetComponentType::Count)], size_t(&byte_offsets)[size_t(assets::AssetComponentType::Count)], size_t num_elements)
    {
        byte_offsets[u64(component_type)] = running_byte_offset;
        running_byte_offset += num_elements * sizeof(ComponentData);
        byte_sizes[u64(component_type)] = running_byte_offset + byte_offsets[u64(component_type)];
    }

    void allocate_component_arrays(const tinygltf::Model& model, IAllocator& allocator, assets::MeshAsset& out_mesh)
    {
        const size_t num_meshes = model.meshes.size();
        size_t num_primitives = 0;
        for (size_t mesh_idx = 0; mesh_idx < num_meshes; ++mesh_idx)
        {
            const auto& mesh = model.meshes[mesh_idx];
            num_primitives += mesh.primitives.size();
        }

        size_t render_node_count = 0;
        for (size_t node_idx = 0; node_idx < model.meshes.size(); ++node_idx)
        {
            if (model.nodes[node_idx].mesh >= 0)
            {
                ++render_node_count;
            }
        }

        u64 running_byte_offset = 0;
        size_t byte_sizes[u64(assets::AssetComponentType::Count)] = {};
        size_t byte_offsets[u64(assets::AssetComponentType::Count)] = {};

        fill_component_desc_size_offset<assets::RenderNode, assets::AssetComponentType::RenderNode>(running_byte_offset, byte_sizes, byte_offsets, render_node_count);
        fill_component_desc_size_offset<assets::TransformNode, assets::AssetComponentType::TransformNode>(running_byte_offset, byte_sizes, byte_offsets, model.nodes.size());
        fill_component_desc_size_offset<assets::MeshDesc, assets::AssetComponentType::Mesh>(running_byte_offset, byte_sizes, byte_offsets, num_meshes);
        fill_component_desc_size_offset<AABB, assets::AssetComponentType::AABB>(running_byte_offset, byte_sizes, byte_offsets, num_primitives);
        fill_component_desc_size_offset<assets::SubmeshDesc, assets::AssetComponentType::Submesh>(running_byte_offset, byte_sizes, byte_offsets, num_primitives);
        fill_component_desc_size_offset<assets::MaterialDesc, assets::AssetComponentType::Material>(running_byte_offset, byte_sizes, byte_offsets, model.materials.size());
        fill_component_desc_size_offset<assets::TextureDesc, assets::AssetComponentType::Texture>(running_byte_offset, byte_sizes, byte_offsets, model.images.size());

        out_mesh.components_blob = { running_byte_offset, allocator.allocate(running_byte_offset) };
        out_mesh.render_nodes = {
            render_node_count,
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::RenderNode)]
        };
        out_mesh.transform_nodes = {
            model.nodes.size(),
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::TransformNode)]
        };
        out_mesh.meshes = {
            num_meshes,
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::Mesh)]
        };
        out_mesh.aabbs = {
            num_primitives,
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::AABB)]
        };
        out_mesh.submeshes = {
            num_primitives,
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::Submesh)]
        };
        out_mesh.materials = {
            model.materials.size(),
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::Material)]
        };
        out_mesh.textures = {
            model.images.size(),
            reinterpret_cast<u8*>(out_mesh.components_blob.ptr) + byte_offsets[size_t(assets::AssetComponentType::Texture)]
        };

        size_t index_buffer_byte_size = 0;
        size_t vertex_buffers_byte_size[size_t(rhi::MeshAttribute::COUNT)] = { 0 };
        for (size_t mesh_idx = 0; mesh_idx < model.meshes.size(); ++mesh_idx)
        {
            const tinygltf::Mesh& gltf_mesh = model.meshes[mesh_idx];
            for (const auto& primitive : gltf_mesh.primitives)
            {
                // INDEX BUFFER
                {
                    const auto& accessor = model.accessors[primitive.indices];
                    index_buffer_byte_size += accessor.count * sizeof(u32);
                }

                // VERTEX DATA

                static_assert(ARRAY_SIZE(assets::attr_meta_data) == size_t(rhi::MeshAttribute::COUNT));
                for (size_t i = 0; i < ARRAY_SIZE(assets::attr_meta_data); ++i) {
                    if (primitive.attributes.contains(assets::attr_meta_data[i].name))
                    {
                        const auto& attr_idx = primitive.attributes.at(assets::attr_meta_data[i].name);
                        const auto& accessor = model.accessors[attr_idx];
                        const auto& buffer_view = model.bufferViews[accessor.bufferView];

                        ASSERT_MSG(buffer_view.byteStride == 0 || buffer_view.byteStride == assets::attr_meta_data[i].stride, "We only allow tightly packed vertex attributes (sorry)");
                        vertex_buffers_byte_size[i] += accessor.count * assets::attr_meta_data[i].stride;
                    }
                }
            }
        }

        out_mesh.index_buffers = { index_buffer_byte_size, allocator.allocate(index_buffer_byte_size) };

        for (size_t i = 0; i < size_t(rhi::MeshAttribute::COUNT); ++i)
        {
            if (vertex_buffers_byte_size[i] > 0)
            {
                out_mesh.vertex_buffers[i] = { vertex_buffers_byte_size[i], allocator.allocate(vertex_buffers_byte_size[i]) };
            }
        }
    }

    void process_primitives(const tinygltf::Model& model, assets::MeshAsset& out_mesh, VirtualArray<ChildParentPair>& node_processing_list)
    {
        u32 render_node_offset = 0;
        u32 vertex_attribute_idx = 0;
        u32 renderable_idx = 0;
        u32 index_buffer_offset = 0;
        u32 vertex_buffers_offsets[size_t(rhi::MeshAttribute::COUNT)] = { 0 };
        for (size_t node_idx = 0; node_idx < node_processing_list.size; ++node_idx)
        {
            ChildParentPair pair = node_processing_list[node_idx];
            const tinygltf::Node& gltf_node = model.nodes[pair.child_idx];
            if (gltf_node.mesh < 0)
            {
                continue;
            }
            const size_t mesh_idx = static_cast<size_t>(gltf_node.mesh);
            const tinygltf::Mesh& gltf_mesh = model.meshes[mesh_idx];
            const u32 mesh_renderable_start = renderable_idx;
            for (const auto& primitive : gltf_mesh.primitives) {
                assets::SubmeshDesc submesh_desc{};
                // Submesh Indices
                {
                    const auto& accessor = model.accessors[primitive.indices];
                    const auto& buffer_view = model.bufferViews[accessor.bufferView];
                    const auto& buffer = model.buffers[buffer_view.buffer];
                    size_t offset = accessor.byteOffset + buffer_view.byteOffset;
                    u32 in_byte_stride = 0;

                    switch (accessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        ASSERT(buffer_view.byteStride == 0 || buffer_view.byteStride == 1);
                        in_byte_stride = 1;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        ASSERT(buffer_view.byteStride == 0 || buffer_view.byteStride == 2);
                        in_byte_stride = 2;
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        ASSERT(buffer_view.byteStride == 0 || buffer_view.byteStride == 4);
                        in_byte_stride = 4;
                        break;
                    default:
                        ASSERT_FAIL("Cannot handle this index type!");
                        break;
                    }

                    u32 byte_stride = sizeof(u32);
                    size_t byte_length = accessor.count * byte_stride;
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        u32* index_buffer = reinterpret_cast<u32*>(static_cast<u8*>(out_mesh.index_buffers.ptr) + (index_buffer_offset * byte_stride));
                        for (size_t i = 0; i < accessor.count; i++)
                        {
                            const u8* pindex = &buffer.data.at(offset + i * in_byte_stride);

                            index_buffer[i] = static_cast<u32>(accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
                                ? *reinterpret_cast<u16 const*>(pindex)
                                : *pindex);
                        }
                    }
                    else
                    {
                        memory::copy((u8*)(out_mesh.index_buffers.ptr) + (index_buffer_offset * byte_stride), &buffer.data.at(offset), byte_length);
                    }

                    submesh_desc.index_buffer_view = { .offset = index_buffer_offset, .length = u32(accessor.count), .stride = byte_stride };
                    index_buffer_offset += accessor.count;
                }

                const auto& attributes = primitive.attributes;

                // Store our AABB
                {
                    const int attr_idx = attributes.at("POSITION");
                    const auto& accessor = model.accessors[attr_idx];
                    vec3 aabb_min = {
                        float(accessor.minValues[0]),
                        float(accessor.minValues[1]),
                        float(accessor.minValues[2])
                    };
                    vec3 aabb_max = {
                        float(accessor.maxValues[0]),
                        float(accessor.maxValues[1]),
                        float(accessor.maxValues[2])
                    };

                    out_mesh.aabbs[renderable_idx] = {
                        .min = aabb_min,
                        .max = aabb_max,
                    };
                }

                for (size_t i = 0; i < ARRAY_SIZE(assets::attr_meta_data); ++i) {
                    if (attributes.contains(assets::attr_meta_data[i].name))
                    {
                        const auto& attr_idx = attributes.at(assets::attr_meta_data[i].name);
                        const auto& accessor = model.accessors[attr_idx];
                        const auto& buffer_view = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[buffer_view.buffer];
                        size_t offset = accessor.byteOffset + buffer_view.byteOffset;

                        size_t byte_stride = assets::attr_meta_data[i].stride;
                        size_t byte_length = accessor.count * byte_stride;
                        memory::copy((u8*)(out_mesh.vertex_buffers[i].ptr) + (vertex_buffers_offsets[i] * byte_stride), &buffer.data.at(offset), byte_length);

                        submesh_desc.vertex_attributes.create_back(assets::BufferView{
                            .offset = vertex_buffers_offsets[i],
                            .length = u32(accessor.count),
                            .stride = u32(byte_stride)
                        }, assets::attr_meta_data[i].type);

                        vertex_buffers_offsets[i] += u32(accessor.count);
                    }
                }

                submesh_desc.material_id = { u32(primitive.material) };

                out_mesh.submeshes[renderable_idx++] = submesh_desc;
            }

            out_mesh.meshes[mesh_idx] = assets::MeshDesc{
                .submeshes = ArrayView{ renderable_idx - mesh_renderable_start, mesh_renderable_start },
                .AABBs = ArrayView{ renderable_idx - mesh_renderable_start, mesh_renderable_start },
            };
            out_mesh.render_nodes[render_node_offset++] = { .transform_id = { u32(node_idx) }, .mesh_id = { u32(mesh_idx)} };
        }

    }

    void process_materials(const tinygltf::Model& model, assets::MeshAsset& out_mesh)
    {
        for (size_t i = 0; i < model.materials.size(); i++)
        {
            const tinygltf::Material& material = model.materials[i];
            assets::MaterialDesc material_desc{};
            // ALBEDO
            if (material.values.contains("baseColorFactor"))
            {
                const tinygltf::ColorValue& color_factor = material.values.at("baseColorFactor").ColorFactor();
                material_desc.albedo = { float(color_factor[0]), float(color_factor[1]), float(color_factor[2]), float(color_factor[3]) };
            }
            // EMISSIVE
            if (material.additionalValues.contains("emissiveFactor"))
            {
                const tinygltf::ColorValue& emissive = material.additionalValues.at("emissiveFactor").ColorFactor();
                material_desc.emissive = { float(emissive[0]), float(emissive[1]), float(emissive[2]) };
            }
            // METALNESS
            if (material.values.contains("metallicFactor"))
            {
                material_desc.metalness = float(material.values.at("metallicFactor").Factor());
            }
            // ROUGHNESS
            if (material.values.contains("roughnessFactor"))
            {
                material_desc.roughness = float(material.values.at("roughnessFactor").Factor());
            }

            // ALBEDO TEXTURE
            if (material.values.contains("baseColorTexture"))
            {
                const int texture_index = material.values.at("baseColorTexture").TextureIndex();
                if (texture_index >= 0)
                {
                    const tinygltf::Texture& texture = model.textures[texture_index];
                    material_desc.albedo_texture = {
                        .texture={ u32(texture.source) },
                        .sampling_mode=0 // TODO
                    };
                }
            }

            // NORMAL TEXTURE
            if (material.additionalValues.contains("normalTexture"))
            {
                const int texture_index = material.additionalValues.at("normalTexture").TextureIndex();
                if (texture_index >= 0)
                {
                    const tinygltf::Texture& texture = model.textures[texture_index];
                    material_desc.normal_texture = {
                        .texture = { u32(texture.source) },
                        .sampling_mode = 0 // TODO
                    };
                }
            }

            if (material.values.contains("metallicRoughnessTexture"))
            {
                const int texture_index = material.values.at("metallicRoughnessTexture").TextureIndex();
                if (texture_index >= 0)
                {
                    const tinygltf::Texture& texture = model.textures[texture_index];
                    material_desc.metal_roughness_ao_texture = {
                        .texture = { u32(texture.source) },
                        .sampling_mode = 0 // TODO
                    };
                }

                if (material.additionalValues.contains("occlusionTexture"))
                {
                    const int ao_texture_index = material.additionalValues.at("occlusionTexture").TextureIndex();
                    ASSERT_MSG(texture_index == ao_texture_index, "Not sure yet how to handle an occlusion texture that's not the same as the metallic roughness texture");
                }
            }

            if (material.additionalValues.contains("emissiveTexture"))
            {
                const int texture_index = material.additionalValues.at("emissiveTexture").TextureIndex();
                if (texture_index >= 0)
                {
                    const tinygltf::Texture& texture = model.textures[texture_index];
                    material_desc.emissive_texture = {
                        .texture = { u32(texture.source) },
                        .sampling_mode = 0 // TODO
                    };
                }
            }

            // ALPHA MODE
            if (material.additionalValues.contains("alphaMode"))
            {
                const std::string& alphaMode = material.additionalValues.at("alphaMode").string_value;
                if (alphaMode.compare("OPAQUE") == 0)
                {
                    material_desc.blend_mode = assets::MaterialDesc::BlendMode::Opaque;
                }
                else if (alphaMode.compare("MASK") == 0)
                {
                    material_desc.blend_mode = assets::MaterialDesc::BlendMode::Cutoff;
                    // ALPHA CUTOFF
                    if (material.additionalValues.contains("alphaCutoff"))
                    {
                        material_desc.alpha_cutoff = float(material.additionalValues.at("alphaCutoff").Factor());
                    }
                    else
                    {
                        material_desc.alpha_cutoff = 0.5f;
                    }
                }
                else if (alphaMode.compare("BLEND") == 0)
                {
                    material_desc.blend_mode = assets::MaterialDesc::BlendMode::AlphaBlend;
                }
                else
                {
                    ASSERT_FAIL("We've encountered an alpha mode that's not supported by the GLTF spec, default to OPAQUE");
                }
            }

            out_mesh.materials[i] = material_desc;
        }
    }

    constexpr u32 get_attr_index_by_type(const rhi::MeshAttribute mesh_attribute)
    {
        ASSERT(mesh_attribute < rhi::MeshAttribute::COUNT);
        return u32(mesh_attribute);
    };

    void convert_from_gltf(const char* gltf_file_path, IAllocator& allocator, LinearAllocator& texture_path_allocator, const std::filesystem::path& output_path, assets::MeshAsset& out_mesh_asset, const LoaderFlags flags = GLTF_LOADING_FLAG_NONE)
    {
        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(noop_image_loader_callback, nullptr);

        std::string err, warn;
        tinygltf::Model model;

        bool res = false;
        if (flags & GLTF_LOADING_BINARY_FORMAT)
        {
            res = loader.LoadBinaryFromFile(&model, &err, &warn, gltf_file_path);
        }
        else
        {
            res = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_file_path);
        }

        if (!warn.empty())
        {
            write_log(warn.c_str());
        }
        if (!err.empty())
        {
            write_log(err.c_str());
        }

        if (!res) {
            throw std::runtime_error("Failed to load GLTF Model");
        }

        if (model.scenes.size() > 1) {
            throw std::runtime_error("Can't handle gltf files with more than one scene atm");
        }

        allocate_component_arrays(model, allocator, out_mesh_asset);

        // Flatten gltf node graph
        VirtualArray<ChildParentPair> node_processing_list{};
        flatten_gltf_scene_graph(model, node_processing_list);

        // Copy Node List
        process_scene_graph(model, node_processing_list, allocator, out_mesh_asset);

        // todo: Mark Joint Nodes?

        // Process Textures
        process_textures(model, gltf_file_path, texture_path_allocator, output_path.parent_path(), out_mesh_asset);

        // Process Materials
        process_materials(model, out_mesh_asset);

        // Process Primitives
        VirtualArray<MeshArrayView> mesh_to_mesh_mapping = {};
        process_primitives(model, out_mesh_asset, node_processing_list);
    }
}

int main(int argc, char** argv)
{
    if (argc <= 2)
    {
        printf("Please provide a path for the asset to convert, as well as the output asset path");
        return 1;
    }
    DXCall(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    size_t k2GB = 2u * (1024u * 1024u * 1024u);
    zec::LinearAllocator allocator{ };
    zec::LinearAllocator texture_path_allocator{ };
    allocator.init(k2GB);
    texture_path_allocator.init(1024 * 1024); // 1MB
    std::filesystem::path asset_path = argv[1];
    if (asset_path.extension().compare(".gltf") == 0)
    {
        zec::assets::MeshAsset mesh_asset{};

        std::filesystem::path output_path{ argv[2] };
        zec::asset_converter::convert_from_gltf(argv[1], allocator, texture_path_allocator, output_path, mesh_asset);
        zec::assets::save_binary_file(argv[2], mesh_asset);
        wprintf(L"Model created at %s", output_path.c_str());
    }
    else
    {
        printf("Asset path invalid.");
    }
    texture_path_allocator.shutdown();
    allocator.shutdown();
    return 0;
}