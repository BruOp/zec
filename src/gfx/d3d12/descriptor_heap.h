#pragma once
#include "pch.h"
#include "gfx/constants.h"
#include "core/array.h"

namespace zec
{
    namespace dx12
    {
        struct PersistentDescriptorAlloc
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handles[RENDER_LATENCY] = { };
            u32 idx = UINT32_MAX;
        };

        struct DescriptorHeapDesc
        {
            u32 num_persistent = 0;
            u32 num_temporary = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            boolean is_shader_visible = false;
        };

        struct DescriptorHeap
        {
            ID3D12DescriptorHeap* heaps[RENDER_LATENCY] = {  };
            u32 num_persistent = 0;
            u32 num_allocated_persistent = 0;
            Array<u32> dead_list = {};

            u32 num_temporary = 0;
            volatile i64 num_allocated_temporary = 0;

            u32 heap_idx = 0;
            u32 num_heaps = 0;
            u32 descriptor_size = 0;
            boolean is_shader_visible = false;
            D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_start[RENDER_LATENCY] = { };
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_start[RENDER_LATENCY] = { };
        };

        void init(DescriptorHeap& descriptor_heap, const DescriptorHeapDesc& desc);
        void destroy(DescriptorHeap& descriptor_heap);

        PersistentDescriptorAlloc allocate_persistent_descriptor(DescriptorHeap& descriptor_heap);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const u32 alloc_indx);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
        void free_persistent_alloc(DescriptorHeap& descriptor_heap, const D3D12_GPU_DESCRIPTOR_HANDLE& handle);
    }
}