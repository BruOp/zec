#pragma once
#include <stdexcept>
#include <d3d12.h>

#include "core/array.h"
#include "core/ring_buffer.h"
#include "gfx/rhi_public_resources.h"
#include "gfx/constants.h"

namespace zec::rhi::dx12
{
    enum struct HeapType : u8
    {
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        SAMPLER = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        NUM_HEAPS = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES,
    };

    struct DescriptorRangeHandle
    {
        static constexpr u32 invalid_heap_type = 0xFF;
        static constexpr u32 invalid_count = 0x00FFFFFF;
        static constexpr u32 invalid_offset = UINT32_MAX;

        // Composite of 8-bit heap_type, 24-bit count (so most significant 8 bits is heap-type)
        u32 heap_type : 8 = 0xFF;
        u32 count : 24 = 0x00FFFFFF;
        u32 offset = UINT32_MAX;

        inline u32 get_offset() const
        {
            return offset;
        }

        inline u32 get_count() const
        {
            return count;
        }

        inline HeapType get_heap_type() const
        {
            ASSERT(heap_type < u8(HeapType::NUM_HEAPS));
            return HeapType(heap_type);
        }

        static DescriptorRangeHandle encode(HeapType heap_type, const u32 offset, const u32 count)
        {
            ASSERT(count < invalid_count);
            return DescriptorRangeHandle{ .heap_type = u32(heap_type), .count = count, .offset = offset };
        }
    };

    inline bool is_valid(const DescriptorRangeHandle handle)
    {
        return handle.offset != DescriptorRangeHandle::invalid_offset
            && handle.count != DescriptorRangeHandle::invalid_count
            && handle.heap_type != DescriptorRangeHandle::invalid_heap_type;
    }

    // NOTE: Treat DescriptorRanges as immutable, they can only be allocated and destroyed.
    class DescriptorHeap
    {
    public:
        bool is_shader_visible = false;
        const HeapType heap_type;
        ID3D12DescriptorHeap* heap;
        const size_t capacity = 0;
        size_t num_allocated = 0;
        size_t num_free = 0;
        size_t descriptor_size = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start = {};

        VirtualArray<DescriptorRangeHandle> free_list;

        DescriptorHeap(HeapType heap_type, size_t heap_size);
        ~DescriptorHeap();

        void initialize(ID3D12Device* device);
        void destroy();

        DescriptorRangeHandle allocate_descriptors(const size_t count);

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            return { .ptr = cpu_start.ptr + (u64(descriptor.get_offset()) + local_offset) * descriptor_size };
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            return { .ptr = gpu_start.ptr + (u64(descriptor.get_offset()) + local_offset) * descriptor_size };
        }

        inline void free_descriptors(const DescriptorRangeHandle descriptor_range)
        {
            if (is_valid(descriptor_range)) {
                free_list.push_back(descriptor_range);
                num_free += descriptor_range.get_count();
            }
        };
    };

    class DescriptorHeapManager
    {
    public:
        DescriptorHeapManager();

        void initialize(ID3D12Device* device);
        void destroy();

        DescriptorRangeHandle allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc);
        // In this case, num_descs allows you to create UAVs for e.g. each mip of a map
        DescriptorRangeHandle allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_descs, const size_t num_descs);
        DescriptorRangeHandle allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* rtv_desc);
        DescriptorRangeHandle allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* dsv_desc);

        // Use this only if you need to provide an allocation for libs that create the descriptors themselves. e.g. imgui
        DescriptorRangeHandle allocate_descriptors(HeapType heap_type, const size_t count);

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            ASSERT(is_valid(descriptor));
            return get_heap(descriptor).get_cpu_descriptor_handle(descriptor, local_offset);
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(const DescriptorRangeHandle descriptor, const u32 local_offset = 0) const
        {
            ASSERT(is_valid(descriptor));
            return get_heap(descriptor).get_gpu_descriptor_handle(descriptor, local_offset);
        }

        ID3D12DescriptorHeap* get_d3d_heaps(const HeapType type)
        {
            ASSERT(get_heap(type).heap != nullptr);
            return get_heap(type).heap;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_start(const HeapType type)
        {
            return get_heap(type).gpu_start;
        }

        inline void free_descriptors(const size_t current_frame_idx, const DescriptorRangeHandle descriptor)
        {
            if (is_valid(descriptor)) {
                descriptors_to_destroy[current_frame_idx].push_back(descriptor);
            }
        };

        void process_destruction_queue(const size_t current_frame_idx)
        {
            auto& destruction_queue = descriptors_to_destroy[current_frame_idx];
            for (size_t i = 0; i < destruction_queue.size; ++i) {
                get_heap(destruction_queue[i]).free_descriptors(destruction_queue[i]);
            }
            destruction_queue.empty();
        }

    private:
        DescriptorHeap& get_heap(const HeapType type)
        {
            switch (type) {
            case HeapType::CBV_SRV_UAV:
                return srv_heap;
            case HeapType::RTV:
                return rtv_heap;
            case HeapType::DSV:
                return dsv_heap;
            case HeapType::SAMPLER:
                return sampler_heap;
            default:
                throw std::runtime_error("Invalid heap type");
            }
        }
        const DescriptorHeap& get_heap(const HeapType type) const
        {
            switch (type) {
            case HeapType::CBV_SRV_UAV:
                return srv_heap;
            case HeapType::RTV:
                return rtv_heap;
            case HeapType::DSV:
                return dsv_heap;
            case HeapType::SAMPLER:
                return sampler_heap;
            default:
                throw std::runtime_error("Invalid heap type");
            }
        }
        DescriptorHeap& get_heap(const DescriptorRangeHandle handle)
        {
            return get_heap(handle.get_heap_type());
        }
        const DescriptorHeap& get_heap(const DescriptorRangeHandle handle) const
        {
            return get_heap(handle.get_heap_type());
        }

        DescriptorHeap srv_heap;
        DescriptorHeap rtv_heap;
        DescriptorHeap dsv_heap;
        DescriptorHeap sampler_heap;

        VirtualArray<DescriptorRangeHandle> descriptors_to_destroy[RENDER_LATENCY] = {};
    };
}
