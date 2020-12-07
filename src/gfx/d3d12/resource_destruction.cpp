#include "pch.h"
#include "resource_destruction.h"
#include "dx_helpers.h"

namespace zec::gfx::dx12
{
    // The destruction of different objects is defined here, rather than in their respective utility namespaces,
    // because destruction involves so many different classes.
    // I'd rather keep it this way on the very slim chance that I do end up moving to more explicitly modifying
    // my resource lists on creation.

    void process_destruction_queue(ResourceDestructionQueue& queue)
    {
        for (size_t i = 0; i < queue.resource_ptrs.size; i++) {
            queue.resource_ptrs[i]->Release();
            if (queue.allocations[i] != nullptr) queue.allocations[i]->Release();
        }
        queue.resource_ptrs.empty();
        queue.allocations.empty();
    }

    template<typename ResourceList>
    void destroy(ResourceDestructionQueue& queue, ResourceList& list)
    {
        for (size_t i = 0; i < list.size(); i++) {
            queue_destruction(queue, list.resources[i], list.allocations[i]);
        }
        list.resources.empty();
    }

    void destroy(ResourceDestructionQueue& queue, Array<Fence>& fences)
    {
        for (size_t i = 0; i < fences.size; i++) {
            Fence& fence = fences[i];
            queue_destruction(queue, fence.d3d_fence);
        }
        fences.empty();
    }

    void destroy(
        ResourceDestructionQueue& queue,
        TextureList& texture_list,
        DescriptorHeap& rtv_descriptor_heap,
        SwapChain& swap_chain
    )
    {
        // The backbuffers will be destroyed when we destroy our Texture List
        swap_chain.swap_chain->Release();
        if (swap_chain.output) swap_chain.output->Release();
    }

    void destroy(
        ResourceDestructionQueue& destruction_queue,
        DescriptorHeap* heaps,
        TextureList& texture_list,
        u64 current_frame_idx
    )
    {
        DescriptorHeap& srv_heap = heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        DescriptorHeap& rtv_heap = heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
        for (size_t i = 0; i < texture_list.resources.size; i++) {
            if (texture_list.resources[i]) {
                queue_destruction(destruction_queue, texture_list.resources[i], texture_list.allocations[i]);
            }

            descriptor_utils::free_descriptors(srv_heap, texture_list.srvs[i], current_frame_idx);
            descriptor_utils::free_descriptors(srv_heap, texture_list.uavs[i], current_frame_idx);
            descriptor_utils::free_descriptors(rtv_heap, texture_list.rtvs[i], current_frame_idx);
        }

        DescriptorHeap& dsv_heap = heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
        for (size_t i = 0; i < texture_list.dsv_infos.size; i++) {
            const auto& dsv_info = texture_list.dsv_infos[i];
            descriptor_utils::free_descriptors(dsv_heap, dsv_info.dsv, current_frame_idx);
        }

        texture_list.resources.empty();
        texture_list.allocations.empty();
        texture_list.srvs.empty();
        texture_list.uavs.empty();
        texture_list.rtvs.empty();
        texture_list.infos.empty();
        texture_list.render_target_infos.empty();
    }
}
