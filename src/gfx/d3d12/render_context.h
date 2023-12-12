#pragma once
#include "gfx/rhi_public_resources.h"
#include "gfx/constants.h"
#include "dx_helpers.h"
#include "wrappers.h"
#include "descriptor_heap.h"
#include "buffers.h"
#include "samplers.h"
#include "textures.h"
#include "upload_manager.h"
#include "command_context.h"
#include "resource_destruction.h"
#include "shader_blob_manager.h"

namespace D3D12MA
{
    class Allocator;
}

namespace zec::rhi
{
    namespace dx12
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
    }

    // Defined in the public namespace since we had to forward declare it in rhi.h
    class RenderContext
    {
        typedef dx12::DXPtrArray<ID3D12RootSignature, ResourceLayoutHandle> ResourceLayoutStore;
        typedef dx12::DXPtrArray<ID3D12PipelineState, PipelineStateHandle> PipelineStateStore;
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

        dx12::SwapChain swap_chain = {};

        dx12::PerQueueArray<dx12::CommandQueue> command_queues;
        dx12::PerQueueArray<dx12::CommandContextPool> command_pools = {};

        dx12::UploadContextStore upload_store;
        dx12::ResourceDestructionQueue destruction_queue = {};
        dx12::AsyncResourceDestructionQueue async_destruction_queue = {};

        dx12::DescriptorHeapManager descriptor_heap_manager = {};

        dx12::Fence frame_fence = { };

        ResourceLayoutStore root_signatures = {};
        dx12::ShaderBlobsManager shader_blob_manager = {};
        PipelineStateStore pipelines = {};

        dx12::BufferList buffers = {};
        dx12::TextureList textures = {};
        dx12::SamplerStore samplers = {};
    };

    namespace dx12
    {
        inline ID3D12GraphicsCommandList* get_command_list(const RenderContext& render_context, const CommandContextHandle& handle)
        {
            CommandQueueType type = cmd_utils::get_command_queue_type(handle);
            const CommandContextPool& command_pool = render_context.command_pools[type];
            return command_pool.get_graphics_command_list(handle);
        };
    }
}
