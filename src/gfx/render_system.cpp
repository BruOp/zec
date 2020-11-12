#include "pch.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"

namespace zec::RenderSystem
{
    void compile_render_list(RenderList& in_render_list, const RenderPassDesc* render_pass_descs, size_t num_render_passes)
    {
        ASSERT(in_render_list.setup_fns.size == 0);
        std::unordered_map<std::string, PassOutputDesc> output_descs = {};

        // Process outputs
        for (size_t i = 0; i < num_render_passes; i++) {
            const auto& render_pass = render_pass_descs[i];
            for (const auto& input : render_pass.inputs) {
                if (input.type == PassResourceType::INVALID || input.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // Check if resource exists and matches description
                std::string input_name = { input.name };
                if (output_descs.contains(input_name)) {
                    ASSERT(output_descs[input_name].type == input.type);
                    output_descs[input_name].usage = ResourceUsage(output_descs[input_name].usage | input.usage);
                }
                else {
                    ASSERT_FAIL("Cannot use an input that is not declared as an corresponding input");
                }
            }

            for (const auto& output : render_pass.outputs) {
                if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // We do not currently allow outputing to the same resource more than once in a render list
                ASSERT(!output_descs.contains(output.name));

                // Add it to our list
                output_descs[output.name] = output;
            }
        }

        // Okay, now our resource list should be complete
        // Now we want to process it, such that we can then look up the resources that each pass needs later on
        for (auto& pair : output_descs) {
            const std::string& name = pair.first;
            auto& resource_desc = pair.second;

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

        // Finally, loop through our pass descs again and store the details per pass
        in_render_list.setup_fns.reserve(num_render_passes);
        in_render_list.execute_fns.reserve(num_render_passes);
        in_render_list.destroy_fns.reserve(num_render_passes);
        in_render_list.context_data.reserve(num_render_passes);

        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
            const RenderPassDesc& render_pass_desc = render_pass_descs[pass_idx];
            in_render_list.setup_fns.push_back(render_pass_desc.setup);
            in_render_list.execute_fns.push_back(render_pass_desc.execute);
            in_render_list.destroy_fns.push_back(render_pass_desc.destroy);
            in_render_list.context_data.push_back(nullptr);
        }
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
