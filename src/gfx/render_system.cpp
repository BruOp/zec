#include "pch.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"

namespace zec::RenderSystem
{
    struct ResourceTransitionDesc
    {
        PassResourceType type;
        union
        {
            BufferHandle buffer = {};
            TextureHandle texture;
        };
        ResourceUsage after;
    };

    struct PassOutput
    {
        PassOutputDesc desc;
        u32 output_by_pass_idx;
    };

    constexpr size_t GRAPH_DEPTH_LIMIT = 16; //totally arbitrary

    struct ArrayView
    {
        u32 offset = 0;
        u32 size = 0;
    };

    struct GraphInfo
    {
        u32 max_depth = 0;
        ArrayView bucketed_passes_info[GRAPH_DEPTH_LIMIT] = {  };
        ArrayView transition_views_per_depth[GRAPH_DEPTH_LIMIT] = {  };
    };

    void compile_render_list(RenderList& in_render_list, const RenderPassDesc* render_pass_descs, size_t num_render_passes)
    {
        ASSERT(in_render_list.setup_fns.size == 0);
        std::unordered_map<std::string, PassOutput> outputs = {};
        Array<u32> pass_depths{ num_render_passes };

        GraphInfo graph_info{};

        in_render_list.setup_fns.reserve(num_render_passes);
        in_render_list.execute_fns.reserve(num_render_passes);
        in_render_list.destroy_fns.reserve(num_render_passes);
        in_render_list.context_data.reserve(num_render_passes);

        // Process outputs
        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
            const auto& render_pass = render_pass_descs[pass_idx];

            u32 depth = 0;
            u32 num_inputs;

            for (const auto& input : render_pass.inputs) {
                if (input.type == PassResourceType::INVALID || input.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // Check if resource exists and matches description
                std::string input_name = { input.name };
                if (outputs.contains(input_name)) {
                    ASSERT(outputs[input_name].desc.type == input.type);
                    outputs[input_name].desc.usage = ResourceUsage(outputs[input_name].desc.usage | input.usage);

                    u32 parent_pass_idx = outputs[input_name].output_by_pass_idx;
                    ASSERT(parent_pass_idx < pass_idx);
                    depth = max(depth, pass_depths[parent_pass_idx] + 1);
                }
                else {
                    ASSERT_FAIL("Cannot use an input that is not declared as an corresponding input");
                }
            }
            ASSERT(depth < GRAPH_DEPTH_LIMIT);
            pass_depths[pass_idx] = depth;
            graph_info.bucketed_passes_info[depth].size += 1;
            graph_info.max_depth = max(depth, graph_info.max_depth);

            for (const auto& output : render_pass.outputs) {
                if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // We do not currently allow outputing to the same resource more than once in a render list
                ASSERT(!outputs.contains(output.name));

                // Add it to our list
                outputs[output.name].desc = output;
                outputs[output.name].output_by_pass_idx = pass_idx;
                //++num_outputs;
            }

            // Store the function pointers
            in_render_list.setup_fns.push_back(render_pass.setup);
            in_render_list.execute_fns.push_back(render_pass.execute);
            in_render_list.destroy_fns.push_back(render_pass.destroy);
            in_render_list.context_data.push_back(nullptr);
        }

        // We now have:
        // - A list of resource descs we have to create
        // - A list containing the depths for each RenderPass
        // - A list of the number of passes at each depth in our tree

        // Now we can:
        // - Create all of our resources
        // - Put our render passes into "Buckets" based on their depth
        // - Create a list of resource transitions 

        // Process list of resources
        for (auto& pair : outputs) {
            const std::string& name = pair.first;
            auto& resource_desc = pair.second.desc;

            // First we have to determine what type of resource we're creating:
            ASSERT(resource_desc.type != PassResourceType::INVALID);

            if (resource_desc.type == PassResourceType::BUFFER) {
                BufferDesc buffer_desc = {
                    .usage = resource_desc.usage,
                    .type = resource_desc.buffer_desc.type,
                    .byte_size = resource_desc.buffer_desc.byte_size,
                    .stride = resource_desc.buffer_desc.stride,
                };

                BufferHandle buffer = create_buffer(buffer_desc);

                in_render_list.buffers_map.insert({ name, buffer });
            }
            else {
                u32 width, height;
                if (resource_desc.texture_desc.size_class == SizeClass::ABSOLUTE) {
                    width = u32(resource_desc.texture_desc.width);
                    height = u32(resource_desc.texture_desc.height);
                }
                else {
                    TextureInfo backbuffer_info = gfx::textures::get_texture_info(get_current_back_buffer_handle());
                    width = u32(resource_desc.texture_desc.width * float(backbuffer_info.width));
                    height = u32(resource_desc.texture_desc.height * float(backbuffer_info.height));
                }
                TextureDesc texture_desc = {
                    .width = width,
                    .height = height,
                    .depth = resource_desc.texture_desc.depth,
                    .num_mips = resource_desc.texture_desc.num_mips,
                    .array_size = resource_desc.texture_desc.array_size,
                    .is_cubemap = resource_desc.texture_desc.is_cubemap,
                    .is_3d = resource_desc.texture_desc.is_3d,
                    .format = resource_desc.texture_desc.format,
                    .usage = resource_desc.usage,
                };

                TextureHandle texture = create_texture(texture_desc);
                in_render_list.textures_map.insert({ name, texture });
            }
        }

        Array<RenderPassDesc> bucketed_list(num_render_passes);
        // Create "bucketed" list
        {
            for (size_t depth_idx = 0; depth_idx < graph_info.max_depth; depth_idx++) {
                graph_info.bucketed_passes_info[depth_idx + 1].offset = graph_info.bucketed_passes_info[depth_idx].offset + graph_info.bucketed_passes_info[depth_idx].size;
            }

            u32 counter_per_depth[GRAPH_DEPTH_LIMIT] = { 0 };

            for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
                u32 graph_depth = pass_depths[pass_idx];
                u32 offset = graph_info.bucketed_passes_info[graph_depth].offset + counter_per_depth[graph_depth];
                counter_per_depth[graph_depth] += 1;
                bucketed_list[offset] = render_pass_descs[pass_idx];
            }
        }

        // Create list of Resource Transtitions
        Array<ResourceTransitionDesc> bucketed_transitions;

        // For each different depth
        // For each pass descs at this depth
        // For each output, append an after state + resource ptr
        // For each input, append an after state + resource ptr

        // Note: During the rendering, we'll actually track the current state of the resource and
        // omit and redundant resource transitions
        // Note: We'll also do some additional book keeping on how many tansitions there are per
        // bucket
        const u32 transition_list_offset = 0;

        for (size_t depth = 0; depth < graph_info.max_depth; depth++) {
            ArrayView per_depth_info = graph_info.bucketed_passes_info[depth];
            u32 offset = per_depth_info.offset;
            u32 size = per_depth_info.size;
            for (size_t pass_idx = offset; pass_idx < offset + size; pass_idx++) {
                RenderPassDesc& pass_desc = bucketed_list[pass_idx];
                for (const auto& input : pass_desc.inputs) {
                    graph_info.transition_views_per_depth[depth].size++;

                    if (input.type == PassResourceType::BUFFER) {
                        BufferHandle buffer = in_render_list.buffers_map[input.name];
                        bucketed_transitions.push_back({
                            .type = input.type,
                            .buffer = buffer,
                            .after = input.usage });
                    }
                    else {
                        TextureHandle texture = in_render_list.textures_map[input.name];
                        bucketed_transitions.push_back({
                            .type = input.type,
                            .texture = texture,
                            .after = input.usage });
                    }
                }

                for (const auto& output : pass_desc.outputs) {
                    graph_info.transition_views_per_depth[depth].size++;

                    if (output.type == PassResourceType::BUFFER) {
                        BufferHandle buffer = in_render_list.buffers_map[output.name];
                        bucketed_transitions.push_back({
                            .type = output.type,
                            .buffer = buffer,
                            .after = output.usage });
                    }
                    else {
                        TextureHandle texture = in_render_list.textures_map[output.name];
                        bucketed_transitions.push_back({
                            .type = output.type,
                            .texture = texture,
                            .after = output.usage });
                    }
                }
            }

            // Write offset for next depth
            graph_info.transition_views_per_depth[depth + 1].offset = graph_info.transition_views_per_depth[depth].size;
        }
        // TODO: Actually store this stuff somewhere

        // TODO: Write some tests to see if this returns what we expect. Should be able to use dummy
        // data, although maybe we need to break this up to make it a bit more testable? Only thing
        // we'd have to mock for sure is the calls that create our resources.
    }

    void setup(RenderList& render_list)
    {
        for (size_t i = 0; i < render_list.setup_fns.size; i++) {
            void* context = render_list.context_data[i];
            render_list.setup_fns[i](context);
        }
    }

    void execute(RenderList& render_list)
    {
        // Since we don't model views at the API level, we'll just pass the handles back in the same order that the pass desc used them
        for (size_t i = 0; i < render_list.execute_fns.size; i++) {
            void* context = render_list.context_data[i];
            render_list.execute_fns[i](render_list, context);
        }
    }


    void destroy(RenderList& render_list)
    {
        for (size_t i = 0; i < render_list.execute_fns.size; i++) {
            void* context = render_list.context_data[i];
            render_list.execute_fns[i](render_list, context);
        }

        // TODO: Destroy resources?
    }


}
