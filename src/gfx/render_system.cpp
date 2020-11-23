#include "pch.h"
#include "murmur/MurmurHash3.h"
#include "render_system.h"

namespace zec::RenderSystem
{

    struct PassOutput
    {
        PassOutputDesc desc;
        ResourceUsage initial_state;
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

    void compile_render_list(RenderList& in_render_list, const RenderListDesc& render_list_desc)
    {
        const RenderPassDesc* render_pass_descs = render_list_desc.render_pass_descs;
        const u32 num_render_passes = render_list_desc.num_render_passes;
        const std::string backbuffer_resource_name = { render_list_desc.resource_to_use_as_backbuffer };

        // So in the new system we're going to:
        // Validate our input/outputs as before
        // Store the "latest" dependency as part of which fence to wait on -- only necessary for cross queue execution. Otherwise, we'll insert resource barriers.
        // The dependencies will need to be marked as "flushes" so that we know we should submit the work done so far before continuing
        // We'll also need to transition our resources at the beginning of passes.
        // There's an ambiguity where if several passes require a resource to be in say a READ state, which pass should perform the transition? Should transitions just be executed

        ASSERT(in_render_list.render_passes.size == 0);
        std::unordered_map<std::string, PassOutput> outputs = {};

        in_render_list.render_passes.grow(num_render_passes);

        // Process outputs
        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
            const auto& render_pass_desc = render_pass_descs[pass_idx];
            auto& render_pass = in_render_list.render_passes[pass_idx];
            render_pass.queue_type = render_pass_desc.queue_type;
            // Store function pointers
            render_pass.setup = render_pass_desc.setup;
            render_pass.execute = render_pass_desc.execute;
            render_pass.destroy = render_pass_desc.destroy;

            u32 num_inputs;
            for (const auto& input : render_pass_desc.inputs) {
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

                    auto& parent_pass = in_render_list.render_passes[parent_pass_idx];
                    // Since this is a hard dependency, the command has to be submitted like we expect
                    parent_pass.requires_flush = true;

                    if (render_pass_descs[parent_pass_idx].queue_type != render_pass_desc.queue_type) {

                        render_pass.receipt_idx_to_wait_on = parent_pass_idx;
                    }

                }
                else {
                    ASSERT_FAIL("Cannot use an input that is not declared as an corresponding input");
                }
            }

            bool writes_to_backbuffer_resource = false;
            for (const auto& output : render_pass_desc.outputs) {
                if (output.type == PassResourceType::INVALID || output.usage == RESOURCE_USAGE_UNUSED) {
                    break;
                }

                // We do not currently allow outputing to the same resource more than once in a render list
                std::string output_name = output.name;
                ASSERT(!outputs.contains(output_name));

                // Add it to our list
                outputs[output_name].initial_state = output.usage;
                outputs[output_name].desc = output;
                outputs[output_name].output_by_pass_idx = pass_idx;

                writes_to_backbuffer_resource = writes_to_backbuffer_resource || output_name == backbuffer_resource_name;
            }

            // Asser that the last pass writes to the backbuffer resource
            ASSERT(pass_idx != num_render_passes - 1 || writes_to_backbuffer_resource);
        }

        ASSERT(outputs.count(backbuffer_resource_name) != 0);
        outputs.erase(backbuffer_resource_name);

        // We now have:
        // - A list of resource descs we have to create
        // - A list containing the depths for each RenderPass
        // - A list of the number of passes at each depth in our tree

        std::unordered_map<std::string, ResourceUsage> expected_state{ outputs.size() };

        // Process list of resources
        for (auto& pair : outputs) {
            const std::string& name = pair.first;
            auto& resource_desc = pair.second.desc;

            expected_state[name] = pair.second.initial_state;

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

        in_render_list.backbuffer_resource_name = backbuffer_resource_name;


        // Create list of Resource Transtitions
        // Note: During the rendering, we'll actually track the current state of the resource and
        // omit and redundant resource transitions
        // Note: We'll also deo some additional book keeping on how many tansitions there are per
        // bucket

        // TODO: How do we manage transition to WRITE modes?

        // I think  instead we could we could split our transition lists into two different lists:
        // READ and WRITE modes
        // Then each pass can transition their outputs after the first pass as well
        // What's an alternative? First pass transition list?
        // Well one possibility is to just keep it in the same list but store a size that's actually
        // smaller than what we need and then correct it afterwards

        auto& resource_transitions = in_render_list.resource_transitions;
        for (size_t pass_idx = 0; pass_idx < num_render_passes; pass_idx++) {
            auto& render_pass = in_render_list.render_passes[pass_idx];
            const RenderPassDesc& pass_desc = render_pass_descs[pass_idx];
            render_pass.resource_transitions_view.offset = resource_transitions.size;
            for (const auto& input : pass_desc.inputs) {

                // Because we only allow output transitions once
                if (expected_state[input.name] == input.usage) {
                    continue;
                }

                if (input.type == PassResourceType::BUFFER) {
                    BufferHandle buffer = in_render_list.buffers_map[input.name];
                    resource_transitions.push_back({
                        .type = ResourceTransitionType(input.type),
                        .buffer = buffer,
                        .before = expected_state[input.name],
                        .after = input.usage });
                }
                else {
                    TextureHandle texture = in_render_list.textures_map[input.name];
                    resource_transitions.push_back({
                        .type = ResourceTransitionType(input.type),
                        .texture = texture,
                        .before = expected_state[input.name],
                        .after = input.usage });
                }
                expected_state[input.name] = input.usage;
            }

            render_pass.resource_transitions_view.size = resource_transitions.size - render_pass.resource_transitions_view.offset;

            // Store additional transitions for the outputs. We'll only use these after the first frame
            for (const auto& output : pass_desc.outputs) {
                // Do not record redundant resource transitions
                if (expected_state[output.name] == output.usage) {
                    continue;
                }

                if (output.type == PassResourceType::BUFFER) {
                    BufferHandle buffer = in_render_list.buffers_map[output.name];
                    resource_transitions.push_back({
                        .type = ResourceTransitionType(output.type),
                        .buffer = buffer,
                        .before = expected_state[output.name],
                        .after = output.usage });
                }
                else {
                    TextureHandle texture = in_render_list.textures_map[output.name];
                    resource_transitions.push_back({
                        .type = ResourceTransitionType(output.type),
                        .texture = texture,
                        .before = expected_state[output.name],
                        .after = output.usage });
                }
            }
        }

        // For now let's just:

        // We'll also need to add a RTV transition before the first pass that writes to this resource
        // We probably also want to ensure that we never use the promoted resource as anything other than
        // an RTV in our 

        // How do we add the RTV transition? Well since we only write to a resource once in the current system
        // we kind of don't have to worry about it

        // Idea: Present flag?

        // TODO: Write some tests to see if this returns what we expect. Should be able to use dummy
        // data, although maybe we need to break this up to make it a bit more testable? Only thing
        // we'd have to mock for sure is the calls that create our resources.
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
        render_list.textures_map[render_list.backbuffer_resource_name] = backbuffer;

        // --------------- RECORD --------------- 
        constexpr size_t MAX_CMD_LIST_SUMISSIONS = 8; // Totally arbitrary
        // Each render_pass gets a command context
        Array<CommandContextHandle> cmd_contexts{ render_list.render_passes.size };
        // Can be parallelized!
        for (size_t pass_idx = 0; pass_idx < render_list.render_passes.size; ++pass_idx) {
            const auto& render_pass = render_list.render_passes[pass_idx];
            CommandContextHandle& cmd_ctx = cmd_contexts[pass_idx];
            cmd_ctx = gfx::cmd::provision(render_pass.queue_type);

            // Submit any resource barriers
            ResourceTransitionDesc* transitions = &render_list.resource_transitions[render_pass.resource_transitions_view.offset];
            u32 num_transitions = render_pass.resource_transitions_view.size;
            if (pass_idx == render_list.render_passes.size - 1) {
                // This is knid of a stupid way of doing this.
                ResourceTransitionDesc* final_pass_transitions = reinterpret_cast<ResourceTransitionDesc*>(_alloca((num_transitions + 1) * sizeof(ResourceTransitionDesc)));
                memory::copy(final_pass_transitions, transitions, sizeof(ResourceTransitionDesc) * num_transitions);
                final_pass_transitions[num_transitions] = ResourceTransitionDesc{ .type = ResourceTransitionType::TEXTURE, .texture = get_current_back_buffer_handle }
            }
            gfx::cmd::transition_resources(cmd_ctx, transitions, num_transitions);

            render_pass.execute(render_list, cmd_ctx, render_pass.context);
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


}
