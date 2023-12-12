#include "samplers.h"
#include "descriptor_heap.h"

namespace zec::rhi::dx12
{
    void SamplerStore::destroy(DescriptorHeapManager* descriptor_heap_manager)
    {
        for (size_t i = 0; i < descriptor_handles.get_size(); ++i)
        {
            descriptor_heap_manager->free_descriptors(0, descriptor_handles[i]);
        }
        descs.empty();
        descriptor_handles.empty();
    }

    SamplerHandle SamplerStore::push_back(const SamplerDesc& sampler_desc, DescriptorRangeHandle drh)
    {
        descs.push_back(sampler_desc);
        size_t idx = descriptor_handles.push_back(drh);
        return SamplerHandle{ .idx = static_cast<u32>(idx) };
    }

    const SamplerDesc& SamplerStore::get_sampler_desc(size_t index) const
    {
        return descs[index];
    }

    const DescriptorRangeHandle& SamplerStore::get_descriptor_range_handle(size_t index) const
    {
        return descriptor_handles[index];
    }
}