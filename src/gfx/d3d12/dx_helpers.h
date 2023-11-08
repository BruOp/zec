#pragma once
#include <d3d12.h>
#include "core/array.h"
#include "gfx/rhi_public_resources.h"

namespace zec::rhi::dx12
{
    template<typename DXPtr>
    inline void dx_destroy(DXPtr** ptr);

    D3D12_SHADER_VISIBILITY to_d3d_visibility(ShaderVisibility visibility);

    D3D12_FILL_MODE to_d3d_fill_mode(const FillMode fill_mode);

    D3D12_CULL_MODE to_d3d_cull_mode(const CullMode cull_mode);

    DXGI_FORMAT to_d3d_format(const BufferFormat format);
    BufferFormat from_d3d_format(const DXGI_FORMAT format);
    DXGI_FORMAT to_srgb_format(const DXGI_FORMAT format);

    D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(const RasterStateDesc& desc);

    D3D12_BLEND to_d3d_blend(const Blend blend);

    D3D12_BLEND_OP to_d3d_blend_op(const BlendOp blend_op);

    D3D12_RENDER_TARGET_BLEND_DESC to_d3d_blend_desc(const RenderTargetBlendDesc& desc);

    D3D12_COMPARISON_FUNC to_d3d_comparison_func(const ComparisonFunc comparison_func);

    D3D12_STENCIL_OP to_d3d_stencil_op(const StencilOp stencil_op);

    D3D12_FILL_MODE getD3DFillMode(const FillMode mode);

    D3D12_DEPTH_STENCILOP_DESC to_d3d_stencil_op_desc(const StencilFuncDesc& func_desc);

    D3D12_DEPTH_STENCIL_DESC to_d3d_depth_stencil_desc(const DepthStencilDesc& depthDesc);

    D3D12_FILTER to_d3d_filter(const SamplerFilterType filter);

    D3D12_TEXTURE_ADDRESS_MODE to_d3d_address_mode(const SamplerWrapMode mode);

    D3D12_RESOURCE_STATES to_d3d_resource_state(const ResourceUsage usage);

    D3D12_PRIMITIVE_TOPOLOGY_TYPE to_d3d_topology_type(const TopologyType type);

    template <typename T, typename ResourceHandle>
    class DXPtrArray
    {
    public:
        DXPtrArray() = default;
        ~DXPtrArray() { ASSERT(ptrs.size == 0); };

        UNCOPIABLE(DXPtrArray);
        UNMOVABLE(DXPtrArray);

        T** begin() { return ptrs.begin(); };
        const T** begin() const { return ptrs.begin(); };

        T** end() { return ptrs.end(); };
        const T** end() const { return ptrs.end(); };

        T* operator[](ResourceHandle handle) const
        {
            return ptrs[handle.idx];
        }

        T*& operator[](ResourceHandle handle)
        {
            return ptrs[handle.idx];
        }

        inline u64 size()
        {
            return ptrs.size;
        }

        inline ResourceHandle push_back(T* ptr)
        {
            ASSERT(ptrs.size < UINT32_MAX);
            return { static_cast<u32>(ptrs.push_back(ptr)) };
        }

        VirtualArray<T*> ptrs;

    };

    template<typename DXPtr>
    void dx_destroy(DXPtr** ptr)
    {
        (*ptr)->Release();
        *ptr = nullptr;
    }
}
