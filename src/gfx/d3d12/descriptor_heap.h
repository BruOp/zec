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

        RESOURCE_HANDLE(DescriptorHandle);

        struct DescriptorHeapDesc
        {
            static constexpr u64 MAX_SIZE = MAX_DESCRIPTOR_HEAP_SIZE / 4u;
            u64 size = 0;
            D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        };

        struct DescriptorDestructionElement
        {
            DescriptorHandle handle;
            u32 frame_index;
        };

        // NOTE: Descriptors are immutable, they can only be allocated and destroyed.
        // So don't try to write to the CPU descriptor handle of an existing descriptor
        struct DescriptorHeap
        {
            ID3D12DescriptorHeap* heap;
            size_t num_allocated = 0;
            size_t capacity = 0;
            size_t descriptor_size = 0;
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_start = {};
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_start = {};
            bool is_shader_visible = false;

            Array<DescriptorDestructionElement> destruction_queue;
            Array<DescriptorHandle> dead_list;
        };

        namespace DescriptorUtils
        {
            void init(ID3D12Device* device, DescriptorHeap& heap, const DescriptorHeapDesc& heap_desc);
            void destroy(DescriptorHeap& heap);

            // This takes a heap and a null CPU_DESCRIPTOR_HANDLE and returns an index, but also sets the in_handles ptr value
            // to the appropriate one so it can be used to create views
            DescriptorHandle allocate_descriptor(DescriptorHeap& heap, D3D12_CPU_DESCRIPTOR_HANDLE* in_handle);
            void free_descriptor(DescriptorHeap& heap, const DescriptorHandle descriptor_handle, u64 current_frame_idx);

            void process_destruction_queue(DescriptorHeap& heap, u64 current_frame_idx);

            void init_descriptor_heaps();
            void destroy_descriptor_heaps();

            // Helpers
            D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle);
            D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle);

            DescriptorHandle allocate_descriptor(const D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_CPU_DESCRIPTOR_HANDLE* in_handle); // Use globals
            void free_descriptor(const D3D12_DESCRIPTOR_HEAP_TYPE type, const DescriptorHandle descriptor_handle); // Use globals

        }
    }
}