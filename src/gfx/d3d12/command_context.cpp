#include "pch.h"
#include "command_context.h"
#include "gfx/gfx.h"
#include "dx_helpers.h"
#include "dx_utils.h"
#include "globals.h"

namespace zec::dx12::CommandContextUtils
{
    constexpr u64 CMD_LIST_IDX_BIT_WIDTH = 8;
    constexpr u64 CMD_ALLOCATOR_IDX_BIT_WIDTH = 16;

    CommandContextHandle encode_command_context_handle(const CommandQueueType type, const u16 cmd_allocator_idx, const u8 cmd_list_idx)
    {
        return {
            (u32(type) << (CMD_LIST_IDX_BIT_WIDTH + CMD_ALLOCATOR_IDX_BIT_WIDTH)) | (u32(cmd_allocator_idx) << CMD_LIST_IDX_BIT_WIDTH) | u32(cmd_list_idx)
        };
    }

    CommandQueueType get_command_queue_type(const CommandContextHandle context_handle)
    {
        return CommandQueueType(context_handle.idx >> (CMD_LIST_IDX_BIT_WIDTH + CMD_ALLOCATOR_IDX_BIT_WIDTH));
    }

    u8 get_command_list_idx(const CommandContextHandle context_handle)
    {
        constexpr u32 mask = 0x000000FF;
        return u8(context_handle.idx & mask);
    }

    u16 get_command_allocator_idx(const CommandContextHandle context_handle)
    {
        constexpr u32 mask = 0x00FFFF00;
        return u16((context_handle.idx & mask) >> CMD_LIST_IDX_BIT_WIDTH);
    }

    CommandContextPool& get_pool(const CommandQueueType type)
    {
        return g_command_pools[size_t(type)];
    }
    CommandContextPool& get_pool(const CommandContextHandle context_handle)
    {
        return g_command_pools[size_t(get_command_queue_type(context_handle))];
    }

    ID3D12GraphicsCommandList* get_command_list(const CommandContextHandle context_handle)
    {
        CommandContextPool& pool = get_pool(context_handle);
        u8 list_idx = get_command_list_idx(context_handle);
        ASSERT(list_idx < pool.cmd_lists.size);
        return pool.cmd_lists[list_idx];
    }

    D3D12_COMMAND_LIST_TYPE to_d3d_list_type(const CommandQueueType type)
    {
        switch (type) {
        case zec::CommandQueueType::GRAPHICS:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case zec::CommandQueueType::COMPUTE:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case zec::CommandQueueType::ASYNC_COMPUTE:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case zec::CommandQueueType::COPY:
            return D3D12_COMMAND_LIST_TYPE_COPY;
        case zec::CommandQueueType::NUM_COMMAND_CONTEXT_POOLS:
        default:
            throw std::runtime_error("Cannot map NUM_COMMAND_CONTEXT_POOLS to D3D12_COMMAND_LIST_TYPE");
        }
    }

    void destroy(CommandContextPool& pool)
    {
        ASSERT(pool.in_flight_allocator_indices.size() == 0);
        ASSERT(pool.in_flight_fence_values.size() == 0);

        wait(pool.fence, pool.last_used_fence_value);

        for (auto& cmd_list : pool.cmd_lists) {
            dx_destroy(&cmd_list);
        }
        for (auto& cmd_allocator : pool.cmd_allocators) {
            dx_destroy(&cmd_allocator);
        }
        // pool.fence is released as part of fence list destruction :)
    };

    void initialize_pools()
    {
        constexpr size_t num_pools = size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS);
        for (size_t i = 0; i < num_pools; i++) {
            g_command_pools[i].type = CommandQueueType(i);
            g_command_pools[i].device = g_device;
            g_command_pools[i].fence = create_fence(g_fences, 0);
        }
        g_command_pools[u64(CommandQueueType::GRAPHICS)].queue = g_gfx_queue;
        g_command_pools[u64(CommandQueueType::COMPUTE)].queue = g_gfx_queue;
        g_command_pools[u64(CommandQueueType::ASYNC_COMPUTE)].queue = g_compute_queue;
        g_command_pools[u64(CommandQueueType::COPY)].queue = g_copy_queue;
    };

    void destroy_pools()
    {
        constexpr size_t num_pools = size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS);
        for (size_t i = 0; i < num_pools; i++) {
            auto& pool = g_command_pools[i];
            destroy(pool);
        }
    }

    void insert_resource_barrier(const CommandContextHandle command_context, D3D12_RESOURCE_BARRIER barriers[], const size_t num_barriers)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(command_context);
        cmd_list->ResourceBarrier(num_barriers, barriers);
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
        DXCall(cmd_allocator->Reset());

        u8 list_idx = 0;
        ID3D12GraphicsCommandList* cmd_list = nullptr;
        if (pool.free_cmd_list_indices.size > 0) {
            list_idx = pool.free_cmd_list_indices.pop_back();
            cmd_list = pool.cmd_lists[list_idx];
            DXCall(cmd_list->Reset(cmd_allocator, nullptr));
        }
        else {
            ASSERT(pool.cmd_lists.size < UINT8_MAX);
            DXCall(pool.device->CreateCommandList(
                0, to_d3d_list_type(pool.type), cmd_allocator, nullptr, IID_PPV_ARGS(&cmd_list)
            ));
            list_idx = pool.cmd_lists.push_back(cmd_list);
        }

        if (pool.type != CommandQueueType::COPY) {
            auto& heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
            cmd_list->SetDescriptorHeaps(1, &heap.heap);
        }

        return encode_command_context_handle(pool.type, allocator_idx, list_idx);
    }

    CmdReceipt return_and_execute(const CommandContextHandle context_handles[], const size_t num_contexts)
    {
        constexpr size_t MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION = 8; // Arbitrary limit 
        ASSERT(num_contexts < MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION&& num_contexts > 0);
        ID3D12CommandList* cmd_lists[MAX_NUM_SIMULTANEOUS_COMMAND_LIST_EXECUTION] = {};
        CommandContextPool& pool = get_pool(context_handles[0]);
        CmdReceipt receipt = {
            .queue_type = pool.type
        };
        // TODO: Make sure the fence value is incremented atomically!
        receipt.fence_value = ++pool.last_used_fence_value;

        for (size_t i = 0; i < num_contexts; i++) {
            // Make sure all contexts are using the same pool/queue
            ASSERT(get_command_queue_type(context_handles[i]) == pool.type);

            u8 cmd_list_idx = get_command_list_idx(context_handles[i]);
            u16 cmd_allocator_idx = get_command_allocator_idx(context_handles[i]);

            // Is this being closed here?
            ID3D12GraphicsCommandList* cmd_list = pool.cmd_lists[cmd_list_idx];
            DXCall(cmd_list->Close());
            cmd_lists[i] = cmd_list;

            pool.free_cmd_list_indices.push_back(cmd_list_idx);
            pool.in_flight_allocator_indices.push_back(cmd_allocator_idx);
            pool.in_flight_fence_values.push_back(pool.last_used_fence_value);
        }

        pool.queue->ExecuteCommandLists(num_contexts, cmd_lists);
        signal(pool.fence, pool.queue, pool.last_used_fence_value);
        return receipt;
    }

    void reset(CommandContextPool& pool)
    {
        u64 completed_fence_value = get_completed_value(pool.fence);
        const u64 num_in_flight = pool.in_flight_fence_values.size();
        ASSERT(num_in_flight == pool.in_flight_allocator_indices.size());
        while (pool.in_flight_allocator_indices.size() > 0) {
            if (pool.in_flight_fence_values.front() <= completed_fence_value) {
                pool.in_flight_fence_values.pop_front();
                u64 allocator_idx = pool.in_flight_allocator_indices.pop_front();
                pool.free_allocator_indices.push_back(u16(allocator_idx));
            }
            else {
                break;
            }
        }
    }
}

namespace zec::gfx::cmd
{
    using namespace dx12;
    using namespace dx12::CommandContextUtils;

    CommandContextHandle provision(const CommandQueueType type)
    {
        CommandContextPool& pool = get_pool(type);
        return CommandContextUtils::provision(pool);
    }

    CmdReceipt return_and_execute(const CommandContextHandle context_handles[], const size_t num_contexts)
    {
        return dx12::CommandContextUtils::return_and_execute(context_handles, num_contexts);
    };

    bool check_status(const CmdReceipt receipt)
    {
        CommandContextPool& pool = get_pool(receipt.queue_type);
        return is_signaled(pool.fence, receipt.fence_value);
    }

    void flush_queue(const CommandQueueType queue)
    {
        CommandContextPool& source_pool = get_pool(queue);
        wait(source_pool.fence, source_pool.last_used_fence_value);
    }

    void cpu_wait(const CmdReceipt receipt)
    {
        CommandContextPool& pool = get_pool(receipt.queue_type);
        wait(pool.fence, receipt.fence_value);
    }

    void gpu_wait(const CommandQueueType queue_to_insert_wait, const CmdReceipt receipt_to_wait_on)
    {
        CommandContextPool& source_pool = get_pool(receipt_to_wait_on.queue_type);
        ID3D12CommandQueue* dest_queue = get_pool(queue_to_insert_wait).queue;
        dest_queue->Wait(source_pool.fence.d3d_fence, receipt_to_wait_on.fence_value);
    }

    void set_active_resource_layout(const CommandContextHandle ctx, const ResourceLayoutHandle resource_layout_id)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        ID3D12RootSignature* root_signature = g_root_signatures[resource_layout_id];

        CommandQueueType queue_type = get_command_queue_type(ctx);
        if (queue_type == CommandQueueType::GRAPHICS) {
            cmd_list->SetGraphicsRootSignature(root_signature);
        }
        else {
            cmd_list->SetComputeRootSignature(root_signature);
        }
    };

    void set_pipeline_state(const CommandContextHandle ctx, const PipelineStateHandle pso_handle)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        ID3D12PipelineState* pso = g_pipelines[pso_handle];
        cmd_list->SetPipelineState(pso);
    };

    void bind_resource_table(const CommandContextHandle ctx, const u32 resource_layout_entry_idx)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);

        const DescriptorHeap& heap = g_descriptor_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap.gpu_start;

        CommandQueueType queue_type = get_command_queue_type(ctx);
        if (queue_type == CommandQueueType::GRAPHICS) {
            cmd_list->SetGraphicsRootDescriptorTable(resource_layout_entry_idx, handle);
        }
        else {
            cmd_list->SetComputeRootDescriptorTable(resource_layout_entry_idx, handle);
        }
    };

    void bind_constant(const CommandContextHandle ctx, const void* data, const u64 num_constants, const u32 binding_slot)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);

        CommandQueueType queue_type = get_command_queue_type(ctx);
        if (queue_type == CommandQueueType::GRAPHICS) {
            cmd_list->SetGraphicsRoot32BitConstants(binding_slot, u32(num_constants), data, 0);
        }
        else {
            cmd_list->SetComputeRoot32BitConstants(binding_slot, u32(num_constants), data, 0);
        }
    }

    void bind_constant_buffer(const CommandContextHandle ctx, const BufferHandle& buffer_handle, u32 binding_slot)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        const Buffer& buffer = g_buffers[buffer_handle];
        auto gpu_address = buffer.cpu_accessible ? buffer.gpu_address + (buffer.size * g_current_frame_idx) : buffer.gpu_address;
        CommandQueueType queue_type = get_command_queue_type(ctx);
        if (queue_type == CommandQueueType::GRAPHICS) {
            cmd_list->SetGraphicsRootConstantBufferView(binding_slot, gpu_address);
        }
        else {
            cmd_list->SetComputeRootConstantBufferView(binding_slot, gpu_address);
        }
    };

    void draw_mesh(const CommandContextHandle ctx, const MeshHandle mesh_id)
    {
        ASSERT(get_command_queue_type(ctx) == CommandQueueType::GRAPHICS);
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);

        const Mesh& mesh = g_meshes[mesh_id.idx];
        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->IASetIndexBuffer(&mesh.index_buffer_view);
        cmd_list->IASetVertexBuffers(0, mesh.num_vertex_buffers, mesh.buffer_views);
        cmd_list->DrawIndexedInstanced(mesh.index_count, 1, 0, 0, 0);
    }

    void dispatch(const CommandContextHandle ctx, const u32 thread_group_count_x, const u32 thread_group_count_y, const u32 thread_group_count_z)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        cmd_list->Dispatch(thread_group_count_x, thread_group_count_y, thread_group_count_z);
    };

    void clear_render_target(const CommandContextHandle ctx, const TextureHandle render_texture, const float* clear_color)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        DescriptorRangeHandle rtv = TextureUtils::get_rtv(g_textures, render_texture);
        auto cpu_handle = DescriptorUtils::get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv);
        cmd_list->ClearRenderTargetView(cpu_handle, clear_color, 0, nullptr);
    };

    void clear_depth_target(const CommandContextHandle ctx, const TextureHandle depth_stencil_buffer, const float depth_value, const u8 stencil_value)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        DescriptorRangeHandle dsv = TextureUtils::get_dsv(g_textures, depth_stencil_buffer);
        auto cpu_handle = DescriptorUtils::get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsv);
        cmd_list->ClearDepthStencilView(cpu_handle, D3D12_CLEAR_FLAG_DEPTH, depth_value, stencil_value, 0, nullptr);

    };

    void set_viewports(const CommandContextHandle ctx, const Viewport* viewports, const u32 num_viewports)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        cmd_list->RSSetViewports(num_viewports, reinterpret_cast<const D3D12_VIEWPORT*>(viewports));
    };

    void set_scissors(const CommandContextHandle ctx, const Scissor* scissors, const u32 num_scissors)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        D3D12_RECT rects[16];
        for (size_t i = 0; i < num_scissors; i++) {
            rects[i].left = LONG(scissors[i].left);
            rects[i].right = LONG(scissors[i].right);
            rects[i].top = LONG(scissors[i].top);
            rects[i].bottom = LONG(scissors[i].bottom);
        }
        cmd_list->RSSetScissorRects(num_scissors, rects);
    };

    void set_render_targets(
        const CommandContextHandle ctx,
        TextureHandle* render_textures,
        const u32 num_render_targets,
        const TextureHandle depth_target)
    {
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv;
        D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
        if (is_valid(depth_target)) {
            DescriptorRangeHandle descriptor_handle = TextureUtils::get_dsv(g_textures, depth_target);
            dsv = DescriptorUtils::get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, descriptor_handle);
            dsv_ptr = &dsv;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[16] = {};
        for (size_t i = 0; i < num_render_targets; i++) {
            DescriptorRangeHandle descriptor_handle = TextureUtils::get_rtv(g_textures, render_textures[i]);
            rtvs[i] = DescriptorUtils::get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, descriptor_handle);
        }
        cmd_list->OMSetRenderTargets(num_render_targets, rtvs, FALSE, dsv_ptr);
    }

    void transition_textures(const CommandContextHandle ctx, TextureTransitionDesc* transition_descs, u64 num_transitions)
    {
        constexpr u64 MAX_NUM_BARRIERS = 16;
        ASSERT(num_transitions <= MAX_NUM_BARRIERS);
        D3D12_RESOURCE_BARRIER barriers[MAX_NUM_BARRIERS];
        for (size_t i = 0; i < num_transitions; i++) {
            D3D12_RESOURCE_BARRIER& barrier = barriers[i];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = get_resource(g_textures, transition_descs[i].texture);
            barrier.Transition.StateBefore = to_d3d_resource_state(transition_descs[i].before);
            barrier.Transition.StateAfter = to_d3d_resource_state(transition_descs[i].after);
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        cmd_list->ResourceBarrier(u32(num_transitions), barriers);
    };

    void transition_resources(const CommandContextHandle ctx, ResourceTransitionDesc* transition_descs, u64 num_transitions)
    {
        constexpr u64 MAX_NUM_BARRIERS = 16;
        ASSERT(num_transitions <= MAX_NUM_BARRIERS);
        D3D12_RESOURCE_BARRIER barriers[MAX_NUM_BARRIERS];
        for (size_t i = 0; i < num_transitions; i++) {
            D3D12_RESOURCE_BARRIER& barrier = barriers[i];
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            ID3D12Resource* resource = nullptr;
            if (transition_descs[i].type == ResourceTransitionType::TEXTURE) {
                resource = get_resource(g_textures, transition_descs[i].texture);
            }
            else {
                resource = g_buffers.get(transition_descs[i].buffer).resource;
            }
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = to_d3d_resource_state(transition_descs[i].before);
            barrier.Transition.StateAfter = to_d3d_resource_state(transition_descs[i].after);
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        ID3D12GraphicsCommandList* cmd_list = get_command_list(ctx);
        cmd_list->ResourceBarrier(u32(num_transitions), barriers);
    }
}
