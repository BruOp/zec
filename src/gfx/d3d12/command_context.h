#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "gfx/gfx.h"
#include "wrappers.h"

namespace zec::dx12
{
    struct CommandContext
    {
        ID3D12CommandQueue* queue = nullptr;
        ID3D12GraphicsCommandList* cmd_list = nullptr;
        ID3D12CommandAllocator* cmd_allocator = nullptr;
    };

    struct CommandAllocatorPool
    {

    };

    struct CommandContextPool
    {
        ID3D12Device* device = nullptr; // Non-owning
        ID3D12CommandQueue* queue = nullptr; // Non-owning
        Fence fence; // Don't destroy this (it'll get destroyed by Fence list destruction at the end)
        u64 current_fence_value = 0;

        CommandContextType type = CommandContextType::GRAPHICS;
        Array<ID3D12GraphicsCommandList*> cmd_lists = {};
        Array<ID3D12CommandAllocator*> cmd_allocators = {};

        Array<u16> free_cmd_list_indices = {};
        Array<u16> free_allocator_indices = {};
        Array<u16> in_flight_allocator_indices = {};
        Array<u64> in_flight_fence_values = {};
    };

    namespace CommandContextUtils
    {
        void destroy(CommandContextPool& pool);

        void initialize_pools();
        void destroy_pools();

        CommandContextHandle provision(CommandContextPool& pool);
        void return_to_pool(CommandContextPool& pool, const CommandContext& command_context);

    }
}