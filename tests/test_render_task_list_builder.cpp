#include "catch2/catch.hpp"
#include "gfx/render_task_system.h"
#include <iostream>

using namespace zec;

namespace test_resources
{
    constexpr u32 NOT_REGISTERED = 0;
    constexpr u32 BACKBUFFER = 1;
    constexpr u32 DEPTH = 2;
    constexpr u32 HDR = 3;
}

constexpr static TexturePassResourceDesc registered_textures[] = {
    {
        .identifier = test_resources::DEPTH,
        .sizing = Sizing::RELATIVE_TO_SWAP_CHAIN,
        .relative_width_factor = 1.0f,
        .relative_height_factor = 1.0f,
        .desc = {
            .num_mips = 1,
            .format = BufferFormat::D32,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL | RESOURCE_USAGE_SHADER_READABLE,
            .initial_state = RESOURCE_USAGE_DEPTH_STENCIL,
            .clear_depth = 1.0f,
            .clear_stencil = 0,
        },
    },
    {
        .identifier = test_resources::BACKBUFFER,
        .sizing = Sizing::RELATIVE_TO_SWAP_CHAIN,
        .relative_width_factor = 1.0f,
        .relative_height_factor = 1.0f,
        .desc = {
            .num_mips = 1,
            .format = BufferFormat::R8G8B8A8_UNORM_SRGB,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .initial_state = RESOURCE_USAGE_RENDER_TARGET,
            .clear_color = { 0.0f, 0.0f, 0.0f },
        },
    },
};

static RenderPassResourceUsage test_inputs[] = {
    {
        .identifier = test_resources::DEPTH,
        .type = PassResourceType::TEXTURE,
        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
        .name = "depth",
    },
};

TEST_CASE("RenderTaskListBuilder will return FAILURE when pass uses unregistered resource as output")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage outputs[] = { {
            .identifier = test_resources::NOT_REGISTERED,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = "unregistered lol",
    } };

    RenderPassTaskDesc test_pass{
        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { test_inputs },
        .outputs = { outputs },
    };
    RenderPassTaskDesc descs[] = { test_pass };

    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
                .backbuffer_resource_id = test_resources::BACKBUFFER,
                .render_pass_task_descs = descs,
        }, errors);
    REQUIRE(!is_valid(handle));
}

TEST_CASE("The backbuffer does not need to be registered first")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage outputs[] = { {
            .identifier = test_resources::BACKBUFFER,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = "backbuffer",
    } };

    RenderPassTaskDesc test_pass{
        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { },
        .outputs = { outputs },
    };
    RenderPassTaskDesc descs[] = { test_pass };

    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
        .backbuffer_resource_id = test_resources::BACKBUFFER,
        .render_pass_task_descs = descs,
        }, errors);

    REQUIRE(is_valid(handle));
}

TEST_CASE("RenderTaskListBuilder will return FAILURE when pass uses unregistered resource as input")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage outputs[] = { {
            .identifier = test_resources::BACKBUFFER,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = "backbuffer",
    } };

    RenderPassResourceUsage inputs[] = { {
            .identifier = test_resources::NOT_REGISTERED,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_DEPTH_STENCIL,
            .name = "depth",
    } };
    RenderPassTaskDesc descs[] = { {
        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { inputs },
        .outputs = outputs,
    } };

    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
            .backbuffer_resource_id = test_resources::BACKBUFFER,
            .render_pass_task_descs = descs,
        }, errors);

    REQUIRE(!is_valid(handle));
}

TEST_CASE("RenderTaskListBuilder will return FAILURE when pass input is not output by another pass")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage inputs[] = { {
        .identifier = test_resources::DEPTH,
        .type = PassResourceType::TEXTURE,
        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
        .name = "depth",
    } };
    RenderPassResourceUsage outputs[] = { {
        .identifier = test_resources::DEPTH,
        .type = PassResourceType::TEXTURE,
        .usage = RESOURCE_USAGE_RENDER_TARGET,
        .name = "backbuffer",
    } };
    RenderPassTaskDesc descs[] = {
        {
            .name = "Test Pass",
            .command_queue_type = CommandQueueType::GRAPHICS,
            .execute_fn = nullptr,
            .resource_layout_descs = {},
            .pipeline_descs = {},
            .settings = { },
            .inputs = { test_inputs },
            .outputs = { outputs },
        }
    };
    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
            .backbuffer_resource_id = test_resources::NOT_REGISTERED,
            .render_pass_task_descs = descs,
        }, errors);

    REQUIRE(!is_valid(handle));
}

TEST_CASE("RenderTaskListBuilder will return FAILURE when resource is used as both an input and an output in same pass")
{
    RenderTaskSystem render_task_system{};
    RenderPassTaskDesc descs[] = { {
        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { test_inputs },
        .outputs = { test_inputs },
    } };
    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
            .backbuffer_resource_id = test_resources::NOT_REGISTERED,
            .render_pass_task_descs = descs,
        }, errors);

    REQUIRE(!is_valid(handle));
}

TEST_CASE("RenderTaskListBuilder will return SUCCESS when task list is valid")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage depth_outputs[] = { {
        .identifier = test_resources::DEPTH,
        .type = PassResourceType::TEXTURE,
        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
        .name = "depth",
    } };

    RenderPassResourceUsage outputs[] = { {
            .identifier = test_resources::BACKBUFFER,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = "backbuffer",
    } };

    RenderPassTaskDesc descs[] = {
        {
            .name = "Depth Pass",
            .command_queue_type = CommandQueueType::GRAPHICS,
            .execute_fn = nullptr,
            .resource_layout_descs = {},
            .pipeline_descs = {},
            .settings = { },
            .inputs = { },
            .outputs = { depth_outputs },
        },
        {

        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { test_inputs },
        .outputs = { outputs },
    }
    };

    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
            .backbuffer_resource_id = test_resources::BACKBUFFER,
            .render_pass_task_descs = descs,
        }, errors);

    for (size_t i = 0; i < errors.size(); i++) {
        std::cout << errors[i] << std::endl;
    }
    REQUIRE(is_valid(handle));
}

TEST_CASE("The RenderTaskListBuilder will produce the expected outputs")
{
    RenderTaskSystem render_task_system{};
    render_task_system.register_texture_resources(registered_textures, std::size(registered_textures));

    RenderPassResourceUsage depth_outputs[] = { {
        .identifier = test_resources::DEPTH,
        .type = PassResourceType::TEXTURE,
        .usage = RESOURCE_USAGE_DEPTH_STENCIL,
        .name = "depth",
    } };

    RenderPassResourceUsage inputs[] = {
        {
            .identifier = test_resources::DEPTH,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_SHADER_READABLE,
            .name = "depth"
        }
    };

    RenderPassResourceUsage outputs[] = { {
            .identifier = test_resources::BACKBUFFER,
            .type = PassResourceType::TEXTURE,
            .usage = RESOURCE_USAGE_RENDER_TARGET,
            .name = "backbuffer",
    } };

    RenderPassTaskDesc descs[] = {
        {
            .name = "Depth Pass",
            .command_queue_type = CommandQueueType::GRAPHICS,
            .execute_fn = nullptr,
            .resource_layout_descs = {},
            .pipeline_descs = {},
            .settings = { },
            .inputs = { },
            .outputs = { depth_outputs },
        },
        {

        .name = "Test Pass",
        .command_queue_type = CommandQueueType::GRAPHICS,
        .execute_fn = nullptr,
        .resource_layout_descs = {},
        .pipeline_descs = {},
        .settings = { },
        .inputs = { inputs },
        .outputs = { outputs },
    }
    };

    std::vector<std::string> errors{};
    RenderTaskListHandle handle = render_task_system.create_render_task_list(RenderTaskListDesc{
            .backbuffer_resource_id = test_resources::BACKBUFFER,
            .render_pass_task_descs = descs,
        }, errors);

    SECTION("Resource transitions list is populated")
    {
        REQUIRE(is_valid(handle));
        const auto& transitions = render_task_system.render_task_lists[handle.idx].resource_transition_descs;
        REQUIRE(transitions.size() == 3);
        REQUIRE(transitions[0].resource_id == test_resources::DEPTH);
        REQUIRE(transitions[0].type == ResourceTransitionType::TEXTURE);
        REQUIRE(transitions[0].usage == RESOURCE_USAGE_DEPTH_STENCIL);

        REQUIRE(transitions[1].resource_id == test_resources::DEPTH);
        REQUIRE(transitions[1].type == ResourceTransitionType::TEXTURE);
        REQUIRE(transitions[1].usage == RESOURCE_USAGE_SHADER_READABLE);

        REQUIRE(transitions[2].resource_id == test_resources::BACKBUFFER);
        REQUIRE(transitions[2].type == ResourceTransitionType::TEXTURE);
        REQUIRE(transitions[2].usage == RESOURCE_USAGE_RENDER_TARGET);

        const auto& render_tasks = render_task_system.render_task_lists[handle.idx].render_pass_tasks;
        REQUIRE(render_tasks[0].resource_transitions_view.offset == 0);
        REQUIRE(render_tasks[0].resource_transitions_view.size == 1);

        REQUIRE(render_tasks[1].resource_transitions_view.offset == 1);
        REQUIRE(render_tasks[1].resource_transitions_view.size == 2);
    }

    SECTION("Passes which require a flush will be marked")
    {
        const auto& render_tasks = render_task_system.render_task_lists[handle.idx].render_pass_tasks;
        REQUIRE(render_tasks[0].requires_flush == true);

        REQUIRE(render_tasks[1].requires_flush == false);
    }
    /*SECTION("Passes which require a GPU wait due to cross-queue dependencies will be marked")
    {
        const auto& render_tasks = render_task_system.render_task_lists[handle.idx].render_tasks;
        REQUIRE(render_tasks[0].receipt_idx_to_wait_on == UINT32_MAX);
        REQUIRE(render_tasks[1].receipt_idx_to_wait_on == UINT32_MAX);
    }*/
}
