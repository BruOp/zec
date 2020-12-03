#include "pch.h"
#include "globals.h"

namespace zec::gfx::dx12
{
    IDXGIFactory4* g_factory = nullptr;
    IDXGIAdapter1* g_adapter = nullptr;
    ID3D12Device* g_device = nullptr;
    D3D_FEATURE_LEVEL g_supported_feature_level = {};

    D3D12MA::Allocator* g_allocator = nullptr;

    CommandContextPool g_command_pools[size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS)] = {};

    ID3D12GraphicsCommandList1* g_cmd_list = 0;
    ID3D12CommandAllocator* g_cmd_allocators[NUM_CMD_ALLOCATORS] = {};
    ID3D12CommandQueue* g_gfx_queue = 0;
    ID3D12CommandQueue* g_compute_queue = 0;
    ID3D12CommandQueue* g_copy_queue = 0;

    ResourceDestructionQueue g_destruction_queue = {};
    UploadManager g_upload_manager = { };

    SwapChain g_swap_chain = {};

    DescriptorHeap g_descriptor_heaps[size_t(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)] = {};

    // Resources
    Array<Fence> g_fences = {};
    Fence g_frame_fence = { };
    u64 g_current_frame_idx = 0;
    // Total number of CPU frames completed (means that we've recorded and submitted commands for the frame)
    u64 g_current_cpu_frame = 0;
    // Total number of GPU frames completed (completed means that the GPU signals the fence)
    u64 g_current_gpu_frame = 0;

    DXPtrArray<ID3D12RootSignature, ResourceLayoutHandle> g_root_signatures = {};
    DXPtrArray<ID3D12PipelineState, PipelineStateHandle> g_pipelines = {};

    ResourceList<Buffer, BufferHandle> g_buffers = {};
    TextureList g_textures = {};
    Array<Mesh> g_meshes = {};
}
