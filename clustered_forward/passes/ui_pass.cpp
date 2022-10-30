#include "ui_pass.h"
#include "../pass_resources.h"
#include <gfx/ui.h>

namespace clustered::ui_pass
{
    using namespace zec;
    using namespace zec::render_graph;

    static constexpr PassResourceUsage outputs[] = {
        {
            .identifier = pass_resources::SDR_TARGET.identifier,
            .type = PassResourceType::TEXTURE,
            .usage = zec::RESOURCE_USAGE_RENDER_TARGET,
        },
    };

    static void ui_pass_exectution(const PassExecutionContext* context)
    {
        const CommandContextHandle cmd_ctx = context->cmd_context;
        ui::draw_frame(cmd_ctx);
    }

    extern const render_graph::PassDesc pass_desc = {
        .name = "UI Pass",
        .command_queue_type = zec::CommandQueueType::GRAPHICS,
        .setup_fn = nullptr,
        .execute_fn = &ui_pass_exectution,
        .inputs = {},
        .outputs = outputs,
    };
}