#include "pch.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"

namespace zec::RenderSystem
{
    using namespace zec::gfx;

    struct PassOutput
    {
        PassOutputDesc desc;
        ResourceUsage initial_state;
        u32 last_written_to_by_pass_idx;
    };

    constexpr size_t GRAPH_DEPTH_LIMIT = 16; //totally arbitrary

    void compile_render_list(RenderList& in_render_list, const RenderListDesc& render_list_desc)
    {
        const RenderPassDesc* render_pass_descs = render_list_desc.render_pass_descs;
        const u32 num_render_passes = render_list_desc.num_render_passes;
        const std::string backbuffer_resource_name = { render_list_desc.resource_to_use_as_backbuffer };

        in_render_list.backbuffer_resource_name = backbuffer_resource_name;

        // So in the new system we're going to:
        // Validate our input/outputs as before
        // Store the "latest" dependency as part of which fence to wait on -- only necessary for cross queue execution. Otherwise, we'll insert resource barriers.
        // The dependencies will need to be marked as "flushes" so that we know we should submit the work done so far before continuing

        ASSERT(in_render_list.render_passes.size == 0);
        std::unordered_map<std::string, PassOutput> output_descs = {};

        in_render_list.render_passes.grow(num_render_passes);
        in_render_list.receipts.grow(num_render_passes);

        // Process passes
        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {

            // Create RenderPass instance
            const auto& render_pass_desc = render_pass_descs[pass_idx];
            auto& render_pass = in_render_list.render_passes[pass_idx];
            render_pass.queue_type = render_pass_desc.queue_type;
            // Store function pointers
            render_pass.context = render_pass_desc.context;
            render_pass.setup = render_pass_desc.setup;
            render_pass.execute = render_pass_desc.execute;
            render_pass.destroy = render_pass_desc.destroy;
            render_pass.resource_transitions_view.offset = in_render_list.resource_transition_descs.size;

            // Validate inputs
            for (const auto& input : render_pass_desc.inputs) {
                if (input.type == PassResourceType::INVALID || input.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // Check if resource exists and matches description
                std::string input_name = { input.name };
                if (output_descs.contains(input_name)) {
                    ASSERT(output_descs[input_name].desc.type == input.type);
                    output_descs[input_name].desc.usage = ResourceUsage(output_descs[input_name].desc.usage | input.usage);

                    u32 parent_pass_idx = output_descs[input_name].last_written_to_by_pass_idx;
                    ASSERT(parent_pass_idx < pass_idx);

                    auto& parent_pass = in_render_list.render_passes[parent_pass_idx];
                    // Since this is a hard dependency, the command has to be submitted like we expect
                    parent_pass.requires_flush = true;

                    if (render_pass_descs[parent_pass_idx].queue_type != render_pass_desc.queue_type) {
                        render_pass.receipt_idx_to_wait_on = parent_pass_idx;
                    }

                    size_t transition_idx = in_render_list.resource_transition_descs.push_back({
                        .type = ResourceTransitionType(input.type),
                        .usage = input.usage });
                    strcpy(in_render_list.resource_transition_descs[transition_idx].name, input.name.c_str());
                }
                else {
                    ASSERT_FAIL("Cannot use an input that is not declared as an corresponding input");
                }
            }

            // Validate outputs and process descriptions
            bool writes_to_backbuffer_resource = false;
            for (const auto& output : render_pass_desc.outputs) {
                if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                std::string output_name = output.name;

                if (output_descs.contains(output_name)) {
                    u32 previously_written_by = output_descs[output_name].last_written_to_by_pass_idx;
                    auto& previous_pass = in_render_list.render_passes[previously_written_by];
                    previous_pass.requires_flush = true;

                    output_descs[output_name].last_written_to_by_pass_idx = pass_idx;

                }
                else {
                    // Add it to our list
                    output_descs[output_name].initial_state = output.usage;
                    output_descs[output_name].desc = output;
                    output_descs[output_name].last_written_to_by_pass_idx = pass_idx;

                    if (output_name == backbuffer_resource_name) {
                        ASSERT(output.type == PassResourceType::TEXTURE);
                        ASSERT(output.texture_desc.size_class == SizeClass::RELATIVE_TO_SWAP_CHAIN);
                        ASSERT(output.texture_desc.width == 1.0f);
                        ASSERT(output.texture_desc.height == 1.0f);
                        ASSERT(output.texture_desc.num_mips == 1);
                        ASSERT(output.texture_desc.depth == 1);
                        ASSERT(output.texture_desc.is_3d == false);
                        ASSERT(output.texture_desc.is_cubemap == false);
                        ASSERT(output.usage == RESOURCE_USAGE_RENDER_TARGET);

                        writes_to_backbuffer_resource = true;
                        render_pass.requires_flush = true;
                    }
                }

                size_t transition_idx = in_render_list.resource_transition_descs.push_back({
                    .type = ResourceTransitionType(output.type),
                    .usage = output.usage });
                strcpy(in_render_list.resource_transition_descs[transition_idx].name, output.name.c_str());

            }

            // Assert that the last pass writes to the backbuffer resource
            ASSERT(pass_idx != num_render_passes - 1 || writes_to_backbuffer_resource);

            render_pass.resource_transitions_view.size = in_render_list.resource_transition_descs.size - render_pass.resource_transitions_view.offset;

        }

        // Check that we do indeed write to the resource aliasing the backbuffer
        // We should probably also assert that the sizing and stuff is what we expect
        ASSERT(output_descs.count(backbuffer_resource_name) != 0);
        // We don't need to create this resource, so remove it.
        output_descs.erase(backbuffer_resource_name);

        // We now have:
        // - A list of resource descs we have to create
        // - A list containing the depths for each RenderPass
        // - A list of the number of passes at each depth in our tree

        // Process list of resources, creating each one RENDER_LATENCY times
        // (For each frame in flight)
        for (auto& pair : output_descs) {
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

                BufferHandle buffer = buffers::create(buffer_desc);
                ResourceState resource_state = {
                    .type = PassResourceType::BUFFER,
                    .last_usages = { pair.second.initial_state, pair.second.initial_state },
                };
                for (size_t j = 0; j < RENDER_LATENCY; j++) {

                    resource_state.buffers[j] = buffers::create(buffer_desc);;
                }
                in_render_list.resource_map[name] = resource_state;
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
                    .initial_state = pair.second.initial_state,
                };

                ResourceState resource_state = {
                    .type = PassResourceType::TEXTURE,
                    .last_usages = { pair.second.initial_state, pair.second.initial_state }
                };
                for (size_t j = 0; j < RENDER_LATENCY; j++) {
                    resource_state.textures[j] = textures::create(texture_desc);
                }
                in_render_list.resource_map[name] = resource_state;
            }
        }
    }

    void setup(RenderList& render_list)
    {
        for (auto& render_pass : render_list.render_passes) {
            render_pass.setup(render_pass.context);
        }
    }

    void execute(RenderList& render_list)
    {
        TextureHandle backbuffer = get_current_back_buffer_handle();
        // Alias the backbuffer as the resource;
        render_list.resource_map[render_list.backbuffer_resource_name] = {
            .type = PassResourceType::TEXTURE,
            .last_usages = { RESOURCE_USAGE_PRESENT, RESOURCE_USAGE_PRESENT },
            .textures = { backbuffer, backbuffer }, // Bit of a hack
        };

        // --------------- RECORD --------------- 
        constexpr size_t MAX_CMD_LIST_SUMISSIONS = 8; // Totally arbitrary
        // Each render_pass gets a command context
        Array<CommandContextHandle> cmd_contexts{ render_list.render_passes.size };
        // Can be parallelized!
        for (size_t pass_idx = 0; pass_idx < render_list.render_passes.size; ++pass_idx) {
            const auto& render_pass = render_list.render_passes[pass_idx];
            CommandContextHandle& cmd_ctx = cmd_contexts[pass_idx];
            cmd_ctx = gfx::cmd::provision(render_pass.queue_type);

            // Transition Resources
            {
                const size_t offset = render_pass.resource_transitions_view.offset;
                const size_t size = render_pass.resource_transitions_view.size;
                size_t num_recorded_transitions = 0;
                ASSERT(size < 15);
                ResourceTransitionDesc transition_descs[16] = {};
                // Submit any resource barriers
                for (size_t transition_idx = 0; transition_idx < size; transition_idx++) {
                    PassResourceTransitionDesc& transition_desc = render_list.resource_transition_descs[transition_idx + offset];
                    u64 current_frame_idx = get_current_frame_idx();
                    ResourceState& resource_state = render_list.resource_map.at(transition_desc.name);

                    if (transition_desc.usage == resource_state.last_usages[current_frame_idx]) continue;

                    if (transition_desc.type == ResourceTransitionType::BUFFER) {
                        transition_descs[num_recorded_transitions] = {
                            .type = transition_desc.type,
                            .buffer = resource_state.buffers[current_frame_idx],
                            .before = resource_state.last_usages[current_frame_idx],
                            .after = transition_desc.usage
                        };
                    }
                    else {
                        transition_descs[num_recorded_transitions] = {
                            .type = transition_desc.type,
                            .texture = resource_state.textures[current_frame_idx],
                            .before = resource_state.last_usages[current_frame_idx],
                            .after = transition_desc.usage
                        };
                    }
                    num_recorded_transitions++;
                    resource_state.last_usages[current_frame_idx] = transition_desc.usage;
                }
                if (num_recorded_transitions > 0) {
                    gfx::cmd::transition_resources(cmd_ctx, transition_descs, num_recorded_transitions);
                }
            }

            // Run our 
            render_pass.execute(render_list, cmd_ctx, render_pass.context);

            // Transition backbuffer
            if (pass_idx == render_list.render_passes.size - 1) {
                ResourceTransitionDesc transition_desc{
                    .type = ResourceTransitionType::TEXTURE,
                    .texture = backbuffer,
                    .before = RESOURCE_USAGE_RENDER_TARGET,
                    .after = RESOURCE_USAGE_PRESENT,
                };
                gfx::cmd::transition_resources(cmd_ctx, &transition_desc, 1);
            }
        }

        // --------------- SUBMIT --------------- 
        // TODO: Move this struct
        struct CommandStream
        {
            size_t pending_count = 0;
            CommandContextHandle contexts_to_submit[MAX_CMD_LIST_SUMISSIONS] = {};
        };

        CommandStream gfx_stream{};
        CommandStream async_compute_stream{};

        // Loop through all but the last command context
        for (size_t pass_idx = 0; pass_idx < render_list.render_passes.size; pass_idx++) {
            const auto& render_pass = render_list.render_passes[pass_idx];

            if (render_pass.receipt_idx_to_wait_on) {
                ASSERT(render_pass.receipt_idx_to_wait_on < pass_idx);
                const CmdReceipt& cmd_receipt = render_list.receipts[render_pass.receipt_idx_to_wait_on];
                ASSERT(render_pass.receipt_idx_to_wait_on != CmdReceipt::INVALID_FENCE_VALUE);
                gfx::cmd::gpu_wait(render_pass.queue_type, cmd_receipt);
            }

            CommandStream& stream = render_pass.queue_type == CommandQueueType::ASYNC_COMPUTE ? async_compute_stream : gfx_stream;
            stream.contexts_to_submit[stream.pending_count++] = cmd_contexts[pass_idx];

            // If we've reached the max or need to flush at this pass (for synchronization 
            // purposes), we submit our command contexts and store the receipt
            if (stream.pending_count == MAX_CMD_LIST_SUMISSIONS) {
                debug_print(L"We're hitting the max number of command context submissions, might want to investigate this!");
                gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                stream.pending_count = 0;
            }
            else if (render_pass.requires_flush) {
                render_list.receipts[pass_idx] = gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                stream.pending_count = 0;
            }

            // Otherwise we continue so that we can still batch our command contexts.
        }

        if (async_compute_stream.pending_count > 0) {
            gfx::cmd::return_and_execute(async_compute_stream.contexts_to_submit, async_compute_stream.pending_count);
        }

        // We cannot have any pending submissions at this point
        ASSERT(gfx_stream.pending_count == 0);

        // TODO: Figure out where to put PRESENT

    }


    void destroy(RenderList& render_list)
    {
        for (auto& render_pass : render_list.render_passes) {
            render_pass.setup(render_pass.context);
        }

        // TODO: Destroy resources?
    }

    BufferHandle get_buffer_resource(RenderList& render_list, const std::string& resource_name)
    {
        ResourceState resource = render_list.resource_map.at(resource_name);
        ASSERT(resource.type == PassResourceType::BUFFER);
        return resource.buffers[get_current_frame_idx()];
    }

    TextureHandle get_texture_resource(RenderList& render_list, const std::string& resource_name)
    {
        ResourceState resource = render_list.resource_map.at(resource_name);
        ASSERT(resource.type == PassResourceType::TEXTURE);
        return resource.textures[get_current_frame_idx()];
    }
}
