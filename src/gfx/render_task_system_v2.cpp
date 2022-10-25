#include "render_task_system_v2.h"
#include "gfx.h"
#include "profiling_utils.h"
// TODO -- implement new system

namespace zec::render_graph
{
    namespace
    {
        PassListBuilder::Result validate_usage(const ResourceContext* resource_context, const PassResourceUsage& resource_usage)
        {
            if (resource_usage.usage == RESOURCE_USAGE_UNUSED) {
                return PassListBuilder::Result{ PassListBuilder::StatusCodes::RESOURCE_USAGE_INVALID, resource_usage.identifier };
            }

            bool resource_was_registered = false;
            if (resource_usage.type == PassResourceType::TEXTURE) {
                resource_was_registered = resource_context->has_texture(resource_usage.identifier);
            }
            else {
                resource_was_registered = resource_context->has_buffer(resource_usage.identifier);
            }

            if (!resource_was_registered) {
                return PassListBuilder::Result{ PassListBuilder::StatusCodes::UNREGISTERED_RESOURCE, resource_usage.identifier };
            }

            // TODO: Check usage? Not currently supported
            return PassListBuilder::Result{ PassListBuilder::StatusCodes::SUCCESS };
        }
    }

    void ResourceContext::register_buffer(const BufferResourceDesc& buffer_desc)
    {
        ASSERT(buffers.count(buffer_desc.identifier) == 0);
        // TODO : Combine resoruce state with the handle, so that we can't have silent conflicts in resource_states accidentally
        ASSERT(resource_states.count(buffer_desc.identifier) == 0);
        BufferHandle (&buffer_handles)[RENDER_LATENCY] = buffers[buffer_desc.identifier];

        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            buffer_handles[i] = gfx::buffers::create(buffer_desc.desc);
            gfx::set_debug_name(buffer_handles[i], buffer_desc.name.data());
        }

        resource_states.emplace(buffer_desc.identifier, buffer_desc.initial_usage);
    }

    void ResourceContext::register_texture(const TextureResourceDesc& texture_desc)
    {
        ASSERT(textures.count(texture_desc.identifier) == 0);
        // TODO : Combine resoruce state with the handle, so that we can't have silent conflicts in resource_states accidentally
        ASSERT(resource_states.count(texture_desc.identifier) == 0);

        TextureDesc temp_desc = texture_desc.desc;
        if (texture_desc.sizing == Sizing::RELATIVE_TO_SWAP_CHAIN) {
            temp_desc.width = render_config_state.width;
            temp_desc.height = render_config_state.height;
            temp_desc.depth = 1;
        }
        TextureHandle(&texture_handles)[RENDER_LATENCY] = textures[texture_desc.identifier];
        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            texture_handles[i] = gfx::textures::create(temp_desc);
            gfx::set_debug_name(texture_handles[i], texture_desc.name.data());
        }
        resource_states.emplace(texture_desc.identifier, texture_desc.desc.initial_state);
    }

    void ResourceContext::set_backbuffer_id(const ResourceIdentifier id)
    {
        ASSERT_MSG(!backbuffer_id.is_valid(), "Backbuffer resource has already been set for this context.");
        backbuffer_id = id;
        resource_states.emplace(backbuffer_id, ResourceState{ RESOURCE_USAGE_PRESENT, CommandQueueType::GRAPHICS });
    }

    void ResourceContext::refresh_backbuffer()
    {
        ASSERT_MSG(backbuffer_id.is_valid(), "We need valid backbuffer ID, to store the current backbuffer handle with");
        TextureHandle backbuffer_handle = gfx::get_current_back_buffer_handle();
        TextureHandle(&texture_handles)[RENDER_LATENCY] = textures[backbuffer_id];
        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            texture_handles[i] = backbuffer_handle;
        }
        resource_states.at(backbuffer_id) = ResourceState{ RESOURCE_USAGE_PRESENT, CommandQueueType::GRAPHICS };
    }

    void ResourceContext::set_resource_state(const ResourceIdentifier id, const ResourceState resource_usage)
    {
        resource_states.at(id) = resource_usage;
    }

    BufferHandle ResourceContext::get_buffer(const ResourceIdentifier buffer_identifier) const
    {
        const u64 idx = gfx::get_current_frame_idx() % RENDER_LATENCY;
        return buffers.at(buffer_identifier)[idx];
    }

    TextureHandle ResourceContext::get_texture(const ResourceIdentifier texture_identifier) const
    {
        const u64 idx = gfx::get_current_frame_idx() % RENDER_LATENCY;
        return textures.at(texture_identifier)[idx];
    }

    ResourceState ResourceContext::get_resource_state(const ResourceIdentifier resource_identifier) const
    {
        return resource_states.at(resource_identifier);
    }

    ResourceIdentifier ResourceContext::get_backbuffer_id()
    {
        return backbuffer_id;
    }

    void ShaderStore::register_resource_layout(const ResourceIdentifier id, const ResourceLayoutHandle handle)
    {
        ASSERT(resource_layouts.count(id) == 0);
        resource_layouts.emplace(id, handle);
    }

    void ShaderStore::register_pipeline(const ResourceIdentifier id, const PipelineStateHandle handle)
    {
        ASSERT(pipelines.count(id) == 0);
        pipelines.emplace(id, handle);
    }

    ResourceLayoutHandle ShaderStore::get_resource_layout(const ResourceIdentifier id) const
    {
        return resource_layouts.at(id);
    }

    PipelineStateHandle ShaderStore::get_pipeline(const ResourceIdentifier id) const
    {
        return pipelines.at(id);
    }

    PassListBuilder::PassListBuilder(RenderTaskList* list_to_build) : out_list(list_to_build)
    {}

    PassListBuilder::Result PassListBuilder::add_pass(const PassDesc& render_pass_desc)
    {
        Result out_result{ StatusCodes::SUCCESS };
        u32 dependency_index = UINT32_MAX;

        if (out_list->resource_context == nullptr)
        {
            out_result = Result{ StatusCodes::MISSING_RESOURCE_CONTEXT };
        }
        else if (!out_list->resource_context->get_backbuffer_id().is_valid())
        {
            out_result = Result{ StatusCodes::UNSPECIFIED_BACKBUFFER_RESOURCE };
        }
        else {

            ASSERT(render_pass_desc.command_queue_type != CommandQueueType::COPY || render_pass_desc.command_queue_type != CommandQueueType::NUM_COMMAND_CONTEXT_POOLS);

            // TODO: Want to validate that the inputs/outputs have been registered
            for (u32 i = 0; out_result.is_success() && i < render_pass_desc.inputs.size(); ++i)
            {
                const auto& input = render_pass_desc.inputs[i];
                if (input.identifier == out_list->resource_context->get_backbuffer_id())
                {
                    out_result = Result{ StatusCodes::BACKBUFFER_READ, input.identifier };
                }
                else if (!resource_write_ledger.contains(input.identifier))
                {
                    out_result = Result{ StatusCodes::MISSING_OUTPUT, input.identifier };
                }
                else
                {
                    const u32 last_written_to_by = resource_write_ledger.at(input.identifier);
                    ASSERT(last_written_to_by < out_list->passes.size());
                    const PassDesc& writer = out_list->passes[last_written_to_by].desc;
                    if (writer.command_queue_type != render_pass_desc.command_queue_type)
                    {
                        // Record the dependency
                        dependency_index = dependency_index == UINT32_MAX ? last_written_to_by : max(dependency_index, last_written_to_by);
                    }
                    out_result = validate_usage(out_list->resource_context, input);
                }
            }

            // TODO: Want to validate that the inputs/outputs have been registered
            for (u32 i = 0; out_result.is_success() && i < render_pass_desc.outputs.size(); ++i)
            {
                const auto& output = render_pass_desc.outputs[i];
                const bool is_backbuffer_output = output.identifier == out_list->resource_context->get_backbuffer_id();
                const bool valid_backbuffer_use = output.type == PassResourceType::TEXTURE && output.usage == RESOURCE_USAGE_RENDER_TARGET;
                if (is_backbuffer_output && !valid_backbuffer_use)
                {
                    out_result = Result{ StatusCodes::BACKBUFFER_USED_AS_NON_RT, output.identifier };
                }
                else if (!is_backbuffer_output) {
                    out_result = validate_usage(out_list->resource_context, output);
                }
            }
        }

        if (out_result.is_success())
        {
            // If we've gotten this far, validation has succeeded
            const u32 pass_index = out_list->passes.size();
            out_list->passes.push_back(RenderTaskList::Pass{
                .requires_flush = false,
                .dependency_index = dependency_index,
                .desc = render_pass_desc
            });

            for (const auto& output : render_pass_desc.outputs)
            {
                // Record last that we're the last writer to this resource
                resource_write_ledger[output.identifier] = pass_index;
            }

            // Mark any cross-queue dependencies as requiring a flush
            if (dependency_index != UINT32_MAX)
            {
                out_list->passes[dependency_index].requires_flush = true;
            }
        }

        return out_result;
    }

    void PassListBuilder::set_pass_list(RenderTaskList* pass_list)
    {
        ASSERT(out_list == nullptr);
        ASSERT(pass_list->passes.size() == 0);
        out_list = pass_list;
    }


    void PassListBuilder::set_resource_context(ResourceContext* resource_context)
    {
        out_list->resource_context = resource_context;
    }

    void PassListBuilder::set_shader_store(ShaderStore* shader_store)
    {
        out_list->shader_store = shader_store;
    }

    RenderTaskList::RenderTaskList(ResourceContext* resource_context, ShaderStore* shader_store)
        : resource_context{ resource_context }
        , shader_store{ shader_store }
        , settings_context{}
        , per_pass_data_store{}
    {}

    void RenderTaskList::execute()
    {
        PROFILE_EVENT("Pass Recording");

        resource_context->refresh_backbuffer();

        CommandContextHandle gfx_cmd_list{};
        CommandContextHandle async_compute_cmd_list{};

        u32 num_passes = passes.size();
        std::vector<CmdReceipt> cmd_receipts(num_passes);

        for (u64 pass_idx = 0; pass_idx < num_passes; ++pass_idx)
        {
            const auto& pass = passes[pass_idx];
            auto& cmd_ctx = pass.desc.command_queue_type == CommandQueueType::ASYNC_COMPUTE ? async_compute_cmd_list : gfx_cmd_list;

            if (!is_valid(cmd_ctx))
            {
                cmd_ctx = gfx::cmd::provision(pass.desc.command_queue_type);
            }

            // Transition resources
            constexpr size_t MAX_TRANSITION_COUNT = 16;
            FixedArray<ResourceTransitionDesc, MAX_TRANSITION_COUNT> transition_descs = {};
            ASSERT_MSG((pass.desc.inputs.size() + pass.desc.outputs.size()) < MAX_TRANSITION_COUNT,
                "Need to either bump our transitions desc count of %u or use a transient allocation for these", MAX_TRANSITION_COUNT);

            for (const auto& input : pass.desc.inputs)
            {
                // Transition inputs
                // Handle inter queue fluses
                ResourceState current_state = resource_context->get_resource_state(input.identifier);

                if (current_state.queue_type == pass.desc.command_queue_type)
                {
                    if (current_state.resource_usage != input.usage)
                    {
                        if (input.type == PassResourceType::BUFFER)
                        {
                            transition_descs.push_back({
                                .type = ResourceTransitionType::BUFFER,
                                .buffer = resource_context->get_buffer(input.identifier),
                                .before = current_state.resource_usage,
                                .after = input.usage,
                            });
                        }
                        else
                        {
                            transition_descs.push_back({
                                .type = ResourceTransitionType::TEXTURE,
                                .texture = resource_context->get_texture(input.identifier),
                                .before = current_state.resource_usage,
                                .after = input.usage,
                            });
                        }

                        current_state.resource_usage = input.usage;
                        resource_context->set_resource_state(input.identifier, current_state);
                    }
                    else if (input.usage == RESOURCE_USAGE_COMPUTE_WRITABLE)
                    {
                        // Need to insert a UAV transition here
                        if (input.type == PassResourceType::BUFFER)
                        {
                            gfx::cmd::compute_write_barrier(cmd_ctx, resource_context->get_buffer(input.identifier));
                        }
                        else
                        {
                            gfx::cmd::compute_write_barrier(cmd_ctx, resource_context->get_texture(input.identifier));
                        }
                    }
                }
                else
                {
                    // Don't need to insert resource barriers if we're using the resource between queues, but we do need a GPU wait
                    // We filled the dependency when adding the pass originally
                    ASSERT(pass.dependency_index != UINT32_MAX);
                    ASSERT(is_valid(cmd_receipts[pass.dependency_index]));
                    gfx::cmd::gpu_wait(pass.desc.command_queue_type, cmd_receipts[pass.dependency_index]);
                    current_state.queue_type = pass.desc.command_queue_type;
                    current_state.resource_usage = input.usage;
                }
            }

            for (const auto& output : pass.desc.outputs)
            {
                // Transition outputs
                // Handle inter queue fluses
                ResourceState current_state = resource_context->get_resource_state(output.identifier);

                if (current_state.queue_type == pass.desc.command_queue_type)
                {
                    if (current_state.resource_usage != output.usage)
                    {
                        if (output.type == PassResourceType::BUFFER)
                        {
                            transition_descs.push_back({
                                .type = ResourceTransitionType::BUFFER,
                                .buffer = resource_context->get_buffer(output.identifier),
                                .before = current_state.resource_usage,
                                .after = output.usage,
                                });
                        }
                        else
                        {
                            transition_descs.push_back({
                                .type = ResourceTransitionType::TEXTURE,
                                .texture = resource_context->get_texture(output.identifier),
                                .before = current_state.resource_usage,
                                .after = output.usage,
                                });
                        }

                        current_state.resource_usage = output.usage;
                        resource_context->set_resource_state(output.identifier, current_state);
                    }
                    else if (output.usage == RESOURCE_USAGE_COMPUTE_WRITABLE)
                    {
                        // Need to insert a UAV transition here
                        if (output.type == PassResourceType::BUFFER)
                        {
                            gfx::cmd::compute_write_barrier(cmd_ctx, resource_context->get_buffer(output.identifier));
                        }
                        else
                        {
                            gfx::cmd::compute_write_barrier(cmd_ctx, resource_context->get_texture(output.identifier));
                        }
                    }
                }
                else
                {
                    // Don't need to insert resource barriers if we're using the resource between queues, but we do need a GPU wait
                    // We filled the dependency when adding the pass originally
                    ASSERT(pass.dependency_index != UINT32_MAX);
                    ASSERT(is_valid(cmd_receipts[pass.dependency_index]));
                    gfx::cmd::gpu_wait(pass.desc.command_queue_type, cmd_receipts[pass.dependency_index]);
                    current_state.queue_type = pass.desc.command_queue_type;
                    current_state.resource_usage = output.usage;
                }
            }

            {
                PROFILE_GPU_EVENT(pass.desc.name.data(), cmd_ctx);

                if (transition_descs.size > 0)
                {
                    gfx::cmd::transition_resources(cmd_ctx, transition_descs.data, transition_descs.size);
                }

                // Pass Execution
                PassExecutionContext execution_context = {
                   .resource_context = resource_context,
                   .shader_context = shader_store,
                   .settings_context = &settings_context,
                   .per_pass_data_store = &per_pass_data_store,
                   .cmd_context = cmd_ctx,
                };
                pass.desc.execute_fn(&execution_context);
            }

            if (pass.requires_flush)
            {
                cmd_receipts[pass_idx] = gfx::cmd::return_and_execute(&cmd_ctx, 1);
                //cmd_ctx = CommandContextHandle{};
                ASSERT(!is_valid(cmd_ctx));
            }
        }

        ASSERT(is_valid(gfx_cmd_list));
        if (is_valid(gfx_cmd_list))
        {
            const auto backbuffer_id = resource_context->get_backbuffer_id();
            // Add assert that it's in render target state?
            ResourceTransitionDesc transition_desc{
                .type = ResourceTransitionType::TEXTURE,
                .texture = resource_context->get_texture(backbuffer_id),
                .before = RESOURCE_USAGE_RENDER_TARGET,
                .after = RESOURCE_USAGE_PRESENT,
            };
            gfx::cmd::transition_resources(gfx_cmd_list, &transition_desc, 1);
            gfx::cmd::return_and_execute(&gfx_cmd_list, 1);

        }

        if (is_valid(async_compute_cmd_list))
        {
            gfx::cmd::return_and_execute(&async_compute_cmd_list, 1);
        }
    }

    void RenderTaskList::setup()
    {
        PROFILE_EVENT("Pass Setup");

        for (const auto& pass : passes)
        {
            if (pass.desc.setup_fn != nullptr)
            {
                pass.desc.setup_fn(&settings_context, &per_pass_data_store);
            }
        }

    }
}
