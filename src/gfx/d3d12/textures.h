#pragma once
#include "pch.h"
#include "core/array.h"
#include "gfx/public.h"

namespace D3D12MA
{
    class Allocation;
}

namespace zec
{
    namespace dx12
    {
        class TextureStorage
        {
        public:
            TextureStorage();
            ~TextureStorage();

            UNCOPIABLE(TextureStorage);

        private:
            Array<ID3D12Resource*> resource = {};
            Array<D3D12MA::Allocation*> allocations = {};
            Array<u32> srvs = {};
            Array<u32> widths = {};
            Array<u32> heights = {};
            Array<u32> depths = {};
            Array<u32> num_mips = {};
            Array<u32> array_sizes = {};
            Array<DXGI_FORMAT> formats = {};
            Array<u8> are_cubemaps = {};
        };
    }
}