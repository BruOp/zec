#include "render_task_system.h"
#include "gfx/rhi.h"
#include "profiling_utils.h"

namespace zec::render_graph
{
    namespace
    {
        PassListBuilder::Result validate_usage(const ResourceManager* resource_context, const PassResourceUsage& resource_usage)
        {
            if (resource_usage.usage == rhi::RESOURCE_USAGE_UNUSED) {
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

    void ResourceManager::register_buffer(const BufferResourceDesc& buffer_desc)
    {
        ASSERT(resource_states.count(buffer_desc.identifier) == 0);
        ResourceState (&resource_state)[RENDER_LATENCY] = resource_states[buffer_desc.identifier];

        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            resource_state[i].buffer = prenderer->buffers_create(buffer_desc.desc);
            resource_state[i].queue_type = rhi::CommandQueueType::GRAPHICS; // TODO: Is this always true???
            resource_state[i].resource_usage = buffer_desc.initial_usage;
            prenderer->set_debug_name(resource_state[i].buffer, buffer_desc.name.data());
        }
    }

    void ResourceManager::register_texture(const TextureResourceDesc& texture_desc)
    {
        ASSERT(resource_states.count(texture_desc.identifier) == 0);

        rhi::TextureDesc temp_desc = texture_desc.desc;
        if (texture_desc.sizing == Sizing::RELATIVE_TO_SWAP_CHAIN) {
            temp_desc.width = render_config_state.width;
            temp_desc.height = render_config_state.height;
            temp_desc.depth = 1;
        }
        ResourceState (&resource_state)[RENDER_LATENCY] = resource_states[texture_desc.identifier];
        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            resource_state[i].texture = prenderer->textures_create(temp_desc);
            resource_state[i].queue_type = rhi::CommandQueueType::GRAPHICS; // TODO: Is this always true???
            resource_state[i].resource_usage = texture_desc.desc.initial_state;
            prenderer->set_debug_name(resource_state[i].texture, texture_desc.name.data());
        }
    }

    void ResourceManager::set_backbuffer_id(const ResourceIdentifier id)
    {
        ASSERT_MSG(!backbuffer_id.is_valid(), "Backbuffer resource has already been set for this context.");
        backbuffer_id = id;
        ResourceState(&resource_state)[RENDER_LATENCY] = resource_states[backbuffer_id];

        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            // The texture handle gets filled in by refresh_backbuffer
            resource_state[i].queue_type = rhi::CommandQueueType::GRAPHICS;
            resource_state[i].resource_usage = rhi::RESOURCE_USAGE_PRESENT;
        }
    }

    void ResourceManager::refresh_backbuffer()
    {
        ASSERT_MSG(backbuffer_id.is_valid(), "We need valid backbuffer ID, to store the current backbuffer handle with");
        rhi::TextureHandle backbuffer_handle = prenderer->get_current_back_buffer_handle();
        ResourceState(&resource_state)[RENDER_LATENCY] = resource_states[backbuffer_id];
        for (u32 i = 0; i < RENDER_LATENCY; ++i)
        {
            resource_state[i].texture = backbuffer_handle;
        }
    }

    void ResourceManager::set_barrier_state(const ResourceIdentifier id, const BarrierState barrier_state)
    {
        const u64 idx = prenderer->get_current_frame_idx() % RENDER_LATENCY;
        ResourceState(&resource_state)[RENDER_LATENCY] = resource_states.at(id);
        resource_state[idx].resource_usage = barrier_state.resource_usage;
        resource_state[idx].queue_type = barrier_state.queue_type;
    }

    rhi::BufferHandle ResourceManager::get_buffer(const ResourceIdentifier buffer_identifier) const
    {
        const u64 idx = prenderer->get_current_frame_idx() % RENDER_LATENCY;
        return resource_states.at(buffer_identifier)[idx].buffer;
    }

    rhi::TextureHandle ResourceManager::get_texture(const ResourceIdentifier texture_identifier) const
    {
        const u64 idx = prenderer->get_current_frame_idx() % RENDER_LATENCY;
        return resource_states.at(texture_identifier)[idx].texture;
    }

    BarrierState ResourceManager::get_barrier_state(const ResourceIdentifier resource_identifier) const
    {
        const u64 idx = prenderer->get_current_frame_idx() % RENDER_LATENCY;
        const ResourceState(&resource_state)[RENDER_LATENCY] = resource_states.at(resource_identifier);
        return {
            .resource_usage = resource_state[idx].resource_usage,
            .queue_type = resource_state[idx].queue_type,
        };
    }

    ResourceIdentifier ResourceManager::get_backbuffer_id()
    {
        return backbuffer_id;
    }

    ZecResult PipelineStore::compile(const PipelineId id, const PipelineCompilationDesc& desc, std::string& inout_errors)
    {
        ASSERT(!pso_handles.contains(id));
        ASSERT(!recompilation_infos.contains(id));

        rhi::ShaderBlobsHandle blobs_handle{};
        ZecResult res = prenderer->shaders_compile(desc.shader_compilation_desc, blobs_handle, inout_errors);

        if (res == ZecResult::SUCCESS)
        {
            rhi::ResourceLayoutHandle resource_layout = desc.resource_layout;
            rhi::PipelineStateHandle pso_handle = prenderer->pipelines_create(blobs_handle, resource_layout, desc.pso_desc);

            // Success!
            // Clean up after ourselves
            prenderer->shaders_release_blobs(blobs_handle);

            pso_handles.set(id, pso_handle);
            recompilation_infos.set(id, desc);;
        }
        return res;
    }

    ZecResult PipelineStore::recompile(const PipelineId pipeline_id, std::string& inout_errors)
    {
        ASSERT(pso_handles.contains(pipeline_id));
        ASSERT(recompilation_infos.contains(pipeline_id));

        const PipelineCompilationDesc& recompilation_desc = recompilation_infos.get(pipeline_id);
        rhi::ShaderBlobsHandle blobs_handle{};
        ZecResult res = prenderer->shaders_compile(recompilation_desc.shader_compilation_desc, blobs_handle, inout_errors);

        if (res == ZecResult::SUCCESS)
        {
            // Recompile pipeline id
            const rhi::ResourceLayoutHandle resource_layout_handle = recompilation_desc.resource_layout;
            const rhi::PipelineStateHandle old_pipeline_id = pso_handles.get(pipeline_id);

            // TODO: do we need to make this whole process async? How would that work?
            res = prenderer->pipelines_recreate(blobs_handle, resource_layout_handle, recompilation_desc.pso_desc, old_pipeline_id);

            prenderer->shaders_release_blobs(blobs_handle);
        }

        return res;
    }

    PassListBuilder::PassListBuilder(RenderTaskList* list_to_build) : out_list(list_to_build)
    {}

    PassListBuilder::Result PassListBuilder::add_pass(const PassDesc& render_pass_desc)
    {
        Result out_result{ StatusCodes::SUCCESS };
        u32 dependency_index = UINT32_MAX;

        if (out_list->resource_context == nullptr)
        {
            out_result = Result{ StatusCodes::MISSING_RESOURCE_MANAGER };
        }
        else if (!out_list->resource_context->get_backbuffer_id().is_valid())
        {
            out_result = Result{ StatusCodes::UNSPECIFIED_BACKBUFFER_RESOURCE };
        }
        else {

            ASSERT(render_pass_desc.command_queue_type != rhi::CommandQueueType::COPY && render_pass_desc.command_queue_type != rhi::CommandQueueType::NUM_COMMAND_CONTEXT_POOLS);

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
                const bool valid_backbuffer_use = output.type == PassResourceType::TEXTURE && output.usage == rhi::RESOURCE_USAGE_RENDER_TARGET;
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
            out_result.pass_handle = { pass_index };
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


    void PassListBuilder::set_resource_context(ResourceManager* resource_context)
    {
        out_list->resource_context = resource_context;
    }

    void PassListBuilder::set_shader_store(PipelineStore* shader_store)
    {
        out_list->shader_store = shader_store;
    }

    RenderTaskList::RenderTaskList(ResourceManager* resource_context, PipelineStore* shader_store)
        : resource_context{ resource_context }
        , shader_store{ shader_store }
        , settings_context{}
        , per_pass_data_store{}
    {}

    void RenderTaskList::execute()
    {
        PROFILE_EVENT("Pass Recording");

        resource_context->refresh_backbuffer();

        rhi::CommandContextHandle gfx_cmd_list{};
        rhi::CommandContextHandle async_compute_cmd_list{};

        u32 num_passes = passes.size();
        std::vector<rhi::CmdReceipt> cmd_receipts(num_passes);

        for (u64 pass_idx = 0; pass_idx < num_passes; ++pass_idx)
        {
            const auto& pass = passes[pass_idx];
            if (!pass.enabled)
            {
                continue;
            }

            auto& cmd_ctx = pass.desc.command_queue_type == rhi::CommandQueueType::ASYNC_COMPUTE ? async_compute_cmd_list : gfx_cmd_list;

            if (!is_valid(cmd_ctx))
            {
                cmd_ctx = prenderer->cmd_provision(pass.desc.command_queue_type);
            }

            // Transition resources
            constexpr size_t MAX_TRANSITION_COUNT = 16;
            FixedArray<rhi::ResourceTransitionDesc, MAX_TRANSITION_COUNT> transition_descs = {};
            ASSERT_MSG((pass.desc.inputs.size() + pass.desc.outputs.size()) < MAX_TRANSITION_COUNT,
                "Need to either bump our transitions desc count of %u or use a transient allocation for these", MAX_TRANSITION_COUNT);

            for (const auto& input : pass.desc.inputs)
            {
                // Transition inputs
                // Handle inter queue fluses
                BarrierState current_state = resource_context->get_barrier_state(input.identifier);

                if (current_state.queue_type == pass.desc.command_queue_type)
                {
                    if (current_state.resource_usage != input.usage)
                    {
                        if (input.type == PassResourceType::BUFFER)
                        {
                            transition_descs.push_back({
                                .type = rhi::ResourceTransitionType::BUFFER,
                                .buffer = resource_context->get_buffer(input.identifier),
                                .before = current_state.resource_usage,
                                .after = input.usage,
                            });
                        }
                        else
                        {
                            transition_descs.push_back({
                                .type = rhi::ResourceTransitionType::TEXTURE,
                                .texture = resource_context->get_texture(input.identifier),
                                .before = current_state.resource_usage,
                                .after = input.usage,
                            });
                        }

                        current_state.resource_usage = input.usage;
                        resource_context->set_barrier_state(input.identifier, current_state);
                    }
                    else if (input.usage == rhi::RESOURCE_USAGE_COMPUTE_WRITABLE)
                    {
                        // Need to insert a UAV transition here
                        if (input.type == PassResourceType::BUFFER)
                        {
                            prenderer->cmd_compute_write_barrier(cmd_ctx, resource_context->get_buffer(input.identifier));
                        }
                        else
                        {
                            prenderer->cmd_compute_write_barrier(cmd_ctx, resource_context->get_texture(input.identifier));
                        }
                    }
                }
                else
                {
                    // Don't need to insert resource barriers if we're using the resource between queues, but we do need a GPU wait
                    // We filled the dependency when adding the pass originally
                    ASSERT(pass.dependency_index != UINT32_MAX);
                    ASSERT(is_valid(cmd_receipts[pass.dependency_index]));
                    prenderer->cmd_gpu_wait(pass.desc.command_queue_type, cmd_receipts[pass.dependency_index]);
                    current_state.queue_type = pass.desc.command_queue_type;
                    current_state.resource_usage = input.usage;
                }
            }

            for (const auto& output : pass.desc.outputs)
            {
                // Transition outputs
                // Handle inter queue fluses
                BarrierState current_state = resource_context->get_barrier_state(output.identifier);

                if (current_state.queue_type == pass.desc.command_queue_type)
                {
                    if (current_state.resource_usage != output.usage)
                    {
                        if (output.type == PassResourceType::BUFFER)
                        {
                            transition_descs.push_back({
                                .type = rhi::ResourceTransitionType::BUFFER,
                                .buffer = resource_context->get_buffer(output.identifier),
                                .before = current_state.resource_usage,
                                .after = output.usage,
                                });
                        }
                        else
                        {
                            transition_descs.push_back({
                                .type = rhi::ResourceTransitionType::TEXTURE,
                                .texture = resource_context->get_texture(output.identifier),
                                .before = current_state.resource_usage,
                                .after = output.usage,
                                });
                        }

                        current_state.resource_usage = output.usage;
                        resource_context->set_barrier_state(output.identifier, current_state);
                    }
                    else if (output.usage == rhi::RESOURCE_USAGE_COMPUTE_WRITABLE)
                    {
                        // Need to insert a UAV transition here
                        if (output.type == PassResourceType::BUFFER)
                        {
                            prenderer->cmd_compute_write_barrier(cmd_ctx, resource_context->get_buffer(output.identifier));
                        }
                        else
                        {
                            prenderer->cmd_compute_write_barrier(cmd_ctx, resource_context->get_texture(output.identifier));
                        }
                    }
                }
                else
                {
                    // Don't need to insert resource barriers if we're using the resource between queues, but we do need a GPU wait
                    // We filled the dependency when adding the pass originally
                    ASSERT(pass.dependency_index != UINT32_MAX);
                    ASSERT(is_valid(cmd_receipts[pass.dependency_index]));
                    prenderer->cmd_gpu_wait(pass.desc.command_queue_type, cmd_receipts[pass.dependency_index]);
                    current_state.queue_type = pass.desc.command_queue_type;
                    current_state.resource_usage = output.usage;
                }
            }

            {
                PROFILE_GPU_EVENT(pass.desc.name.data(), cmd_ctx);

                if (transition_descs.size > 0)
                {
                    prenderer->cmd_transition_resources(cmd_ctx, transition_descs.data, transition_descs.size);
                }

                // Pass Execution
                PassExecutionContext execution_context = {
                   .resource_context = resource_context,
                   .pipeline_context = shader_store,
                   .settings_context = &settings_context,
                   .per_pass_data_store = &per_pass_data_store,
                   .cmd_context = cmd_ctx,
                };
                pass.desc.execute_fn(&execution_context);
            }

            if (pass.requires_flush)
            {
                cmd_receipts[pass_idx] = prenderer->cmd_return_and_execute(&cmd_ctx, 1);
                //cmd_ctx = CommandContextHandle{};
                ASSERT(!is_valid(cmd_ctx));
            }
        }

        ASSERT(is_valid(gfx_cmd_list));
        if (is_valid(gfx_cmd_list))
        {
            const auto backbuffer_id = resource_context->get_backbuffer_id();
            BarrierState backbuffer_state = resource_context->get_barrier_state(backbuffer_id);
            ASSERT(backbuffer_state.resource_usage == rhi::RESOURCE_USAGE_RENDER_TARGET);
            ASSERT(backbuffer_state.queue_type == rhi::CommandQueueType::GRAPHICS);
            // Add assert that it's in render target state?
            rhi::ResourceTransitionDesc transition_desc{
                .type = rhi::ResourceTransitionType::TEXTURE,
                .texture = resource_context->get_texture(backbuffer_id),
                .before = rhi::RESOURCE_USAGE_RENDER_TARGET,
                .after = rhi::RESOURCE_USAGE_PRESENT,
            };
            prenderer->cmd_transition_resources(gfx_cmd_list, &transition_desc, 1);
            prenderer->cmd_return_and_execute(&gfx_cmd_list, 1);

            backbuffer_state.resource_usage = rhi::RESOURCE_USAGE_PRESENT;
            resource_context->set_barrier_state(backbuffer_id, backbuffer_state);

        }

        if (is_valid(async_compute_cmd_list))
        {
            prenderer->cmd_return_and_execute(&async_compute_cmd_list, 1);
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
