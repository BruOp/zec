#include "prepare_debug_data_pass.h"

namespace clustered::prepare_debug_data_pass
{
	using namespace zec;
	using namespace zec::render_graph;

	namespace
	{
		enum struct BindingSlots : u32
		{
			CLEAR_BUFFER_RC = 0,
		};

		static constexpr PassResourceUsage outputs[] = {
			{
				.identifier = pass_resources::DEBUG_LINES_POSITIONS,
				.type = PassResourceType::BUFFER,
				.usage = RESOURCE_USAGE_COMPUTE_WRITABLE,
			},
		};

		struct ClearBufferConstantData
		{
			u32 buffer_idx;
			u32 buffer_byte_size;
			u32 clear_value;
		};

		//struct PrepareDebugDataPassData
		//{
		//	u32 clear_buffer_cb;
		//};
		//MAKE_IDENTIFIER(PREPARE_DEBUG_DATA_CLEAR_BUFFER_CB);
	}

	static void prepare_debug_data_pass_setup(const SettingsStore* settings_context, PerPassDataStore* per_pass_data_store)
	{
		//BufferHandle clear_buffer_cb = gfx::buffers::create({
		//	.usage = RESOURCE_USAGE_CONSTANT,
		//	.type = BufferType::DEFAULT,
		//	.byte_size = sizeof(ClearBufferConstantData),
		//	.stride = 0 });
		//gfx::set_debug_name(clear_buffer_cb, L"PrepareDebugData ClearBufferCB");
		//per_pass_data_store->emplace(PREPARE_DEBUG_DATA_CLEAR_BUFFER_CB, )
	}

	static void prepare_debug_data_pass_execution(const PassExecutionContext* context)
	{
		const CommandContextHandle cmd_ctx = context->cmd_context;
		const ResourceContext& resource_context = *context->resource_context;
		const PipelineStore& pipeline_context = *context->pipeline_context;
		const SettingsStore& settings_context = *context->settings_context;
		const PerPassDataStore& per_pass_data_store = *context->per_pass_data_store;

		const ResourceLayoutHandle resource_layout = pipeline_context.get_resource_layout(CLEAR_BUFFER_RESOURCE_LAYOUT);
		const PipelineStateHandle pso = pipeline_context.get_pipeline(CLEAR_BUFFER_PIPELINE);
		const BufferHandle debug_line_positions = resource_context.get_buffer(pass_resources::DEBUG_LINES_POSITIONS);
		ClearBufferConstantData constants = {
			.buffer_idx = gfx::buffers::get_shader_writable_index(debug_line_positions),
			.buffer_byte_size = pass_resource_descs::DEBUG_LINES_POSITIONS.desc.byte_size,
			.clear_value = 0u
		};

		gfx::cmd::set_compute_resource_layout(cmd_ctx, resource_layout);
		gfx::cmd::set_compute_pipeline_state(cmd_ctx, pso);
		gfx::cmd::bind_compute_constants(cmd_ctx, &constants, sizeof(constants) / sizeof(u32), BindingSlots::CLEAR_BUFFER_RC);
		gfx::cmd::dispatch(cmd_ctx, (constants.buffer_byte_size / 4u + 31u) / 32u, 1, 1);

		RenderableScene* renderable_scene = settings_context->get(settings::RENDERABLE_SCENE_PTR);
		// TODO: This should actually be a per-frame buffer?
		SceneConstantData& scene_constant_data = renderable_scene->scene_constants;
		scene_constant_data.scene_debug_data.debug_lines_buffer_idx = gfx::buffers::get_shader_writable_index(debug_line_positions);
	}

	extern const zec::render_graph::PassDesc pass_desc{
		.name = "Prepare Debug Data Pass",
		.command_queue_type = CommandQueueType::GRAPHICS, // We'll need it to block I guess
		.setup_fn = &prepare_debug_data_pass_setup,
		.execute_fn = &prepare_debug_data_pass_execution,
		//.inputs = inputs,
		.outputs = outputs,
	};
}