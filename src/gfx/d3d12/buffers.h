#pragma once
#include "pch.h"
#include "gfx/public_resources.h"
#include "descriptor_heap.h"

namespace D3D12MA
{
    class Allocation;
}

namespace zec::gfx::dx12
{
    constexpr size_t CONSTANT_BUFFER_ALIGNMENT = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    constexpr size_t VERTEX_BUFFER_ALIGNMENT = 4;
    constexpr size_t INDEX_BUFFER_ALIGNMENT = 4;

    struct Buffer
    {
        ID3D12Resource* resource = nullptr;
        D3D12MA::Allocation* allocation = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE uav = {};
        void* cpu_address = 0;
        u64 gpu_address = 0;
        u64 size = 0;
        u32 stride = 0;
        u32 cpu_accessible = false;
        DescriptorRangeHandle srv = {};
    };

    namespace buffer_utils
    {

    }
}
