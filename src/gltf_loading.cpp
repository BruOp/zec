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
            for (i32 parent_idx : scene.nodes) {
                node_processing_list.push_back({ u32(parent_idx), UINT32_MAX });

                const tinygltf::Node& node = model.nodes[parent_idx];
                for (i32 child_idx : node.children) {
                    node_queue.push_back({ u32(child_idx), u32(parent_idx) });
                }
            }

            // Use queue to do a depth-first traversal of the tree and construct the list of child parent pairs.
            while (node_queue.size > 0) {
                auto pair = node_queue.pop_back();
                node_processing_list.push_back({ pair.child_idx, pair.parent_idx });

                const tinygltf::Node& node = model.nodes[pair.child_idx];
                for (i32 child_idx : node.children) {
                    node_queue.push_back({ u32(child_idx), u32(pair.child_idx) });
                }
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

                scene_graph.parent_ids[node_idx] = pair.parent_idx;
                // Translation
                for (size_t i = 0; i < node.translation.size(); i++) {
                    scene_graph.positions[node_idx][i] = float(node.translation[i]);
                }
                // Rotation
                for (size_t i = 0; i < node.rotation.size(); i++) {
                    scene_graph.rotations[node_idx][i] = float(node.rotation[i]);
                }
                // Scale
                for (size_t i = 0; i < node.scale.size(); i++) {
                    scene_graph.scales[node_idx][i] = float(node.scale[i]);
                }
            }


        }


        void process_textures(const tinygltf::Model& model, Context& out_context)
        {

        }

        void process_primitives(const tinygltf::Model& model, Context& out_context)
        {

        }

        void process_draw_calls(const tinygltf::Model& model, Context& out_context)
        {

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
            Array<ChildParentPair> node_processing_list{ model.nodes.size() };
            flatten_gltf_scene_graph(model, node_processing_list);

            // Copy Node List
            process_scene_graph(model, node_processing_list, out_context);

            // todo: Mark Joint Nodes?

            // Process Textures
            process_textures(model, out_context);

            // Process Primitives
            process_primitives(model, out_context);

            // Process Materials
            process_primitives(model, out_context);

            // Process Draw calls
            process_draw_calls(model, out_context);
        };
    }
}
