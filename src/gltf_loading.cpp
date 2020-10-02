#include "pch.h"
#include "gltf_loading.h"
#include <filesystem>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf/tiny_gltf.h"

namespace zec
{
    namespace gltf
    {
        bool loadImageDataCallback(
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
            const tinygltf::Scene& scene = model.scenes[model.defaultScene];
            Array<ChildParentPair> node_queue = {};
            node_processing_list.reserve(model.nodes.size());

            // Push root of scene tree into queue
            for (i32 root_idx : scene.nodes) {
                traverse_scene_graph(model, UINT32_MAX, root_idx, node_processing_list);
            }
        }

        void process_scene_graph(const tinygltf::Model& model, const Array<ChildParentPair>& node_processing_list, Context& out_context)
        {
            const u64 num_nodes = model.nodes.size();

            // Construct scene graph using node_processing_list
            SceneGraph& scene_graph = out_context.scene_graph;
            scene_graph.parent_ids.grow(node_processing_list.size);
            scene_graph.positions.grow(node_processing_list.size);
            scene_graph.rotations.grow(node_processing_list.size);
            scene_graph.scales.grow(node_processing_list.size);
            scene_graph.global_transforms.grow(node_processing_list.size);

            for (size_t node_idx = 0; node_idx < node_processing_list.size; node_idx++) {
                ChildParentPair pair = node_processing_list[node_idx];

                const tinygltf::Node& node = model.nodes[pair.child_idx];
                mat4& global_transform = scene_graph.global_transforms[node_idx];
                global_transform = identity_mat4();
                scene_graph.parent_ids[node_idx] = pair.parent_idx;
                // Scale
                if (node.scale.size() > 0) {
                    for (size_t i = 0; i < node.scale.size(); i++) {
                        scene_graph.scales[node_idx][i] = float(node.scale[i]);
                    }
                    set_scale(global_transform, scene_graph.scales[node_idx]);
                }
                else {
                    scene_graph.scales[node_idx] = vec3{ 1.0f, 1.0f, 1.0f };
                }

                // Rotation
                if (node.rotation.size() > 0) {
                    for (size_t i = 0; i < node.rotation.size(); i++) {
                        scene_graph.rotations[node_idx][i] = float(node.rotation[i]);
                    }
                    rotate(global_transform, scene_graph.rotations[node_idx]);
                }

                // Translation
                if (node.translation.size() > 0) {
                    for (size_t i = 0; i < node.translation.size(); i++) {
                        scene_graph.positions[node_idx][i] = float(node.translation[i]);
                    }
                    set_translation(global_transform, scene_graph.positions[node_idx]);
                }

                const u32 parent_idx = scene_graph.parent_ids[node_idx];
                if (parent_idx != UINT32_MAX) {
                    const auto& parent_transform = scene_graph.global_transforms[parent_idx];
                    global_transform = parent_transform * global_transform;
                }
            }
        }


        void process_textures(const tinygltf::Model& model, const std::string& gltf_file, Context& out_context)
        {
            const std::filesystem::path file_path{ gltf_file };
            const std::filesystem::path folder_path = file_path.parent_path();

            const size_t num_textures = model.textures.size();
            out_context.textures.reserve(num_textures);

            for (const auto& texture : model.textures) {
                const auto& image = model.images[texture.source];
                std::filesystem::path image_path = folder_path / std::filesystem::path{ image.uri };
                TextureHandle& texture = load_texture_from_file(image_path.string().c_str());
                out_context.textures.push_back(texture);

                // TODO: Process Samplers?
            }
        }

        void process_primitives(const tinygltf::Model& model, Array<MeshArrayView>& mesh_to_mesh_mapping, Context& out_context)
        {
            for (size_t their_mesh_idx = 0; their_mesh_idx < model.meshes.size(); ++their_mesh_idx) {
                const auto& mesh = model.meshes[their_mesh_idx];
                // New entry for each gltf mesh
                mesh_to_mesh_mapping.push_back({ .offset = u32(out_context.meshes.size), .count = u32(mesh.primitives.size()) });
                for (const auto& primitive : mesh.primitives) {
                    MeshDesc mesh_desc{};

                    // Indices
                    {
                        const auto& accessor = model.accessors[primitive.indices];
                        const auto& buffer_view = model.bufferViews[accessor.bufferView];
                        const auto& buffer = model.buffers[buffer_view.buffer];
                        size_t offset = accessor.byteOffset + buffer_view.byteOffset;

                        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            throw std::runtime_error("TODO: Allow byte sized indices");
                        }

                        mesh_desc.index_buffer_desc = {
                            .usage = RESOURCE_USAGE_INDEX,
                            .type = BufferType::DEFAULT,
                            .byte_size = u32(buffer_view.byteLength),
                            .stride = u32(accessor.ByteStride(buffer_view)),
                            .data = (void*)(&buffer.data.at(offset)),
                        };
                    }

                    const auto& attributes = primitive.attributes;
                    constexpr char* attr_names[] = {
                        "POSITION",
                        "NORMAL",
                        "TEXCOORD_0"
                    };

                    for (size_t i = 0; i < ARRAY_SIZE(attr_names); ++i) {
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
                            .data = (void*)(&buffer.data.at(offset)),
                        };
                    }

                    // Create mesh
                    out_context.meshes.push_back(create_mesh(mesh_desc));
                }
            }
        }

        void process_materials(
            const tinygltf::Model& model,
            const Array<ChildParentPair>& node_processing_list,
            Context& out_context
        )
        {
            for (const auto& material : model.materials) {
                MaterialData material_data{ };

                const auto values_end = material.values.end();
                const auto additional_values_end = material.additionalValues.end();

                // Textures
                {
                    const auto& color_texture = material.values.find("baseColorTexture");
                    if (color_texture != values_end) {
                        const TextureHandle handle = out_context.textures[color_texture->second.TextureIndex()];
                        material_data.base_color_texture_idx = get_shader_readable_texture_index(handle);
                    }

                    const auto& m_r_texture = material.values.find("metallicRoughnessTexture");
                    if (m_r_texture != values_end) {
                        const TextureHandle handle = out_context.textures[m_r_texture->second.TextureIndex()];
                        material_data.metallic_roughness_texture_idx = get_shader_readable_texture_index(handle);
                    }

                    const auto& normal_texture = material.additionalValues.find("normalTexture");
                    if (normal_texture != additional_values_end) {
                        const TextureHandle handle = out_context.textures[normal_texture->second.TextureIndex()];
                        material_data.normal_texture_idx = get_shader_readable_texture_index(handle);
                    }

                    const auto& occlusion_texture = material.additionalValues.find("occlusionTexture");
                    if (occlusion_texture != additional_values_end) {
                        const TextureHandle handle = out_context.textures[occlusion_texture->second.TextureIndex()];
                        material_data.occlusion_texture_idx = get_shader_readable_texture_index(handle);
                    }

                    const auto& emissive_texture = material.additionalValues.find("emissiveTexture");
                    if (emissive_texture != additional_values_end) {
                        const TextureHandle handle = out_context.textures[emissive_texture->second.TextureIndex()];
                        material_data.emissive_texture_idx = get_shader_readable_texture_index(handle);
                    }
                }

                // Factors
                {
                    const auto& color_factor = material.values.find("baseColorFactor");
                    if (color_factor != values_end) {
                        const auto& factor = color_factor->second.ColorFactor();
                        for (size_t i = 0; i < factor.size(); i++) {
                            material_data.base_color_factor[i] = float(factor[i]);
                        }
                    }

                    const auto& emissive_factor = material.additionalValues.find("emissiveFactor");
                    if (emissive_factor != additional_values_end) {
                        const auto& factor = emissive_factor->second.ColorFactor();
                        for (size_t i = 0; i < 3u; i++) { // Emissive values cannot have alpha
                            material_data.emissive_factor[i] = float(factor[i]);
                        }
                    }

                    const auto& metallic_factor = material.values.find("metallicFactor");
                    if (metallic_factor != values_end) {
                        material_data.metallic_factor = float(metallic_factor->second.Factor());
                    }

                    const auto& roughness_factor = material.values.find("roughnessFactor");
                    if (roughness_factor != values_end) {
                        material_data.roughness_factor = float(roughness_factor->second.Factor());
                    }
                }

                out_context.materials.push_back(material_data);
            }
        }

        void process_draw_calls(
            const tinygltf::Model& model,
            const Array<ChildParentPair>& node_processing_list,
            const Array<MeshArrayView>& mesh_to_mesh_mapping,
            Context& out_context
        )
        {
            for (size_t our_node_idx = 0; our_node_idx < node_processing_list.size; ++our_node_idx) {
                const auto& pair = node_processing_list[our_node_idx];

                const auto& node = model.nodes[pair.child_idx];
                if (node.mesh >= 0) {
                    const auto& gltf_mesh = model.meshes[node.mesh];
                    const auto& mesh_info = mesh_to_mesh_mapping[node.mesh];

                    for (size_t primitive_offset = 0; primitive_offset < mesh_info.count; ++primitive_offset) {
                        const auto& gltf_primitive = gltf_mesh.primitives[primitive_offset];

                        out_context.draw_calls.push_back(DrawCall{
                            .mesh = out_context.meshes[mesh_info.offset + primitive_offset],
                            .scene_node_idx = u32(our_node_idx),
                            .material_index = u32(gltf_primitive.material),
                            });
                    }
                }
            }
        }

        void load_gltf_file(const std::string& gltf_file, Context& out_context)
        {
            tinygltf::TinyGLTF loader;
            loader.SetImageLoader(loadImageDataCallback, nullptr);

            std::string err, warn;
            tinygltf::Model model;

            bool res = loader.LoadASCIIFromFile(&model, &err, &warn, gltf_file);

            if (!warn.empty()) {
                write_log(warn.c_str());
            }
            if (!err.empty()) {
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

            // Copy Node List
            process_scene_graph(model, node_processing_list, out_context);

            // todo: Mark Joint Nodes?

            // Process Textures
            process_textures(model, gltf_file, out_context);

            // Process Primitives
            Array<MeshArrayView> mesh_to_mesh_mapping = {};
            process_primitives(model, mesh_to_mesh_mapping, out_context);

            // Process Materials
            process_materials(model, node_processing_list, out_context);

            // Process Draw calls
            process_draw_calls(model, node_processing_list, mesh_to_mesh_mapping, out_context);
        };
    }
}
