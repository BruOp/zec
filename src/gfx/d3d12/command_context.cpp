#include "pch.h"
#include "command_context.h"
#include "gfx/gfx.h"
#include "dx_helpers.h"
#include "dx_utils.h"
#include "globals.h"

namespace zec::dx12::CommandContextUtils
{
    CommandContextHandle encode_command_context_handle(const u16 cmd_allocator_idx, const u16 cmd_list_idx)
    {
        return {
            (u32(cmd_allocator_idx) << 16) & u32(cmd_list_idx)
        };
    }

    u16 get_command_list_idx(const CommandContextPool& pool, const CommandContextHandle context_handle)
    {
        constexpr u32 mask = 0x0000FFFF;
        return u16(context_handle.idx & mask);
    }

    u16 get_command_allocator_idx(const CommandContextPool& pool, const CommandContextHandle context_handle)
    {
        return u16(context_handle.idx >> 16);
    }

    D3D12_COMMAND_LIST_TYPE to_d3d_list_type(CommandContextType type)
    {
        switch (type) {
        case zec::CommandContextType::GRAPHICS:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case zec::CommandContextType::COMPUTE:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case zec::CommandContextType::NUM_COMMAND_CONTEXT_POOLS:
        default:
            throw std::runtime_error("Cannot map NUM_COMMAND_CONTEXT_POOLS to D3D12_COMMAND_LIST_TYPE");
        }
    }

    void destroy(CommandContextPool& pool)
    {
        ASSERT(pool.free_cmd_list_indices.size == 0);
        ASSERT(pool.free_allocator_indices.size == 0);
        ASSERT(pool.in_flight_allocator_indices.size == 0);

        for (auto& cmd_list : pool.cmd_lists) {
            dx_destroy(&cmd_list);
        }
        for (auto& cmd_allocator : pool.cmd_allocators) {
            dx_destroy(&cmd_allocator);
        }
    };

    void initialize_pools()
    {
        constexpr size_t num_pools = size_t(CommandContextType::NUM_COMMAND_CONTEXT_POOLS);
        for (size_t i = 0; i < num_pools; i++) {
            g_command_pools[i].type = CommandContextType(i);
        }
    };

    void destroy_pools()
    {
        constexpr size_t num_pools = size_t(CommandContextType::NUM_COMMAND_CONTEXT_POOLS);
        for (size_t i = 0; i < num_pools; i++) {
            auto& pool = g_command_pools[i];


        }
    };

    CommandContextHandle provision(CommandContextPool& pool)
    {
        ASSERT(pool.device != nullptr);

        u16 allocator_idx = 0;
        ID3D12CommandAllocator* cmd_allocator = nullptr;
        if (pool.free_allocator_indices.size > 0) {
            allocator_idx = pool.free_allocator_indices.pop_back();
            cmd_allocator = pool.cmd_allocators[allocator_idx];
        }
        else {
            ASSERT(pool.cmd_allocators.size < UINT16_MAX);
            DXCall(pool.device->CreateCommandAllocator(
                to_d3d_list_type(pool.type), IID_PPV_ARGS(&cmd_allocator)
            ));
            allocator_idx = u16(pool.cmd_allocators.push_back(cmd_allocator));
        }

        u16 list_idx = 0;
        ID3D12GraphicsCommandList* cmd_list = nullptr;
        if (pool.free_cmd_list_indices.size > 0) {
            list_idx = u16(pool.free_cmd_list_indices.pop_back());
            cmd_list = pool.cmd_lists[list_idx];
            cmd_list->Reset(cmd_allocator, nullptr);
        }
        else {
            DXCall(pool.device->CreateCommandList(
                0, to_d3d_list_type(pool.type), cmd_allocator, nullptr, IID_PPV_ARGS(&cmd_list)
            ));
            list_idx = pool.cmd_lists.push_back(cmd_list);
        }

        auto& heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        cmd_list->SetDescriptorHeaps(1, &heap.heap);

        return encode_command_context_handle(allocator_idx, list_idx);
    }

    void return_and_execute(CommandContextPool& pool, const CommandContextHandle context_handles[], const size_t num_contexts)
    {
        constexpr size_t MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION = 8; // Arbitrary limit 
        ASSERT(num_contexts < MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION);
        ID3D12CommandList* cmd_lists[MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION] = {};
        for (size_t i = 0; i < num_contexts; i++) {
            u16 cmd_list_idx = get_command_list_idx(pool, context_handles[i]);
            u16 cmd_allocator_idx = get_command_allocator_idx(pool, context_handles[i]);


            // Is this being closed here?
            ID3D12GraphicsCommandList* cmd_list = pool.cmd_lists[cmd_list_idx];
            cmd_list->Close();
            cmd_lists[i] = cmd_list;

            pool.free_cmd_list_indices.push_back(cmd_list_idx);
            pool.in_flight_allocator_indices.push_back(cmd_allocator_idx);
            pool.in_flight_fence_values.push_back(pool.current_fence_value);
        }

        pool.queue->ExecuteCommandLists(num_contexts, cmd_lists);
        // TODO: Make sure the fence value is incremented atomically!
        signal(pool.fence, pool.queue, pool.current_fence_value++);
    }

}

namespace zec::gfx::cmd
{
    void return_to_pool(CommandContextHandle& command_context) { };

    CommandContextHandle provision(CommandContextPool& pool) { };

    void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id) { };

    void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle) { };

    void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx) { };
    void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot) { };

    void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_id) { };
    void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color) { };

    void clear_depth_target(const CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value) { };

    void set_viewports(const CommandContextHandle ctx, const Viewport* viewport, const u32 num_viewports) { };
    void set_scissors(const CommandContextHandle ctx, const Scissor* scissor, const u32 num_scissors) { };
    void set_render_targets(
        const CommandContextHandle ctx,
        TextureHandle* render_textures,
        const u32 num_render_targets,
        const TextureHandle depth_texture = INVALID_HANDLE)
    { };
}
