#include "pch.h"
#include "resource_destruction.h"
#include "dx_helpers.h"

namespace zec
{
    namespace dx12
    {
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
            swap_chain.output->Release();
        }

        void destroy(
            ResourceDestructionQueue& destruction_queue,
            DescriptorHeap& srv_descriptor_heap,
            DescriptorHeap& uav_descriptor_heap,
            DescriptorHeap& rtv_descriptor_heap,
            DescriptorHeap& dsv_descriptor_heap,
            TextureList& texture_list
        )
        {
            for (size_t i = 0; i < texture_list.resources.size; i++) {
                if (texture_list.resources[i]) {
                    queue_destruction(destruction_queue, texture_list.resources[i], texture_list.allocations[i]);
                }
                free_persistent_alloc(srv_descriptor_heap, texture_list.srv_indices[i]);
                free_persistent_alloc(uav_descriptor_heap, texture_list.uavs[i]);
                free_persistent_alloc(rtv_descriptor_heap, texture_list.rtvs[i]);
            }

            for (size_t i = 0; i < texture_list.dsv_infos.size; i++) {
                const auto& dsv_info = texture_list.dsv_infos[i];
                free_persistent_alloc(dsv_descriptor_heap, dsv_info.dsv);
            }

            texture_list.resources.empty();
            texture_list.allocations.empty();
            texture_list.srv_indices.empty();
            texture_list.uavs.empty();
            texture_list.infos.empty();
            texture_list.render_target_infos.empty();
        }

        void destroy_resources(ResourceDestructionQueue& destruction_queue, TextureList& texture_list)
        { }

    }
}