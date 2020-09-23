#pragma once
#include "pch.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include "gfx/public_resources.h"
#include "wrappers.h"
#include "utils/utils.h"

namespace zec
{
    namespace dx12
    {
        class ResourceDestructionQueue
        {
        public:
            ResourceDestructionQueue() = default;
            ~ResourceDestructionQueue()
            {
                ASSERT(queue.size == 0);
                ASSERT(allocations.size == 0);
            }

            UNCOPIABLE(ResourceDestructionQueue);
            UNMOVABLE(ResourceDestructionQueue);

            void queue_for_destruction(IUnknown* resource, D3D12MA::Allocation* allocation = nullptr)
            {
                if (resource == nullptr) {
                    return;
                }
                // So resource is a reference to a ptr of type T that is castable to IUnknown
                queue.push_back(resource);
                allocations.push_back(allocation);
            }

            void process_queue();
            void destroy();
        private:
            Array<IUnknown*> queue = {};
            Array<D3D12MA::Allocation*> allocations;
        };

        class FenceManager
        {
        public:
            FenceManager() = default;
            ~FenceManager()
            {
                ASSERT(fences.size == 0);
            };

            UNCOPIABLE(FenceManager);
            UNMOVABLE(FenceManager);

            void init(ID3D12Device* pDevice, ResourceDestructionQueue* p_destruction_queue);
            void destroy();

            Fence create_fence(const u64 initial_value = 0);
        private:
            // Not owned
            ID3D12Device* device = nullptr;
            ResourceDestructionQueue* destruction_queue = nullptr;
            // Owned
            Array<Fence> fences = {};
        };

        template< typename Resource, typename ResourceHandle>
        class ResourceList
        {
        public:
            ResourceList(ResourceDestructionQueue* destruction_queue) : destruction_queue{ destruction_queue }, resources{ } { };
            ~ResourceList()
            {
                ASSERT(resources.size == 0);
            }

            ResourceHandle create_back()
            {
                const size_t idx = resources.create_back();
                ASSERT(idx < u64(UINT32_MAX));
                return { u32(idx) };
            };

            void destroy()
            {
                for (size_t i = 0; i < resources.size; i++) {
                    destruction_queue->queue_for_destruction(resources[i].resource, resources[i].allocation);
                }
                resources.empty();
            }

            // TODO: Support deleting using a free list or something
            void destroy_resource(const ResourceHandle handle)
            {
                Resource& resource = resources.get(handle.idx);
                destruction_queue->queue_for_destruction(resource.resource, resource.allocation);
                resource = {};
            }

            Resource& get(const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            const Resource& get(const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }
            Resource& operator[](const ResourceHandle handle)
            {
                return resources[handle.idx];
            }
            const Resource& operator[](const ResourceHandle handle) const
            {
                return resources[handle.idx];
            }

            ResourceDestructionQueue* destruction_queue = nullptr;
            Array<Resource> resources;
        };

        class RenderTargetManager
        {
        public:
            RenderTargetManager(ResourceDestructionQueue* destruction_queue) :
                destruction_queue{ destruction_queue },
                render_targets{ NUM_BACK_BUFFERS }
            { }
            ~RenderTargetManager()
            {
                ASSERT(render_targets.size == 0);
            }

            void destroy()
            {
                // We skip the first N=NUM_BACK_BUFFERS entries that are reserved for the backbuffer, they are deleted elsewhere
                for (size_t i = NUM_BACK_BUFFERS; i < render_targets.size; i++) {
                    destruction_queue->queue_for_destruction(render_targets[i].resource, render_targets[i].allocation);
                }
                render_targets.empty();
            }

            RenderTarget& get_backbuffer(const u64 current_frame_index)
            {
                ASSERT(render_targets.size >= NUM_BACK_BUFFERS);
                return render_targets[current_frame_index];
            }

            // We use this to store our backbuffers in the first N entries
            void set_backbuffer(RenderTarget back_buffer, const u64 current_frame_index)
            {
                render_targets[current_frame_index] = back_buffer;
            }

            RenderTarget& operator[](RenderTargetHandle handle)
            {
                return render_targets[handle.idx];
            }
            const RenderTarget& operator[](RenderTargetHandle handle) const
            {
                return render_targets[handle.idx];
            }
        private:
            ResourceDestructionQueue* destruction_queue;
            Array<RenderTarget> render_targets = {};
        };
    }
}