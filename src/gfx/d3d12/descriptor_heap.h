#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/public_resources.h"
#include "gfx/constants.h"

namespace zec
{
    namespace dx12
    {
        constexpr u32 MAX_DESCRIPTOR_HEAP_SIZE = 1000000;
        constexpr u32 MAX_SAMPLER_HEAP_SIZE = 2048;

        struct DescriptorHandle
        {
            DescriptorRangeHandle range_handle;
            u32 idx;
        };

        struct DescriptorHeapDesc
        {
            static constexpr u64 MAX_SIZE = MAX_DESCRIPTOR_HEAP_SIZE / RENDER_LATENCY;
            u64 size = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        };

        struct DescriptorHeapRange
        {
            u32 size = UINT32_MAX;
            u32 capacity = UINT32_MAX;
            u32 offset = UINT32_MAX;
            Array<u32> dead_list = {};
        };

        struct DescriptorHeap
        {
            ID3D12DescriptorHeap* heap;
            size_t num_allocated = 0;
            size_t capacity = 0;
            size_t descriptor_size = 0;
            u32 num_per_frame_ranges = RENDER_LATENCY;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_starts[RENDER_LATENCY] = {};
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_starts[RENDER_LATENCY] = {};

            Array<DescriptorHeapRange> ranges;
            Array<DescriptorHeapRange> dead_list;
            bool is_shader_visible = false;
        };

        namespace DescriptorUtils
        {
            void init(ID3D12Device* device, DescriptorHeap& heap, const DescriptorHeapDesc& heap_desc);
            void destroy(DescriptorHeap& heap);

            DescriptorRangeHandle allocate_range(const DescriptorRangeDesc& range_desc);
            void free_range(const DescriptorRangeHandle& range);

            DescriptorHandle allocate_descriptor(DescriptorHeapRange& range);
            void free_descriptor(const DescriptorHandle descriptor_handle);


            // How do we handle when a texture is destroyed?
            // Currently we maintain a dead list for every heap (since I guess descriptors are of fixed size in the heap)
            // We can do the same, but I'm not sure if we should do this at the range level?
            // I guess the reasoning is that we want to be able to re-use descriptors for new textures as old ones are destroyed?

            void init_descriptor_heaps();
            void destroy_descriptor_heaps();

            // Helpers
            D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index);
            D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type, u32 index);
        }
    }
}