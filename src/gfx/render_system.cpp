#include "pch.h"
#include "core/array.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"
#include "profiling_utils.h"

namespace zec::render_pass_system
{
    using namespace zec::gfx;

    struct PassOutput
    {
        PassOutputDesc desc;
        ResourceUsage initial_state;
        u32 last_written_to_by_pass_idx;
    };

    constexpr size_t GRAPH_DEPTH_LIMIT = 16; //totally arbitrary

    //boolean validate_render_List(const RenderPassListDesc& render_list_desc, std::vector<std::string>& out_errors)
    //{
    //    const u32 num_render_passes = render_list_desc.num_render_passes;
    //    if (num_render_passes == 0) {
    //        out_errors.push_back("You cannot create a render list with zero passes");
    //    };

    //    const u64 backbuffer_resource_id = render_list_desc.resource_to_use_as_backbuffer;

    //    bool backbuffer_is_written_to = false;
    //    std::unordered_map<u64, PassOutputDesc> output_descs = {};

    //    for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {

    //        // Create RenderPass instance
    //        const auto& render_pass_desc = render_list_desc.render_pass_descs[pass_idx];

    //        // Validate inputs
    //        for (const auto& input : render_pass_desc.inputs) {
    //            if (input.type == PassResourceType::INVALID || input.usage == RESOURCE_USAGE_UNUSED) {
    //                break;
    //            }

    //            // Check if resource exists and matches description
    //            u64 input_id = { input.id };
    //            if (output_descs.contains(input_id)) {
    //                const auto& output_desc = output_descs[input_id];
    //                if (output_desc.type != input.type) {
    //                    out_errors.push_back(make_string("The type of input %u for pass %s does not match between output and input. Output uses format %u while input uses %u.", input_id, render_pass_desc.name, input.type, output_desc.type));
    //                };
    //            }
    //            else {
    //                out_errors.push_back(make_string("Input %u is not output by a pass before being consumed by %s", input_id, render_pass_desc.name));
    //            }
    //        }

    //        // Validate outputs and process descriptions
    //        for (const auto& output : render_pass_desc.outputs) {
    //            if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
    //                break;
    //            }

    //            u64 output_id = output.id;

    //            if (!output_descs.contains(output_id)) {
    //                // Add it to our list
    //                output_descs[output_id] = output;
    //            }

    //            if (output_id == backbuffer_resource_id) {
    //                const bool is_valid = output.type == PassResourceType::TEXTURE
    //                    && output.texture_desc.size_class == SizeClass::RELATIVE_TO_SWAP_CHAIN
    //                    && output.texture_desc.width == 1.0f
    //                    && output.texture_desc.height == 1.0f
    //                    && output.texture_desc.num_mips == 1
    //                    && output.texture_desc.depth == 1
    //                    && output.texture_desc.is_3d == false
    //                    && output.texture_desc.is_cubemap == false
    //                    && output.usage == RESOURCE_USAGE_RENDER_TARGET;
    //                if (!is_valid) {
    //                    out_errors.push_back(make_string("Passes that write to the backbuffer must have a sepcific set of outputs!"));
    //                }
    //                backbuffer_is_written_to = true;
    //            }
    //        }
    //    }

    //    if (!backbuffer_is_written_to) {
    //        out_errors.push_back(make_string("None of the passes seem to output to the resource %u which is marked as the backbuffer", backbuffer_resource_id));
    //    }

    //    return out_errors.size() == 0;
    //};

    void RenderPassList::compile(RenderPassListDesc& render_list_desc)
    {
        //#if _DEBUG
        //    std::vector<std::string> errors{};
        //    bool is_valid = validate_render_List(render_list_desc, errors);
        //    if (!is_valid) {
        //        for (size_t i = 0; i < errors.size(); i++) {
        //            write_log(errors[i].c_str());
        //        }
        //        throw std::runtime_error("Cannot compile invalid render list! Consider calling validate separately");
        //    }
        //#endif // _DEBUG

        IRenderPass** in_render_passes = render_list_desc.render_passes;
        const u32 num_render_passes = render_list_desc.num_render_passes;
        backbuffer_resource_id = render_list_desc.resource_to_use_as_backbuffer;

        // So in the new system we're going to:
        // Validate our input/outputs as before
        // Store the "latest" dependency as part of which fence to wait on -- only necessary for cross queue execution. Otherwise, we'll insert resource barriers.
        // The dependencies will need to be marked as "flushes" so that we know we should submit the work done so far before continuing

        ASSERT(render_passes.size == 0);
        std::unordered_map<u64, PassOutput> output_descs = {};

        render_passes.grow(num_render_passes);
        per_pass_submission_info.grow(num_render_passes);
        receipts.grow(num_render_passes);
        cmd_contexts.grow(num_render_passes);

        // Process passes
        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {

            // Create RenderPass instance
            render_passes[pass_idx] = in_render_passes[pass_idx];
            IRenderPass* render_pass = render_passes[pass_idx];

            PerPassSubmissionInfo& pass_submission_info = per_pass_submission_info[pass_idx];
            pass_submission_info = {
                .resource_transitions_view = {
                    .offset = u32(resource_transition_descs.size),
                    .size = 0,
                },
                .receipt_idx_to_wait_on = UINT32_MAX,
                .requires_flush = false,
            };

            // Validate inputs
            IRenderPass::InputList inputs = render_pass->get_input_list();
            for (size_t i = 0; i < inputs.count; ++i) {
                const PassInputDesc& input = inputs.ptr[i];
                if (input.type == PassResourceType::INVALID || input.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // Check if resource exists and matches description
                u64 input_id = input.id;
                if (output_descs.contains(input_id)) {
                    ASSERT(output_descs[input_id].desc.type == input.type);
                    ResourceUsage previous_usage = output_descs[input_id].desc.usage;
                    output_descs[input_id].desc.usage = ResourceUsage(output_descs[input_id].desc.usage | input.usage);

                    u32 parent_pass_idx = output_descs[input_id].last_written_to_by_pass_idx;
                    ASSERT(parent_pass_idx < pass_idx);

                    auto& parent_pass = render_passes[parent_pass_idx];
                    auto& parent_pass_submission_info = per_pass_submission_info[parent_pass_idx];

                    // Since this is a hard dependency, the command has to be submitted like we expect
                    parent_pass_submission_info.requires_flush = true;

                    if (render_passes[parent_pass_idx]->get_queue_type() != render_pass->get_queue_type()) {
                        pass_submission_info.receipt_idx_to_wait_on = parent_pass_idx;
                    }

                    size_t transition_idx = resource_transition_descs.push_back({
                        .resource_id = input_id,
                        .type = ResourceTransitionType(input.type),
                        .usage = input.usage });
                }
                else {
                    ASSERT_FAIL("Cannot use an input that is not declared as an corresponding input");
                }
            }

            // Validate outputs and process descriptions
            bool writes_to_backbuffer_resource = false;
            const IRenderPass::OutputList outputs = render_pass->get_output_list();
            for (size_t i = 0; i < outputs.count; ++i) {
                const auto& output = outputs.ptr[i];
                if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                u64 output_id = output.id;

                if (output_descs.contains(output_id)) {
                    u32 previously_written_by = output_descs[output_id].last_written_to_by_pass_idx;

                    auto& previous_pass_submission_info = per_pass_submission_info[previously_written_by];
                    previous_pass_submission_info.requires_flush = true;

                    output_descs[output_id].last_written_to_by_pass_idx = pass_idx;
                }
                else {
                    // Add it to our list
                    output_descs[output_id].initial_state = output.usage;
                    output_descs[output_id].desc = output;
                    output_descs[output_id].last_written_to_by_pass_idx = pass_idx;
                }

                if (output_id == backbuffer_resource_id) {
                    writes_to_backbuffer_resource = true;
                    pass_submission_info.requires_flush = true;
                }

                size_t transition_idx = resource_transition_descs.push_back({
                    .resource_id = output_id,
                    .type = ResourceTransitionType(output.type),
                    .usage = output.usage });

            }

            // Assert that the last pass writes to the backbuffer resource
            ASSERT(pass_idx != num_render_passes - 1 || writes_to_backbuffer_resource);

            pass_submission_info.resource_transitions_view.size = resource_transition_descs.size - pass_submission_info.resource_transitions_view.offset;

        }

        // Check that we do indeed write to the resource aliasing the backbuffer
        ASSERT(output_descs.count(backbuffer_resource_id) != 0);
        // We don't need to create this resource, so remove it from our list.
        output_descs.erase(backbuffer_resource_id);

        // Process list of resources, creating one for each frame
        for (auto& pair : output_descs) {
            const u64& name = pair.first;
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

                ResourceState resource_state = {
                    .type = PassResourceType::BUFFER,
                    .last_usages = { pair.second.initial_state, pair.second.initial_state },
                };
                for (size_t j = 0; j < RENDER_LATENCY; j++) {

                    resource_state.buffers[j] = buffers::create(buffer_desc);
                    std::wstring debug_name = ansi_to_wstring(resource_desc.name) + to_string(j);
                    set_debug_name(resource_state.buffers[j], debug_name.c_str());

                }
                resource_map[name] = resource_state;
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
                    std::wstring debug_name = ansi_to_wstring(resource_desc.name) + to_string(j);
                    set_debug_name(resource_state.textures[j], debug_name.c_str());
                }
                resource_map[name] = resource_state;
            }
        }
    }

    void RenderPassList::setup()
    {
        for (auto& render_pass : render_passes) {
            render_pass->setup();
        }
    }

    void RenderPassList::copy()
    {
        for (auto& render_pass : render_passes) {
            render_pass->copy();
        }
    }

    void RenderPassList::execute()
    {

        TextureHandle backbuffer = get_current_back_buffer_handle();
        // Alias the backbuffer as the resource;
        resource_map[backbuffer_resource_id] = {
            .type = PassResourceType::TEXTURE,
            .last_usages = { RESOURCE_USAGE_PRESENT, RESOURCE_USAGE_PRESENT },
            .textures = { backbuffer, backbuffer }, // Bit of a hack
        };

        // --------------- RECORD --------------- 

        constexpr size_t MAX_CMD_LIST_SUMISSIONS = 8; // Totally arbitrary
        // Each render_pass gets a command context

        // Can be parallelized!
        for (size_t pass_idx = 0; pass_idx < render_passes.size; ++pass_idx) {
            IRenderPass* render_pass = render_passes[pass_idx];
            const PerPassSubmissionInfo pass_submission_info = per_pass_submission_info[pass_idx];

            CommandContextHandle& cmd_ctx = cmd_contexts[pass_idx];
            cmd_ctx = gfx::cmd::provision(render_pass->get_queue_type());

            PROFILE_GPU_EVENT(render_pass->get_name(), cmd_ctx);
            // Transition Resources
            {
                const size_t offset = pass_submission_info.resource_transitions_view.offset;
                const size_t size = pass_submission_info.resource_transitions_view.size;
                size_t num_recorded_transitions = 0;
                ASSERT(size < 15);
                ResourceTransitionDesc transition_descs[16] = {};
                // Submit any resource barriers
                for (size_t transition_idx = 0; transition_idx < size; transition_idx++) {
                    PassResourceTransitionDesc& transition_desc = resource_transition_descs[transition_idx + offset];
                    u64 current_frame_idx = get_current_frame_idx();
                    ResourceState& resource_state = resource_map.at(transition_desc.resource_id);

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

            render_pass->record(resource_map, cmd_ctx);

            // Transition backbuffer
            if (pass_idx == render_passes.size - 1) {
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
        {
            PROFILE_EVENT("Render System Submission");

            struct CommandStream
            {
                size_t pending_count = 0;
                CommandContextHandle contexts_to_submit[MAX_CMD_LIST_SUMISSIONS] = {};
            };

            CommandStream gfx_stream{};
            CommandStream async_compute_stream{};

            // Loop through all but the last command context
            for (size_t pass_idx = 0; pass_idx < render_passes.size; pass_idx++) {
                const IRenderPass* render_pass = render_passes[pass_idx];
                const PerPassSubmissionInfo pass_submission_info = per_pass_submission_info[pass_idx];


                if (pass_submission_info.receipt_idx_to_wait_on != UINT32_MAX) {
                    ASSERT(pass_submission_info.receipt_idx_to_wait_on < pass_idx);
                    const CmdReceipt& cmd_receipt = receipts[pass_submission_info.receipt_idx_to_wait_on];
                    ASSERT(is_valid(cmd_receipt));
                    gfx::cmd::gpu_wait(render_pass->get_queue_type(), cmd_receipt);
                }

                CommandStream& stream = render_pass->get_queue_type() == CommandQueueType::ASYNC_COMPUTE ? async_compute_stream : gfx_stream;
                stream.contexts_to_submit[stream.pending_count++] = cmd_contexts[pass_idx];

                // If we've reached the max or need to flush at this pass (for synchronization 
                // purposes), we submit our command contexts and store the receipt
                if (stream.pending_count == MAX_CMD_LIST_SUMISSIONS) {
                    debug_print(L"We're hitting the max number of command context submissions, might want to investigate this!");
                    gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                    stream.pending_count = 0;
                }
                else if (pass_submission_info.requires_flush) {
                    receipts[pass_idx] = gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                    stream.pending_count = 0;
                }

                // Otherwise we continue so that we can still batch our command contexts.
            }

            if (async_compute_stream.pending_count > 0) {
                gfx::cmd::return_and_execute(async_compute_stream.contexts_to_submit, async_compute_stream.pending_count);
            }

            // We cannot have any pending submissions at this point
            ASSERT(gfx_stream.pending_count == 0);

        }

    }


    void RenderPassList::shutdown()
    {
        for (auto& render_pass : render_passes) {
            render_pass->shutdown();
        }

        // TODO: Free resources
    }

    BufferHandle ResourceMap::get_buffer_resource(const u64 resource_id) const
    {
        ResourceState resource = internal_map.at(resource_id);
        ASSERT(resource.type == PassResourceType::BUFFER);
        return resource.buffers[get_current_frame_idx()];
    }

    TextureHandle ResourceMap::get_texture_resource(const u64 resource_id) const
    {
        ResourceState resource = internal_map.at(resource_id);
        ASSERT(resource.type == PassResourceType::TEXTURE);
        return resource.textures[get_current_frame_idx()];
    }
}
