#pragma once

#include "core/zec_types.h"
#include "../rhi_public_resources.h"
#include "../resource_array.h"
#include "dx_helpers.h"
#include "descriptor_heap.h"

namespace zec::rhi::dx12
{

    class SamplerStore
    {
    public:
        SamplerStore() = default;
        ~SamplerStore()
        {
            ASSERT(descs.size == 0);
            ASSERT(descriptor_handles.size == 0);
        }

        void destroy(DescriptorHeapManager* descriptor_heap_manager);

        SamplerHandle push_back(const SamplerDesc& sampler_desc, DescriptorRangeHandle drh);
        const SamplerDesc& get_sampler_desc(size_t index) const;
        const DescriptorRangeHandle& get_descriptor_range_handle(size_t index) const;
    private:
        ResourceArray<SamplerDesc, SamplerHandle> descs{};
        // A lot of wasted space here, FWIW
        ResourceArray<DescriptorRangeHandle, SamplerHandle> descriptor_handles{};
    };
}