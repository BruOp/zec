#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "gfx/constants.h"
#include "dx_helpers.h"
#include "wrappers.h"
#include "descriptor_heap.h"
#include "buffers.h"
#include "textures.h"
#include "upload_manager.h"
#include "command_context.h"
#include "resource_destruction.h"

namespace D3D12MA
{
    class Allocator;
}

namespace zec::gfx::dx12
{

    template<typename T>
    struct PerQueueArray
    {
        static constexpr size_t size = size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS);
        T members[size];

        T* begin() { return members; };
        T* end() { return members + size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS); };

        T& operator[](const size_t idx) { return members[idx]; };
        const T& operator[](const size_t idx) const { return members[idx]; };
        T& operator[](const CommandQueueType queue_type) { return members[size_t(queue_type)]; };
        const T& operator[](const CommandQueueType queue_type) const { return members[size_t(queue_type)]; };
    };

    class RenderContext
    {
        typedef DXPtrArray<ID3D12RootSignature, ResourceLayoutHandle> ResourceLayoutStore;
        typedef DXPtrArray<ID3D12PipelineState, PipelineStateHandle> PipelineStateStore;
    public:
        RenderConfigState config_state = {};
        u64 current_frame_idx = 0;
        // Total number of CPU frames completed (means that we've recorded and submitted commands for the frame)
        u64 current_cpu_frame = 0;
        // Total number of GPU frames completed (completed means that the GPU signals the fence)
        u64 current_gpu_frame = 0;

        IDXGIFactory4* factory = nullptr;
        IDXGIAdapter1* adapter = nullptr;
        ID3D12Device* device = nullptr;
        D3D_FEATURE_LEVEL supported_feature_level = {};

        D3D12MA::Allocator* allocator = nullptr;

        SwapChain swap_chain = {};

        PerQueueArray<CommandQueue> command_queues;
        PerQueueArray<CommandContextPool> command_pools = {};

        UploadContextStore upload_store;
        ResourceDestructionQueue destruction_queue = {};
        AsyncResourceDestructionQueue async_destruction_queue = {};

        DescriptorHeapManager descriptor_heap_manager = {};

        Fence frame_fence = { };

        ResourceLayoutStore root_signatures = {};
        PipelineStateStore pipelines = {};

        BufferList buffers = {};
        TextureList textures = {};
    };

    inline ID3D12GraphicsCommandList* get_command_list(RenderContext& render_context, const CommandContextHandle& handle)
    {
        CommandQueueType type = cmd_utils::get_command_queue_type(handle);
        CommandContextPool& command_pool = render_context.command_pools[type];
        return command_pool.get_graphics_command_list(handle);
    };

    RenderContext& get_render_context();
}
