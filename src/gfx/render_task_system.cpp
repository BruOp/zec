#include "render_task_system.h"
#include "utils/utils.h"
#include "gfx/profiling_utils.h"
#include <ftl/task_scheduler.h>
#include <ftl/task_counter.h>
#include <sstream>

namespace zec
{
    using namespace gfx;

    BufferHandle ResourceMap::get_buffer_resource(const u32 resource_id) const
    {
        ResourceState resource = internal_map.at(resource_id);
        ASSERT(resource.type == PassResourceType::BUFFER);
        return resource.buffers[get_current_frame_idx()];
    }

    TextureHandle ResourceMap::get_texture_resource(const u32 resource_id) const
    {
        ResourceState resource = internal_map.at(resource_id);
        ASSERT(resource.type == PassResourceType::TEXTURE);
        return resource.textures[get_current_frame_idx()];
    }

    void ResourceMap::set_backbuffer_resource(const u32 backbuffer_identifier)
    {
        const auto backbuffer = get_current_back_buffer_handle();
        internal_map[backbuffer_identifier] = {
            .type = PassResourceType::TEXTURE,
            .last_usages = { RESOURCE_USAGE_PRESENT, RESOURCE_USAGE_PRESENT },
            .textures = { backbuffer, backbuffer }, // Bit of a hack
        };
    }

    void validate_usage(const RenderTaskSystem* render_task_system, const RenderPassResourceUsage& resource_usage, std::vector<std::string>& errors)
    {
        if (resource_usage.usage == RESOURCE_USAGE_UNUSED) {
            std::ostringstream ss{};
            ss << resource_usage.name << " cannot be used as UNUSED";
            errors.push_back(ss.str());
        }
        bool resource_was_registered = false;
        const auto& texture_descs = render_task_system->texture_resource_descs;
        const auto& buffer_descs = render_task_system->buffer_resource_descs;


        if (resource_usage.type == PassResourceType::TEXTURE) {
            resource_was_registered = texture_descs.contains(resource_usage.identifier);
        }
        else {
            resource_was_registered = render_task_system->buffer_resource_descs.contains(resource_usage.identifier);
        }

        if (!resource_was_registered) {
            std::ostringstream ss{};
            ss << resource_usage.name << " was not registered.";
            errors.push_back(ss.str());
            return;
        }

        if (resource_usage.type == PassResourceType::TEXTURE) {
            if ((texture_descs.at(resource_usage.identifier).desc.usage & resource_usage.usage) != resource_usage.usage) {
                std::ostringstream ss{};
                ss << resource_usage.name << " was not registered with correct usage.";
                errors.push_back(ss.str());
            }
        }
        else if ((buffer_descs.at(resource_usage.identifier).desc.usage & resource_usage.usage) != resource_usage.usage) {
            std::ostringstream ss{};
            ss << resource_usage.name << " was not registered with correct usage.";
            errors.push_back(ss.str());
        }
    }

    RenderTaskListHandle RenderTaskSystem::create_render_task_list(const RenderTaskListDesc& desc, std::vector<std::string>& errors)
    {
        bool writes_to_backbuffer_resource = false;
        size_t num_passes = desc.render_pass_task_descs.size();
        std::vector<RenderPassTask> render_pass_tasks(num_passes);
        std::vector<PassResourceTransitionDesc> resource_transition_descs{ };
        // This just tracks which pass has written to each resource last
        std::unordered_map<u64, u32> resources_last_written_by = {};

        const u32 backbuffer_resource_id = desc.backbuffer_resource_id;

        const std::span<RenderPassTaskDesc>& pass_descs = desc.render_pass_task_descs;

        for (size_t pass_idx = 0; pass_idx < num_passes; pass_idx++) {
            const RenderPassTaskDesc& task_desc = pass_descs[pass_idx];

            render_pass_tasks[pass_idx] = RenderPassTask{
                .name = task_desc.name,
                .setup_fn = task_desc.setup_fn,
                .execute_fn = task_desc.execute_fn,
                .teardown_fn = task_desc.teardown_fn,
                .command_queue_type = task_desc.command_queue_type,
                .resource_transitions_view = {
                    .offset = u32(resource_transition_descs.size()),
                    .size = 0,
                },
                .receipt_idx_to_wait_on = UINT32_MAX,
                .requires_flush = false,
            };

            for (size_t input_idx = 0; input_idx < task_desc.inputs.size(); ++input_idx) {
                const RenderPassResourceUsage& input = task_desc.inputs[input_idx];


                u32 input_id = input.identifier;
                bool already_being_written_to = resources_last_written_by.contains(input_id);

                // Can't read from the backbuffer resource
                if (input_id == backbuffer_resource_id) {
                    std::ostringstream ss{};
                    ss << task_desc.name << " reads from the backbuffer resource, " << input.name;
                    errors.push_back(ss.str());
                }
                else {
                    validate_usage(this, input, errors);
                }

                // - Marks the outputing pass as the "parent" pass, requiring a flush if they are different queues
                if (already_being_written_to) {
                    u32 parent_pass_idx = resources_last_written_by[input_id];

                    if (render_pass_tasks[parent_pass_idx].command_queue_type != task_desc.command_queue_type) {
                        render_pass_tasks[parent_pass_idx].requires_flush = true;
                        render_pass_tasks[pass_idx].receipt_idx_to_wait_on = parent_pass_idx;
                    }

                    resource_transition_descs.push_back({
                        .resource_id = input_id,
                        .type = ResourceTransitionType(input.type),
                        .usage = input.usage });

                }
                else {
                    std::ostringstream ss{};
                    ss << input.name << " is not output by another pass before being used as an input by " << task_desc.name;
                    errors.push_back(ss.str());
                }
            }

            for (size_t output_idx = 0; output_idx < task_desc.outputs.size(); output_idx++) {
                const auto& output = task_desc.outputs[output_idx];

                if (output.identifier == backbuffer_resource_id) {
                    if (output.usage != RESOURCE_USAGE_RENDER_TARGET) {
                        std::ostringstream ss{};
                        ss << task_desc.name << " attempts to use the backbuffer resource " << output.name << " as something other than the backbuffer, which isn't allowed.";
                        errors.push_back(ss.str());
                    }
                }
                else {
                    validate_usage(this, output, errors);
                }

                resources_last_written_by[output.identifier] = pass_idx;
                // Add resource_transition_desc
                resource_transition_descs.push_back({
                    .resource_id = output.identifier,
                    .type = ResourceTransitionType(output.type),
                    .usage = output.usage });
                if (output.identifier == backbuffer_resource_id) {
                    writes_to_backbuffer_resource = true;
                    render_pass_tasks[output_idx].requires_flush = true;
                }
            }

            render_pass_tasks[pass_idx].resource_transitions_view.size = resource_transition_descs.size() - render_pass_tasks[pass_idx].resource_transitions_view.offset;
        }

        if (!writes_to_backbuffer_resource) {
            errors.push_back({ "No passes write to the resource identified as the backbuffer resource" });
        }

        // Don't need to go through the pain of creating resource layouts and pipelines, etc. if there are already errors
        if (errors.size() > 0) {
            //return invalid handle
            return RenderTaskListHandle{ k_invalid_handle };
        }

        for (const auto& render_pass_desc : desc.render_pass_task_descs) {
            // Register resource layouts, pipeline states and settings
            for (const auto& setting_desc : render_pass_desc.settings) {
                settings.create_settings(setting_desc);
            }

            // Register resource layouts, pipeline states and settings
            for (const auto& resource_layout_desc : render_pass_desc.resource_layout_descs) {
                if (resource_layout_descs.count(resource_layout_desc.identifier) == 0) {
                    resource_layout_descs.insert({ resource_layout_desc.identifier, resource_layout_desc });
                }
            }

            // Register resource layouts, pipeline states and settings
            for (const auto& pipeline_desc : render_pass_desc.pipeline_descs) {
                if (pipeline_state_descs.count(pipeline_desc.identifier) == 0) {
                    pipeline_state_descs.insert({ pipeline_desc.identifier, pipeline_desc });
                }
            }
        }

        // Fill task list with relevant details
        u32 render_task_list_idx = u32(render_task_lists.create_back());
        RenderTaskList& render_task_list = render_task_lists[render_task_list_idx];

        ASSERT(render_task_list.render_pass_tasks.size() == 0);
        render_task_list.backbuffer_resource_id = backbuffer_resource_id;
        render_task_list.cmd_contexts.resize(num_passes);
        render_task_list.receipts.resize(num_passes);
        render_task_list.render_pass_tasks = std::move(render_pass_tasks);
        render_task_list.resource_transition_descs = std::move(resource_transition_descs);
        render_task_list.per_pass_data.resize(num_passes);

        return { render_task_list_idx };
    }

    void RenderTaskSystem::complete_setup()
    {
        for (const auto& [_, desc] : resource_layout_descs) {
            ResourceLayoutHandle resource_layout_handle = gfx::pipelines::create_resource_layout(desc.resource_layout_desc);
            resource_layouts.insert({ desc.identifier, resource_layout_handle });
        }

        for (auto& [_, desc] : pipeline_state_descs) {
            desc.pipeline_desc.resource_layout = resource_layouts.at(desc.resource_layout_id);
            PipelineStateHandle pipeline_handle = gfx::pipelines::create_pipeline_state_object(desc.pipeline_desc);
            pipelines.insert({ desc.identifier, pipeline_handle });
        }

        for (const auto& [id, buffer_desc] : buffer_resource_descs) {
            ResourceState resource_state = {
                    .type = PassResourceType::BUFFER,
                    .last_usages = { buffer_desc.initial_usage, buffer_desc.initial_usage },
            };
            for (size_t j = 0; j < RENDER_LATENCY; j++) {
                resource_state.buffers[j] = buffers::create(buffer_desc.desc);
                std::wstring debug_name = ansi_to_wstring(buffer_desc.name.data()) + to_string(j);
                set_debug_name(resource_state.buffers[j], debug_name.c_str());
            }
            resource_map[id] = resource_state;
        }

        const auto config_state = gfx::get_config_state();

        for (const auto& [id, texture_resource_desc] : texture_resource_descs) {
            TextureDesc texture_desc = texture_resource_desc.desc;
            ResourceState resource_state = {
                    .type = PassResourceType::TEXTURE,
                    .last_usages = { texture_desc.initial_state, texture_desc.initial_state },
            };

            if (texture_resource_desc.sizing == Sizing::RELATIVE_TO_SWAP_CHAIN) {
                texture_desc.width = config_state.width;
                texture_desc.height = config_state.height;
                texture_desc.depth = 1;
            }

            for (size_t j = 0; j < RENDER_LATENCY; j++) {
                resource_state.textures[j] = textures::create(texture_desc);
                std::wstring debug_name = ansi_to_wstring(texture_resource_desc.name.data()) + to_string(j);
                set_debug_name(resource_state.textures[j], debug_name.c_str());
            }
            resource_map[id] = resource_state;
        }

        for (size_t task_list_idx = 0; task_list_idx < render_task_lists.size; task_list_idx++) {
            auto& render_task_list = render_task_lists[task_list_idx];
            for (size_t pass_idx = 0; pass_idx < render_task_list.render_pass_tasks.size(); pass_idx++) {
                const auto& pass = render_task_list.render_pass_tasks[pass_idx];
                if (pass.setup_fn != nullptr) {
                    pass.setup_fn(&render_task_list.per_pass_data[pass_idx]);
                }
            }
        }
    }

    void RenderTaskSystem::execute(const RenderTaskListHandle list, ftl::TaskScheduler* task_scheduler)
    {
        RenderTaskList& render_task_list = render_task_lists[list.idx];

        TextureHandle backbuffer = get_current_back_buffer_handle();

        // Alias the backbuffer as the resource;
        resource_map.set_backbuffer_resource(render_task_list.backbuffer_resource_id);

        // --------------- RECORD --------------- 

        constexpr size_t MAX_CMD_LIST_SUMISSIONS = 8; // Totally arbitrary
        // Each render_pass gets a command context

        const auto& render_passes = render_task_list.render_pass_tasks;

        std::vector<RenderPassContext> task_contexts(render_passes.size());
        std::vector<ftl::Task> ftl_tasks(render_passes.size());

        {
            PROFILE_EVENT("Creating Task Contexts");

            // Can be parallelized!
            for (size_t pass_idx = 0; pass_idx < render_passes.size(); ++pass_idx) {
                const RenderPassTask& render_pass = render_passes[pass_idx];

                CommandContextHandle cmd_ctx = gfx::cmd::provision(render_pass.command_queue_type);
                render_task_list.cmd_contexts[pass_idx] = cmd_ctx;

                // Transition Resources
                {
                    const size_t offset = render_pass.resource_transitions_view.offset;
                    const size_t size = render_pass.resource_transitions_view.size;
                    size_t num_recorded_transitions = 0;
                    ASSERT(size < 15);
                    ResourceTransitionDesc transition_descs[16] = {};
                    // Submit any resource barriers
                    for (size_t transition_idx = 0; transition_idx < size; transition_idx++) {
                        PassResourceTransitionDesc& transition_desc = render_task_list.resource_transition_descs[transition_idx + offset];
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

                constexpr auto task_execution_fn = [](ftl::TaskScheduler* task_scheduler, void* arg) {
                    (void)task_scheduler;

                    const RenderPassContext* task_context = reinterpret_cast<RenderPassContext*>(arg);
                    PROFILE_GPU_EVENT(task_context->name.data(), task_context->cmd_context);

                    task_context->execute_fn(task_context);
                };

                task_contexts[pass_idx] = {
                    .name = render_pass.name,
                    .execute_fn = render_pass.execute_fn,
                    .resource_map = &resource_map,
                    .settings = &settings,
                    .resource_layouts = &resource_layouts,
                    .pipeline_states = &pipelines,
                    .cmd_context = cmd_ctx,
                    .per_pass_data = &render_task_list.per_pass_data[pass_idx]
                };

                ftl_tasks[pass_idx] = {
                    .Function = task_execution_fn,
                    .ArgData = &task_contexts[pass_idx]
                };
            }
        }

        {
            PROFILE_EVENT("MT Pass Recording");

            ftl::TaskCounter counter{ task_scheduler };
            task_scheduler->AddTasks(ftl_tasks.size(), ftl_tasks.data(), ftl::TaskPriority::High, &counter);

            task_scheduler->WaitForCounter(&counter, true);
        }

        // Transition backbuffer
        {
            ResourceTransitionDesc transition_desc{
                .type = ResourceTransitionType::TEXTURE,
                .texture = backbuffer,
                .before = RESOURCE_USAGE_RENDER_TARGET,
                .after = RESOURCE_USAGE_PRESENT,
            };
            gfx::cmd::transition_resources(task_contexts[task_contexts.size() - 1].cmd_context, &transition_desc, 1);
        }

        // --------------- SUBMIT --------------- 
        {
            PROFILE_EVENT("Render System Submission");

            constexpr size_t MAX_CMD_LIST_SUMISSIONS = 8; // Totally arbitrary
            struct CommandStream
            {
                size_t pending_count = 0;
                CommandContextHandle contexts_to_submit[MAX_CMD_LIST_SUMISSIONS] = {};
            };

            CommandStream gfx_stream{};
            CommandStream async_compute_stream{};

            // Loop through all but the last command context
            for (size_t pass_idx = 0; pass_idx < render_passes.size(); pass_idx++) {
                const RenderPassTask& render_pass_task = render_task_list.render_pass_tasks[pass_idx];

                if (render_pass_task.receipt_idx_to_wait_on != UINT32_MAX) {
                    ASSERT(render_pass_task.receipt_idx_to_wait_on < pass_idx);
                    const CmdReceipt& cmd_receipt = render_task_list.receipts[render_pass_task.receipt_idx_to_wait_on];
                    ASSERT(is_valid(cmd_receipt));
                    gfx::cmd::gpu_wait(render_pass_task.command_queue_type, cmd_receipt);
                }

                CommandStream& stream = render_pass_task.command_queue_type == CommandQueueType::ASYNC_COMPUTE ? async_compute_stream : gfx_stream;
                stream.contexts_to_submit[stream.pending_count++] = render_task_list.cmd_contexts[pass_idx];

                // If we've reached the max or need to flush at this pass (for synchronization 
                // purposes), we submit our command contexts and store the receipt
                if (stream.pending_count == MAX_CMD_LIST_SUMISSIONS) {
                    debug_print(L"We're hitting the max number of command context submissions, might want to investigate this!");
                    gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                    stream.pending_count = 0;
                }
                else if (render_pass_task.requires_flush) {
                    render_task_list.receipts[pass_idx] = gfx::cmd::return_and_execute(stream.contexts_to_submit, stream.pending_count);
                    stream.pending_count = 0;
                }

                // Otherwise we continue so that we can still batch our command contexts.
            }

            if (async_compute_stream.pending_count > 0) {
                gfx::cmd::return_and_execute(async_compute_stream.contexts_to_submit, async_compute_stream.pending_count);
            }

            // We cannot have any pending submissions at this point
            if (gfx_stream.pending_count != 0) {
                gfx::cmd::return_and_execute(gfx_stream.contexts_to_submit, gfx_stream.pending_count);
                gfx_stream.pending_count = 0;
            }
        }
    }
}

