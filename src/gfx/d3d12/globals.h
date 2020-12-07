#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "gfx/constants.h"
#include "dx_helpers.h"
#include "wrappers.h"
#include "descriptor_heap.h"
#include "buffers.h"
#include "meshes.h"
#include "resource_managers.h"
#include "upload_manager.h"
#include "resource_destruction.h"
#include "command_context.h"

namespace zec::gfx::dx12
{
    extern IDXGIFactory4* g_factory;
    extern IDXGIAdapter1* g_adapter;
    extern ID3D12Device* g_device;
    extern D3D_FEATURE_LEVEL g_supported_feature_level;

    extern D3D12MA::Allocator* g_allocator;


    extern ID3D12CommandQueue* g_gfx_queue;
    extern ID3D12CommandQueue* g_compute_queue;
    extern ID3D12CommandQueue* g_copy_queue;

    extern CommandContextPool g_command_pools[size_t(CommandQueueType::NUM_COMMAND_CONTEXT_POOLS)];

    extern ResourceDestructionQueue g_destruction_queue;
    extern UploadManager g_upload_manager;

    extern SwapChain g_swap_chain;

    extern DescriptorHeap g_descriptor_heaps[size_t(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)];

    // Resources
    extern Array<Fence> g_fences;
    extern Fence g_frame_fence;
    extern u64 g_current_frame_idx;
    // Total number of CPU frames completed (means that we've recorded and submitted commands for the frame)
    extern u64 g_current_cpu_frame;
    // Total number of GPU frames completed (completed means that the GPU signals the fence)
    extern u64 g_current_gpu_frame;

    extern DXPtrArray<ID3D12RootSignature, ResourceLayoutHandle> g_root_signatures;
    extern DXPtrArray<ID3D12PipelineState, PipelineStateHandle> g_pipelines;

    extern ResourceList<Buffer, BufferHandle> g_buffers;
    extern TextureList g_textures;
    extern Array<Mesh> g_meshes;

}