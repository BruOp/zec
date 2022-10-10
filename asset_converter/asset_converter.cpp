#include "asset_converter.h"

#include <filesystem>
#include <utils/utils.h>
#include <asset_lib.h>
#include <tinygltf/tiny_gltf.h>
#include <DirectXTex.h>
#include "gfx/d3d12/dx_helpers.h"
#include "gfx/d3d12/dx_utils.h"
#include "gfx/gfx.h"


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

    struct ConverterModelAsset
    {
        std::vector<assets::AssetComponentDesc> asset_component_descs = { };
        std::vector<assets::BufferView> buffer_views = {};
        std::vector<assets::MeshAttributeComponent> mesh_attribute_components = { };
        std::vector<assets::TextureDesc> textures = { };
        std::vector<assets::TransformNode> transform_nodes = {};
        std::vector<assets::RenderableDesc> meshes = {};
        std::vector<assets::MaterialDesc> materials = {};

        std::vector<Allocation> binary_blobs;
    };
    
    void traverse_scene_graph(const tinygltf::Model& model, const u32 parent_idx, const u32 node_idx, Array<ChildParentPair>& node_processing_list)
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
    void flatten_gltf_scene_graph(const tinygltf::Model& model, Array<ChildParentPair>& node_processing_list)
    {
        u32 scene_idx = model.defaultScene >= 0 ? u32(model.defaultScene) : 0;
        ASSERT(model.scenes.size() > 0);
        const tinygltf::Scene& scene = model.scenes[scene_idx];
        Array<ChildParentPair> node_queue = {};
        node_processing_list.reserve(model.nodes.size());

        // Push root of scene tree into queue
        for (i32 root_idx : scene.nodes) {
            traverse_scene_graph(model, UINT32_MAX, root_idx, node_processing_list);
        }
    }


    void process_scene_graph(const tinygltf::Model& model, const Array<ChildParentPair>& node_processing_list, IAllocator& allocator, ConverterModelAsset& out_model_asset)
    {
        // Construct scene graph using node_processing_list
        const u64 num_nodes = node_processing_list.size;

        // TODO -- Does this make sense decoupled? Seems like we might need like a ModelAssetFactory to manage the allocator
        out_model_asset.transform_nodes.resize(num_nodes);
        
        for (size_t node_idx = 0; node_idx < node_processing_list.size; node_idx++) {
            ChildParentPair pair = node_processing_list[node_idx];

            const tinygltf::Node& node = model.nodes[pair.child_idx];
            assets::TransformNode& transform_node = out_model_asset.transform_nodes[node_idx];
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
                mat4 rotation_matrix = quat_to_mat4(transform_node.rotation);
            }


            // Translation
            if (node.translation.size() > 0) {
                for (size_t i = 0; i < node.translation.size(); i++) {
                    transform_node.translation[i] = float(node.translation[i]);
                }
            }
        }
    }

    void create_from_file(const char* file_path, IAllocator& allocator, assets::TextureDesc& texture_desc, Allocation& out_binary_blob)
    {
        std::wstring path = ansi_to_wstring(file_path);
        const std::filesystem::path filesystem_path{ file_path };

        DirectX::ScratchImage image;
        const auto& extension = filesystem_path.extension();

        if (extension.compare(L".dds") == 0 || extension.compare(L".DDS") == 0) {
            DXCall(DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
        }
        else if (extension.compare(L".hdr") == 0 || extension.compare(L".HDR") == 0) {
            DXCall(DirectX::LoadFromHDRFile(path.c_str(), nullptr, image));
        }
        else if (extension.compare(L".png") == 0 || extension.compare(L".PNG") == 0) {
            DXCall(DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image));
        }
        else {
            throw std::runtime_error("Wasn't able to load file!");
        }

        const DirectX::TexMetadata meta_data = image.GetMetadata();
        out_binary_blob = allocator.alloc(image.GetPixelsSize());
        memcpy(out_binary_blob.ptr, image.GetPixels(), image.GetPixelsSize());

        texture_desc = assets::TextureDesc {
            .width = u32(meta_data.width),
            .height = u32(meta_data.height),
            .depth = u32(meta_data.depth),
            .num_mips = u32(meta_data.mipLevels),
            .array_size = u32(meta_data.arraySize),
            .is_cubemap = u16(meta_data.IsCubemap()),
            .format = gfx::dx12::from_d3d_format(meta_data.format),
            .usage = RESOURCE_USAGE_SHADER_READABLE,
        };
        
        image.Release();   
    }

    void process_textures(const tinygltf::Model& model, const char* gltf_file_path, IAllocator& allocator, ConverterModelAsset& out_model_asset)
    {
        const std::filesystem::path file_path{ gltf_file_path };
        const std::filesystem::path folder_path = file_path.parent_path();

        const size_t num_textures = model.images.size();
        out_model_asset.textures.reserve(num_textures);

        for (const auto& image : model.images) {
            std::filesystem::path image_path = folder_path / std::filesystem::path{ image.uri };

            assets::TextureDesc texture_desc{};
            Allocation allocation{};
            create_from_file(image_path.string().c_str(), allocator, texture_desc, allocation);
            out_model_asset.textures.push_back(texture_desc);
            out_model_asset.binary_blobs.push_back(allocation);

            // TODO: Process Samplers?
        }
    }

    void process_primitives(const tinygltf::Model& model, const LoaderFlags flags, Array<MeshArrayView>& mesh_to_mesh_mapping, ConverterModelAsset& out_context)
    {

        for (size_t their_mesh_idx = 0; their_mesh_idx < model.meshes.size(); ++their_mesh_idx) {
            const auto& mesh = model.meshes[their_mesh_idx];
            // New entry for each gltf mesh
            mesh_to_mesh_mapping.push_back({ .offset = u32(out_context.meshes.size()), .count = u32(mesh.primitives.size()) });
            for (const auto& primitive : mesh.primitives) {
                assets::RenderableDesc mesh_desc{};
                AABB aabb = {};
                // Indices
                {
                    const auto& accessor = model.accessors[primitive.indices];
                    const auto& buffer_view = model.bufferViews[accessor.bufferView];
                    const auto& buffer = model.buffers[buffer_view.buffer];
                    size_t offset = accessor.byteOffset + buffer_view.byteOffset;

                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        throw std::runtime_error("TODO: Allow byte sized indices");
                    }

                    assets::BufferView index_buffer{
                        
                    };
                    mesh_desc.index_buffer_desc = {
                        .usage = RESOURCE_USAGE_INDEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = u32(buffer_view.byteLength),
                        .stride = u32(accessor.ByteStride(buffer_view)),
                    };
                    mesh_desc.index_buffer_data = (void*)(&buffer.data.at(offset));
                }

                const auto& attributes = primitive.attributes;
                constexpr char const* attr_names[] = {
                    "POSITION",
                    "NORMAL",
                    "TEXCOORD_0"
                };

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

                    out_context.aabbs.push_back({
                        .min = aabb_min,
                        .max = aabb_max,
                        });

                }

                for (size_t i = 0; i < std::size(attr_names); ++i) {
                    const auto& attr_idx = attributes.at(attr_names[i]);
                    const auto& accessor = model.accessors[attr_idx];
                    const auto& buffer_view = model.bufferViews[accessor.bufferView];
                    const auto& buffer = model.buffers[buffer_view.buffer];
                    size_t offset = accessor.byteOffset + buffer_view.byteOffset;

                    ASSERT_MSG(buffer_view.byteStride == 0, "We only allow tightly packed vertex attributes (sorry)");

                    mesh_desc.vertex_buffer_descs[i] = {
                        .usage = RESOURCE_USAGE_VERTEX,
                        .type = BufferType::DEFAULT,
                        .byte_size = u32(buffer_view.byteLength),
                        .stride = u32(accessor.ByteStride(buffer_view)),
                    };
                    mesh_desc.vertex_buffer_data[i] = (void*)(&buffer.data.at(offset));
                }

                // Create mesh
                out_context.meshes.push_back(gfx::meshes::create(cmd_ctx, mesh_desc));
            }
        }
    }

    void convert_from_gltf(const char* gltf_file_path, const LoaderFlags flags, IAllocator& allocator, assets::ModelAsset& out_model_asset)
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

        // Flatten gltf node graph
        Array<ChildParentPair> node_processing_list{};
        flatten_gltf_scene_graph(model, node_processing_list);

        // Used as an intermediate representation
        ConverterModelAsset converter_model_asset = {};
        // Copy Node List
        process_scene_graph(model, node_processing_list, allocator, converter_model_asset);

        // todo: Mark Joint Nodes?

        // Process Textures
        process_textures(model, gltf_file_path, allocator, converter_model_asset);

        // Process Primitives
        Array<MeshArrayView> mesh_to_mesh_mapping = {};
        process_primitives(model, mesh_to_mesh_mapping, allocator, converter_model_asset, flags);

        // Process Materials
        process_materials(model, node_processing_list, out_context);

        // Process Draw calls
        process_draw_calls(model, node_processing_list, mesh_to_mesh_mapping, out_context);

    }
}
