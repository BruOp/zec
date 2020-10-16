#include "pch.h"
#include "render_system.h"

namespace zec::RenderSystem
{
    void compile_render_list(RenderList& in_render_list, const RenderPassDesc* render_passes, size_t num_render_passes)
    {
        ASSERT(in_render_list.render_passes.size == 0);

        // Process outputs
        for (size_t i = 0; i < num_render_passes; i++) {
            const auto& render_pass = render_passes[i];
            for (const auto& output_desc : render_pass.outputs) {
                if (output_desc.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                const auto resource_map_end = in_render_list.resource_map.end();
                const auto entry_itr = in_render_list.resource_map.find(output_desc.name);
                if (entry_itr == resource_map_end) {
                    // Need to add it to our map and the output 
                    if (output_desc.type == PassResourceType::Buffer) {
                        in_render_list.buffers;
                    }
                    else {
                    }
                }
                else {
                    // Resource has already been stored, just need to confirm that the details are the same.
                }
            }

            // Process inputs, validate outputs
            for (size_t i = 0; i < num_render_passes; i++) {

            }
        }

    }
