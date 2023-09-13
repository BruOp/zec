#pragma once
#include <d3d12.h>

#include "core/array.h"
#include "core/ring_buffer.h"
#include "gfx/rhi_public_resources.h"
#include "gfx/rhi.h"
#include "wrappers.h"

namespace zec::rhi::dx12
{
    template<typename IndexType, size_t capacity>
    struct AsyncFreeList
    {
        struct Node
        {
            IndexType idx;
            u64 fence_value;
        };

        void push(IndexType index, u64 fence_value)
        {
            in_flight.push_back({ .idx = index, .fence_value = fence_value });
        }

        IndexType get_free_index()
        {
            if (free_indices.size() > 0) {
                return free_indices.pop_front();
            }
            else {
                return INVALID_FREE_INDEX;
            }
        }

        void process_in_flight(u64 fence_value)
        {
            while (in_flight.size() > 0 && in_flight.front().fence_value <= fence_value) {
                free_indices.push_back(in_flight.pop_front().idx);
            }
        }

        static constexpr IndexType INVALID_FREE_INDEX = capacity;
        FixedRingBuffer<IndexType, capacity> free_indices = {};
        FixedRingBuffer<Node, capacity> in_flight = {};
    };

    class CommandContextPool
    {
    public:
        // Non owning
        ID3D12Device* device;
        // Owning
        CommandQueueType queue_type;
        FixedArray<ID3D12GraphicsCommandList*, 128> cmd_lists = {};
        FixedRingBuffer<u8, 128> free_cmd_list_indices = {};
        FixedArray<ID3D12CommandAllocator*, 128> allocators = {};
        AsyncFreeList<u16, 128> allocators_free_list = {};

        void initialize(CommandQueueType queue_type, ID3D12Device* device);
        void destroy();

        CommandContextHandle provision();

        void return_to_pool(const u64 fence_value, CommandContextHandle& handles);

        void reset(const u64 fence_value);

        ID3D12GraphicsCommandList* get_graphics_command_list(const CommandContextHandle context_handle);
    };

    struct CommandQueue
    {
        CommandQueueType type = CommandQueueType::GRAPHICS;
        ID3D12CommandQueue* queue = nullptr;
        Fence fence = {};
        u64 last_used_fence_value = 0;

        CommandQueue() = default;
        ~CommandQueue();

        UNCOPIABLE(CommandQueue);
        UNMOVABLE(CommandQueue);

        void initialize(CommandQueueType type, ID3D12Device* device);
        void destroy();

        // Returns the fence value used
        u64 execute(ID3D12GraphicsCommandList** command_lists, const size_t num_command_lists)
        {
            queue->ExecuteCommandLists(num_command_lists, reinterpret_cast<ID3D12CommandList**>(command_lists));
            signal(fence, queue, ++last_used_fence_value);
            return last_used_fence_value;
        };

        void flush()
        {
            wait(fence, last_used_fence_value);
        };

        void cpu_wait(u64 fence_value)
        {
            wait(fence, fence_value);
        };

        void insert_wait_on_queue(const CommandQueue& queue_to_wait_on, u64 fence_value)
        {
            queue->Wait(queue_to_wait_on.fence.d3d_fence, fence_value);
        };

        bool check_fence_status(const u64 fence_value)
        {
            return get_completed_value(fence) >= fence_value;
        }
    };


    namespace cmd_utils
    {
        CommandQueueType get_command_queue_type(const CommandContextHandle context_handle);
    }
}