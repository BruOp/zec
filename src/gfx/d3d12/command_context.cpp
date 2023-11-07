#include "command_context.h"
#include "dx_helpers.h"
#include "dx_utils.h"
#include "utils/utils.h"

namespace zec::rhi::dx12
{
    constexpr u64 CMD_LIST_IDX_BIT_WIDTH = 8;
    constexpr u64 CMD_ALLOCATOR_IDX_BIT_WIDTH = 16;

    CommandContextHandle encode_command_context_handle(const CommandQueueType type, const u16 cmd_allocator_idx, const u8 cmd_list_idx)
    {
        return {
            (u32(type) << (CMD_LIST_IDX_BIT_WIDTH + CMD_ALLOCATOR_IDX_BIT_WIDTH)) | (u32(cmd_allocator_idx) << CMD_LIST_IDX_BIT_WIDTH) | u32(cmd_list_idx)
        };
    }

    inline u8 get_command_list_idx(const CommandContextHandle context_handle)
    {
        constexpr u32 mask = 0x000000FF;
        return u8(context_handle.idx & mask);
    }

    inline u16 get_command_allocator_idx(const CommandContextHandle context_handle)
    {
        constexpr u32 mask = 0x00FFFF00;
        return u16((context_handle.idx & mask) >> CMD_LIST_IDX_BIT_WIDTH);
    }

    D3D12_COMMAND_LIST_TYPE to_d3d_queue_type(const CommandQueueType type)
    {
        switch (type) {
        case CommandQueueType::GRAPHICS:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case CommandQueueType::ASYNC_COMPUTE:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case CommandQueueType::COPY:
            return D3D12_COMMAND_LIST_TYPE_COPY;
        case CommandQueueType::NUM_COMMAND_CONTEXT_POOLS:
        default:
            throw std::runtime_error("Cannot map NUM_COMMAND_CONTEXT_POOLS to D3D12_COMMAND_LIST_TYPE");
        }
    }

    inline size_t get_command_pool_index(const CommandContextHandle handle)
    {
        return size_t(cmd_utils::get_command_queue_type(handle));
    }

    void CommandContextPool::initialize(CommandQueueType type, ID3D12Device* device)
    {
        this->queue_type = type;
        this->device = device;
    }

    void CommandContextPool::destroy()
    {
        ASSERT(allocators_free_list.in_flight.size() == 0);

        for (auto& cmd_list : cmd_lists) {
            dx_destroy(&cmd_list);
        }
        for (auto& cmd_allocator : allocators) {
            dx_destroy(&cmd_allocator);
        }
        cmd_lists.empty();
        allocators.empty();
    }

    CommandContextHandle CommandContextPool::provision()
    {
        ID3D12CommandAllocator* cmd_allocator = nullptr;
        u16 allocator_idx = allocators_free_list.get_free_index();
        // Get Allocator
        if (allocator_idx == allocators_free_list.INVALID_FREE_INDEX) {
            // No free index

            // Create and push into our list
            ASSERT(allocators.size < allocators.capacity());
            DXCall(device->CreateCommandAllocator(to_d3d_queue_type(queue_type), IID_PPV_ARGS(&cmd_allocator)
            ));
            allocator_idx = u16(allocators.push_back(cmd_allocator));
            cmd_allocator->SetName(make_string(L"Cmd Allocator %u", allocator_idx).c_str());
        }
        else {
            cmd_allocator = allocators[allocator_idx];
            cmd_allocator->Reset();
        }

        // Get Command List
        u8 cmd_list_idx;
        ID3D12GraphicsCommandList* cmd_list;
        if (free_cmd_list_indices.size() == 0) {
            // Create new command list
            ASSERT(cmd_lists.size < UINT8_MAX);
            DXCall(device->CreateCommandList(0, to_d3d_queue_type(queue_type), cmd_allocator, nullptr, IID_PPV_ARGS(&cmd_list)));
            cmd_list_idx = cmd_lists.push_back(cmd_list);
            cmd_list->SetName(make_string(L"Cmd List %u", cmd_list).c_str());
        }
        else {
            cmd_list_idx = free_cmd_list_indices.pop_front();
            cmd_list = cmd_lists[cmd_list_idx];
            cmd_list->Reset(cmd_allocator, nullptr);
        }

        return encode_command_context_handle(queue_type, allocator_idx, cmd_list_idx);
    }

    void CommandContextPool::return_to_pool(const u64 fence_value, CommandContextHandle& handle)
    {
        // Return the allocator
        u32 allocator_idx = get_command_allocator_idx(handle);
        allocators_free_list.push(allocator_idx, fence_value);
        // Return the command list
        u32 list_idx = get_command_list_idx(handle);
        free_cmd_list_indices.push_back(list_idx);

        // To prevent further use of the handles
        handle.idx = k_invalid_handle;
    }

    CommandQueue::~CommandQueue()
    {
        ASSERT_MSG(queue == nullptr, "Please call .destroy() on the Command Queue before destruction!");
        ASSERT_MSG(fence.d3d_fence == nullptr, "Please call .destroy() on the Command Queue before destruction!");
    }

    void CommandQueue::initialize(CommandQueueType type_, ID3D12Device* device)
    {
        ASSERT(fence.d3d_fence == nullptr);
        ASSERT(queue == nullptr);

        type = type_;
        D3D12_COMMAND_QUEUE_DESC queue_desc{
            .Type = to_d3d_queue_type(type),
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        };

        DXCall(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));

        switch (type) {
        case CommandQueueType::GRAPHICS:
            queue->SetName(L"Graphics Queue");
            break;
        case CommandQueueType::ASYNC_COMPUTE:
            queue->SetName(L"Compute Queue");
            break;
        case CommandQueueType::COPY:
            queue->SetName(L"Copy Queue");
            break;
        }

        fence = create_fence(device, 0);
    };

    void CommandQueue::destroy()
    {
        if (queue) {
            queue->Release();
            queue = nullptr;
        }
        if (fence.d3d_fence) {
            fence.d3d_fence->Release();
            fence.d3d_fence = nullptr;
        }
    }

    ID3D12GraphicsCommandList* CommandContextPool::get_graphics_command_list(const CommandContextHandle context_handle) const
    {
        ASSERT(is_valid(context_handle));
        return cmd_lists[get_command_list_idx(context_handle)];
    }

    void CommandContextPool::reset(const u64 fence_value)
    {
        allocators_free_list.process_in_flight(fence_value);
    }
}

namespace zec::rhi::dx12::cmd_utils
{
    CommandQueueType get_command_queue_type(const CommandContextHandle context_handle)
    {
        return CommandQueueType(context_handle.idx >> (CMD_LIST_IDX_BIT_WIDTH + CMD_ALLOCATOR_IDX_BIT_WIDTH));
    }
}