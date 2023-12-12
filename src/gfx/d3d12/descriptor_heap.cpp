#include "descriptor_heap.h"
#include "dx_utils.h"


namespace zec::rhi::dx12
{
    D3D12_DESCRIPTOR_HEAP_TYPE to_d3d_heap_type(const HeapType heap_type)
    {
        switch (heap_type)
        {
        case HeapType::READ_WRITE_RESOURCES:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        case HeapType::SAMPLER:
            return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        case HeapType::RENDER_TARGETS:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        case HeapType::DEPTH_TARGETS:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        default:
            ASSERT_FAIL("Encountered heap type that we can't translate to D3D12 type");
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        }
    }

    namespace HeapSizes
    {
        static constexpr size_t CBV_SRV_UAV = 1000000;
        static constexpr size_t RTV = 256;
        static constexpr size_t DSV = 256;
        static constexpr size_t SAMPLER = 256;
    }

    DescriptorHeapManager::DescriptorHeapManager() :
        srv_heap{ HeapType::READ_WRITE_RESOURCES, HeapSizes::CBV_SRV_UAV },
        rtv_heap{ HeapType::RENDER_TARGETS, HeapSizes::RTV },
        dsv_heap{ HeapType::DEPTH_TARGETS, HeapSizes::DSV },
        sampler_heap{ HeapType::SAMPLER, HeapSizes::SAMPLER }
    { }

    void DescriptorHeapManager::initialize(ID3D12Device* device)
    {
        srv_heap.initialize(device);
        rtv_heap.initialize(device);
        dsv_heap.initialize(device);
        sampler_heap.initialize(device);
    }

    void DescriptorHeapManager::destroy()
    {
        for (size_t i = 0; i < RENDER_LATENCY; i++) {
            process_destruction_queue(i);
        }
        srv_heap.destroy();
        rtv_heap.destroy();
        dsv_heap.destroy();
        sampler_heap.destroy();
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv_desc)
    {
        DescriptorHeap& heap = get_heap(HeapType::READ_WRITE_RESOURCES);
        DescriptorRangeHandle descriptor_range = heap.allocate_descriptors(1);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap.get_cpu_descriptor_handle(descriptor_range, 0);
        device->CreateShaderResourceView(resource, srv_desc, cpu_handle);
        return descriptor_range;
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav_descs, const size_t num_descs)
    {
        DescriptorHeap& heap = get_heap(HeapType::READ_WRITE_RESOURCES);
        DescriptorRangeHandle descriptor_range = heap.allocate_descriptors(num_descs);
        for (size_t i = 0; i < num_descs; i++) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap.get_cpu_descriptor_handle(descriptor_range, i);
            device->CreateUnorderedAccessView(resource, nullptr, &uav_descs[i], cpu_handle);
        }
        return descriptor_range;
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* rtv_desc)
    {
        DescriptorHeap& heap = get_heap(HeapType::RENDER_TARGETS);
        DescriptorRangeHandle descriptor_range = heap.allocate_descriptors(1);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap.get_cpu_descriptor_handle(descriptor_range, 0);
        device->CreateRenderTargetView(resource, rtv_desc, cpu_handle);
        return descriptor_range;
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(ID3D12Device* device, ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* dsv_desc)
    {
        DescriptorHeap& heap = get_heap(HeapType::DEPTH_TARGETS);
        DescriptorRangeHandle descriptor_range = heap.allocate_descriptors(1);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap.get_cpu_descriptor_handle(descriptor_range, 0);
        device->CreateDepthStencilView(resource, dsv_desc, cpu_handle);
        return descriptor_range;
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(ID3D12Device* device, const D3D12_SAMPLER_DESC* sampler_desc)
    {
        DescriptorHeap& heap = get_heap(HeapType::SAMPLER);
        DescriptorRangeHandle descriptor_range = heap.allocate_descriptors(1);
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap.get_cpu_descriptor_handle(descriptor_range, 0);
        device->CreateSampler(sampler_desc, cpu_handle);
        return descriptor_range;
    }

    DescriptorRangeHandle DescriptorHeapManager::allocate_descriptors(HeapType heap_type, const size_t count)
    {
        return get_heap(heap_type).allocate_descriptors(count);
    }

    DescriptorHeap::DescriptorHeap(HeapType heap_type, size_t heap_size) :
        is_shader_visible{ heap_type == HeapType::READ_WRITE_RESOURCES || heap_type == HeapType::SAMPLER },
        heap_type{ heap_type },
        heap{ nullptr },
        capacity{ heap_size },
        num_allocated{ 0 },
        num_free{ 0 },
        descriptor_size{ 0 },
        cpu_start{},
        gpu_start{},
        free_list{}
    {
        ASSERT(heap_type != HeapType::NUM_HEAPS);
    }

    DescriptorHeap::~DescriptorHeap()
    {
        ASSERT(num_allocated == num_free);
        ASSERT(heap == nullptr);
    }

    void DescriptorHeap::initialize(ID3D12Device* device)
    {
        ASSERT(heap == nullptr);
        D3D12_DESCRIPTOR_HEAP_DESC d3d_desc{ };
        D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE(heap_type);
        d3d_desc.NumDescriptors = capacity;
        d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        d3d_desc.Type = type;

        if (is_shader_visible) {
            d3d_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        DXCall(device->CreateDescriptorHeap(
            &d3d_desc, IID_PPV_ARGS(&heap)
        ));

        cpu_start = heap->GetCPUDescriptorHandleForHeapStart();
        if (is_shader_visible) {
            gpu_start = heap->GetGPUDescriptorHandleForHeapStart();
        }

        descriptor_size = device->GetDescriptorHandleIncrementSize(type);
    }

    void DescriptorHeap::destroy()
    {
        ASSERT(num_allocated == num_free);
        heap->Release();
        heap = nullptr;
    }

    DescriptorRangeHandle DescriptorHeap::allocate_descriptors(const size_t count)
    {
        // TODO: wrap this all in a mutex lock?
        DescriptorRangeHandle handle{};
        // First check if there is an entry in the dead list that satisfies our requirements:
        for (size_t i = 0; i < free_list.size; ++i) {
            DescriptorRangeHandle free_range = free_list[i];
            if (free_range.get_count() == count) {
                if (free_list.size == 1) {
                    free_list.pop_back();
                }
                else {
                    // Swap with the end, so that we don't have holes
                    free_list[i] = free_list.pop_back();
                }
                handle = free_range;
                num_free -= handle.get_count();
                break;
            }
            else if (free_range.get_count() > count) {
                // Grab a chunk of the oversized free range
                free_list[i] = DescriptorRangeHandle::encode(heap_type, free_range.get_offset() + count, free_range.get_count() - count);
                handle = free_range;
                num_free -= handle.get_count();
                break;
            }
        }

        // Allocate from the end of the heap if the free list didn't yield anything
        if (!is_valid(handle)) {
            ASSERT(num_allocated + count <= capacity);
            handle = DescriptorRangeHandle::encode(heap_type, num_allocated, count);
            // Fill in_handles;
            num_allocated += count;
        }

        return handle;
    };
}