#pragma once
#include "pch.h"
#include "wrappers.h"
#include "utils/utils.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"

namespace zec
{
    namespace dx12
    {
        class ResourceDestructionQueue
        {
        public:
            ResourceDestructionQueue() = default;

            UNCOPIABLE(ResourceDestructionQueue);
            UNMOVABLE(ResourceDestructionQueue);

            inline void queue_for_destruction(IUnknown* resource)
            {
                if (resource == nullptr) {
                    return;
                }
                // So resource is a reference to a ptr of type T that is castable to IUnknown
                queue.push_back(resource);
                resource = nullptr;
            }

            void process_queue();
            void destroy();
        private:
            Array<IUnknown*> queue = {};
        };

        class FenceManager
        {
        public:
            FenceManager() = default;

            UNCOPIABLE(FenceManager);
            UNMOVABLE(FenceManager);

            void init(ID3D12Device* pDevice, ResourceDestructionQueue* p_destruction_queue);
            void destroy();

            Fence create_fence(const u64 initial_value = 0);
        private:
            ID3D12Device* device = nullptr;
            ResourceDestructionQueue* destruction_queue = nullptr;
            Array<Fence> fences = {};
            Array<IUnknown*> fences_for_destruction = {};
        };

        //template<typename Resource, typename ResourceDesc, typename ResourceHandle>
        //class ResourceManager
        //{
        //public:
        //    ResourceManager() = default;

        //    UNCOPIABLE(ResourceManager);
        //    UNMOVABLE(ResourceManager);

        //    virtual void init(DeviceContext device, ResourceDestructionQueue* destruction_queue) { };
        //    virtual void destroy() { };

        //    ResourceHandle create_resource(const ResourceDesc& desc) { };
        //    Resource& get_resource(const ResourceHandle handle) { };

        //    virtual inline void destroy_resource(const ResourceHandle handle) { };
        //private:
        //    ResourceDestructionQueue* destruction_queue = nullptr;
        //    Array<Resource> resources;
        //};

        class BufferManager
        {
        public:
            BufferManager() = default;

            UNCOPIABLE(BufferManager);
            UNMOVABLE(BufferManager);

            void init(D3D12MA::Allocator* allocator);
            void destroy();


            BufferHandle create_buffer(BufferDesc);

            inline Buffer& get(const BufferHandle handle)
            {
                return buffers[handle.idx];
            };
            inline const Buffer& get(const BufferHandle handle) const
            {
                return buffers[handle.idx];
            };
            inline Buffer& operator[](const BufferHandle handle)
            {
                return buffers[handle.idx];
            };
            inline const Buffer& operator[](const BufferHandle handle) const
            {
                return buffers[handle.idx];
            };

        private:
            D3D12MA::Allocator* allocator;
            Array<Buffer> buffers;
        };
    }
}