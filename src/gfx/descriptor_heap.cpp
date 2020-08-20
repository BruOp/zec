#include "pch.h"
#include "descriptor_heap.h"
#include "utils/utils.h"
#include "device.h"

namespace zec
{
    void init(DescriptorHeap& descriptor_heap, const DescriptorHeapDesc& desc, const Device& device)
    {
        u32 total_num_descriptors = desc.num_persistent + desc.num_temporary;
        ASSERT(total_num_descriptors > 0);

        descriptor_heap.num_persistent = desc.num_persistent;
        descriptor_heap.num_temporary = desc.num_temporary;
        descriptor_heap.heap_type = desc.heap_type;
        descriptor_heap.is_shader_visible = desc.is_shader_visible;
        // We ignore this settings when using RTV or DSV types
        if (desc.heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || desc.heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
            if (descriptor_heap.is_shader_visible) {
                write_log("Cannot create shader visible descriptor heap for RTV or DSV heaps");
            }
            descriptor_heap.is_shader_visible = false;
        }

        descriptor_heap.num_heaps = descriptor_heap.is_shader_visible ? 2 : 1;
        descriptor_heap.dead_list.grow(descriptor_heap.num_persistent);
        for (u32 i = 0; i < descriptor_heap.num_persistent; i++) {
            descriptor_heap.dead_list[i] = i;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
        heap_desc.NumDescriptors = total_num_descriptors;
        heap_desc.Type = descriptor_heap.heap_type;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (descriptor_heap.is_shader_visible) {
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        for (size_t i = 0; i < descriptor_heap.num_heaps; i++) {
            DXCall(device.device->CreateDescriptorHeap(
                &heap_desc, IID_PPV_ARGS(&descriptor_heap.heaps[i])
            ));
            descriptor_heap.cpu_start[i] = descriptor_heap.heaps[i]->GetCPUDescriptorHandleForHeapStart();
            if (descriptor_heap.is_shader_visible) {
                descriptor_heap.gpu_start[i] = descriptor_heap.heaps[i]->GetGPUDescriptorHandleForHeapStart();
            }
        }

        descriptor_heap.descriptor_size = device.device->GetDescriptorHandleIncrementSize(descriptor_heap.heap_type);
    }

    void destroy(DescriptorHeap& descriptor_heap)
    {
        ASSERT(descriptor_heap.num_allocated_persistant == 0);
        for (size_t i = 0; i < ARRAY_SIZE(descriptor_heap.heaps); i++) {
            descriptor_heap.heaps[i]->Release();
        }
    }
}