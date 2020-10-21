#include "pch.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"

namespace zec::RenderSystem
{
    void compile_render_list(RenderList& in_render_list, const RenderPassDesc* render_pass_descs, size_t num_render_passes)
    {
        ASSERT(in_render_list.setup_fns.size == 0);
        std::unordered_map<std::string, PassResourceDesc> list_resource_descs = {};

        // Process outputs
        for (size_t i = 0; i < num_render_passes; i++) {
            const auto& render_pass = render_pass_descs[i];
            for (const auto& resource : render_pass.resources) {
                if (resource.type == PassResourceType::INVALID) {
                    break;
                }

                bool already_added = list_resource_descs.find(resource.name) != list_resource_descs.end();
                // The following ifs are all exclusive since in a single pass, we cannot use
                // the resource in multiple ways within the same pass.

                if (resource.usage & RESOURCE_USAGE_SHADER_READABLE) {
                    // Check if resource exists in output list already
                    ASSERT(already_added);
                }

                if (resource.usage & RESOURCE_USAGE_RENDER_TARGET || resource.usage & RESOURCE_USAGE_DEPTH_STENCIL) {
                    ASSERT(resource.type == PassResourceType::TEXTURE);

                    // We do not support UAV + SRV usage at this time
                    ASSERT((resource.usage & RESOURCE_USAGE_COMPUTE_WRITABLE) == 0);

                    // We are trying to read + write from an RTV, which is impossible, so we'll have to ping pong
                    // Let's just flag the resource as such so we can track this state later (not sure how to do this just yet...)
                    // We also know that it already exists in our list, so we don't need to check for it
                    if (resource.usage & RESOURCE_USAGE_SHADER_READABLE) {
                        // TODO: Flag that we need to ping pong this bad boy somehow
                    }
                    else {
                        // It doesn't make sense to have a WRITE_ONLY entry appear earlier in the list
                        ASSERT(!already_added);
                    }
                }

                if (already_added) {
                    list_resource_descs[resource.name].usage = ResourceUsage(list_resource_descs[resource.name].usage | resource.usage);
                }
                else {
                    list_resource_descs[resource.name] = resource;
                }
            }
        }

        // Okay, now our resource list should be complete
        // Now we want to process it, such that we can then look up the resources that each pass needs later on
        // The only trick bit is ping ponging... deferring this
        // Also let's write the function to actually access the data first
        for (auto& pair : list_resource_descs) {
            const auto& name = pair.first;
            auto& resource_desc = pair.second;

            // First we have to determine what type of resource we're creating:
            ASSERT(resource_desc.type != PassResourceType::INVALID);

            if (resource_desc.type == PassResourceType::BUFFER) {
                resource_desc.buffer_desc.usage = resource_desc.usage;
                BufferHandle buffer = create_buffer(resource_desc.buffer_desc);

                ResourceListEntry entry = {
                    .type = resource_desc.type,
                    .buffer = buffer
                };
                in_render_list.resource_map.insert({ name, entry });
            }
            else {

                resource_desc.texture_desc.usage = resource_desc.usage;
                TextureHandle texture = create_texture(resource_desc.texture_desc);
                ResourceListEntry entry = {
                    .type = resource_desc.type,
                    .texture = texture
                };
                in_render_list.resource_map.insert({ name, entry });
            }
        }

        // Finally, loop through our pass descs again and store the details per pass
        in_render_list.setup_fns.reserve(num_render_passes);
        in_render_list.execute_fns.reserve(num_render_passes);
        in_render_list.destroy_fns.reserve(num_render_passes);

        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
            const RenderPassDesc& render_pass_desc = render_pass_descs[pass_idx];
            in_render_list.setup_fns.push_back(render_pass_desc.setup);
            in_render_list.execute_fns.push_back(render_pass_desc.execute);
            in_render_list.destroy_fns.push_back(render_pass_desc.destroy);

            PassResourceList resources_per_pass = {};
            for (size_t resource_idx = 0; resource_idx < ARRAY_SIZE(render_pass_desc.resources); resource_idx++) {
                const auto& resource_desc = render_pass_desc.resources[resource_idx];
                if (resource_desc.type == PassResourceType::INVALID) {
                    break;
                }
                resources_per_pass.entries[resource_idx] = in_render_list.resource_map[resource_desc.name];
            }
            in_render_list.resources_per_pass.push_back(resources_per_pass);
        }

    }

    void setup(RenderList& render_list)
    {
        for (auto setup_fn : render_list.setup_fns) {
            setup_fn();
        }
    }

    void execute(RenderList& render_list)
    {
        // Since we don't model views at the API level, we'll just pass the handles back in the same order that the pass desc used them
        for (size_t i = 0; i < render_list.execute_fns.size; i++) {
            auto execute_fn = render_list.execute_fns[i];
            PassResourceList& pass_resources = render_list.resources_per_pass[i];
            execute_fn(pass_resources);
        }
    }


    void destroy(RenderList& render_list)
    {
        for (auto destroy_fn : render_list.destroy_fns) {
            destroy_fn();
        }

        // TODO: Destroy resources?
    }


}
