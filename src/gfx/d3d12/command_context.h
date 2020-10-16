#pragma once
#include "pch.h"
#include "core/array.h"
#include "core/ring_buffer.h"
#include "gfx/public_resources.h"
#include "gfx/gfx.h"
#include "wrappers.h"

namespace zec::dx12
{
    struct CommandContextPool
    {
        ID3D12Device* device = nullptr; // Non-owning
        ID3D12CommandQueue* queue = nullptr; // Non-owning
        CommandQueueType type = CommandQueueType::GRAPHICS;
        Fence fence; // Don't destroy this (it'll get destroyed by Fence list destruction at the end)
        u64 last_used_fence_value = 0;

        Array<ID3D12GraphicsCommandList*> cmd_lists = {};
        Array<ID3D12CommandAllocator*> cmd_allocators = {};

        Array<u8> free_cmd_list_indices = {};
        Array<u16> free_allocator_indices = {};
        RingBuffer<u16> in_flight_allocator_indices = {};
        RingBuffer<u64> in_flight_fence_values = {};
    };

    namespace CommandContextUtils
    {
        void destroy(CommandContextPool& pool);

        void initialize_pools();
        void destroy_pools();

        CommandContextHandle provision(CommandContextPool& pool);
        void return_and_execute(const CommandContextHandle context_handles[], const size_t num_contexts);
    }
}