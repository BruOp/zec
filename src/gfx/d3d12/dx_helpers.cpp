#include "pch.h"
#include "dx_helpers.h"

namespace zec::gfx::dx12
{
    D3D12_SHADER_VISIBILITY to_d3d_visibility(ShaderVisibility visibility)
    {
        switch (visibility) {
        case zec::ShaderVisibility::PIXEL:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case zec::ShaderVisibility::VERTEX:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case zec::ShaderVisibility::ALL:
        default:
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_FILL_MODE to_d3d_fill_mode(const FillMode fill_mode)
    {
        switch (fill_mode) {
        case FillMode::WIREFRAME:
            return D3D12_FILL_MODE_WIREFRAME;
        case FillMode::SOLID:
        default:
            return D3D12_FILL_MODE_SOLID;
        }
    }

    D3D12_CULL_MODE to_d3d_cull_mode(const CullMode cull_mode)
    {
        switch (cull_mode) {
        case CullMode::FRONT_CW:
        case CullMode::FRONT_CCW:
            return D3D12_CULL_MODE_FRONT;
        case CullMode::BACK_CW:
        case CullMode::BACK_CCW:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
        }
    }

    DXGI_FORMAT to_d3d_format(const BufferFormat format)
    {
        switch (format) {
        case BufferFormat::UNKNOWN:
            return DXGI_FORMAT_UNKNOWN;
        case BufferFormat::D32:
            return DXGI_FORMAT_D32_FLOAT;
        case BufferFormat::UINT16:
            return DXGI_FORMAT_R16_UINT;
        case BufferFormat::UINT32:
            return DXGI_FORMAT_R32_UINT;
        case BufferFormat::UNORM8_2:
            return DXGI_FORMAT_R8G8_UNORM;
        case BufferFormat::UNORM16_2:
            return DXGI_FORMAT_R16G16_UNORM;
        case BufferFormat::HALF_2:
            return DXGI_FORMAT_R16G16_FLOAT;
        case BufferFormat::FLOAT_2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case BufferFormat::FLOAT_3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case BufferFormat::UINT8_4:
            //case BufferFormat::R8G8B8A8_UINT:              
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case BufferFormat::UNORM8_4:
            //case BufferFormat::R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case BufferFormat::UINT16_4:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case BufferFormat::UNORM16_4:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case BufferFormat::HALF_4:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case BufferFormat::FLOAT_4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case BufferFormat::B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case BufferFormat::UNORM8_4_SRGB:
            //case BufferFormat::R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case BufferFormat::UNORM8_BC7:
            return DXGI_FORMAT_BC7_UNORM;
        case BufferFormat::UNORM8_BC7_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        case BufferFormat::INVALID:
        default:
            throw std::runtime_error("Cannot transform invalid format to DXGI format");
        }
    }

    BufferFormat from_d3d_format(const DXGI_FORMAT format)
    {
        switch (format) {
        case DXGI_FORMAT_UNKNOWN:
            return BufferFormat::UNKNOWN;
        case DXGI_FORMAT_D32_FLOAT:
            return BufferFormat::D32;
        case DXGI_FORMAT_R16_UINT:
            return BufferFormat::UINT16;
        case DXGI_FORMAT_R32_UINT:
            return BufferFormat::UINT32;
        case DXGI_FORMAT_R8G8_UNORM:
            return BufferFormat::UNORM8_2;
        case DXGI_FORMAT_R16G16_UNORM:
            return BufferFormat::UNORM16_2;
        case DXGI_FORMAT_R32G32_FLOAT:
            return BufferFormat::FLOAT_2;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return BufferFormat::FLOAT_3;
        case DXGI_FORMAT_R8G8B8A8_UINT:
            return BufferFormat::UINT8_4;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return BufferFormat::UNORM8_4;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return BufferFormat::B8G8R8A8_UNORM;
        case DXGI_FORMAT_R16G16B16A16_UINT:
            return BufferFormat::UINT16_4;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return BufferFormat::UNORM16_4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return BufferFormat::HALF_4;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return BufferFormat::FLOAT_4;

        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return BufferFormat::UNORM8_4_SRGB;
        case DXGI_FORMAT_BC7_UNORM:
            return BufferFormat::UNORM8_BC7;
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return BufferFormat::UNORM8_BC7_SRGB;
        default:
            throw std::runtime_error("Do not know how to transform this format!");
        }
    }

    DXGI_FORMAT to_srgb_format(const DXGI_FORMAT format)
    {
        switch (format) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:
            throw std::runtime_error("No SRGB version exists for this format!");
        }
    }

    D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(const RasterStateDesc& desc)
    {
        BOOL cull_ccw = (desc.cull_mode == CullMode::FRONT_CCW || desc.cull_mode == CullMode::BACK_CCW);
        D3D12_RASTERIZER_DESC rasterizer_desc = { };
        rasterizer_desc.FillMode = to_d3d_fill_mode(desc.fill_mode);
        rasterizer_desc.CullMode = to_d3d_cull_mode(desc.cull_mode);
        rasterizer_desc.FrontCounterClockwise = cull_ccw;
        rasterizer_desc.DepthBias = desc.depth_bias;
        rasterizer_desc.DepthBiasClamp = desc.depth_bias_clamp;
        rasterizer_desc.SlopeScaledDepthBias = desc.slope_scaled_depth_bias;
        rasterizer_desc.DepthClipEnable = desc.flags & RasterizerFlags::DEPTH_CLIP_ENABLED;
        rasterizer_desc.MultisampleEnable = desc.flags & RasterizerFlags::MULTISAMPLE;
        rasterizer_desc.AntialiasedLineEnable = desc.flags & RasterizerFlags::ANTIALIASED_LINES;
        rasterizer_desc.ForcedSampleCount = 0;
        rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        return rasterizer_desc;
    };

    D3D12_BLEND to_d3d_blend(const Blend blend)
    {
        switch (blend) {
        case (Blend::ZERO):
            return D3D12_BLEND_ZERO;
        case (Blend::ONE):
            return D3D12_BLEND_ONE;
        case (Blend::SRC_COLOR):
            return D3D12_BLEND_SRC_COLOR;
        case (Blend::INV_SRC_COLOR):
            return D3D12_BLEND_INV_SRC_COLOR;
        case (Blend::SRC_ALPHA):
            return D3D12_BLEND_SRC_ALPHA;
        case (Blend::INV_SRC_ALPHA):
            return D3D12_BLEND_INV_SRC_ALPHA;
        case (Blend::DEST_ALPHA):
            return D3D12_BLEND_DEST_ALPHA;
        case (Blend::INV_DEST_ALPHA):
            return D3D12_BLEND_INV_DEST_ALPHA;
        case (Blend::DEST_COLOR):
            return D3D12_BLEND_DEST_COLOR;
        case (Blend::INV_DEST_COLOR):
            return D3D12_BLEND_INV_DEST_COLOR;
        case (Blend::SRC_ALPHA_SAT):
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case (Blend::BLEND_FACTOR):
            return D3D12_BLEND_BLEND_FACTOR;
        case (Blend::INV_BLEND_FACTOR):
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case (Blend::SRC1_COLOR):
            return D3D12_BLEND_SRC1_COLOR;
        case (Blend::INV_SRC1_COLOR):
            return D3D12_BLEND_INV_SRC1_COLOR;
        case (Blend::SRC1_ALPHA):
            return D3D12_BLEND_SRC1_ALPHA;
        case (Blend::INV_SRC1_ALPHA):
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            throw std::runtime_error("Cannot convert invalid blend type");
        }
    }

    D3D12_BLEND_OP to_d3d_blend_op(const BlendOp blend_op)
    {
        switch (blend_op) {
        case (BlendOp::ADD):
            return D3D12_BLEND_OP_ADD;
        case (BlendOp::SUBTRACT):
            return D3D12_BLEND_OP_SUBTRACT;
        case (BlendOp::REV_SUBTRACT):
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case (BlendOp::MIN):
            return D3D12_BLEND_OP_MIN;
        case (BlendOp::MAX):
            return D3D12_BLEND_OP_MAX;
        default:
            throw std::runtime_error("Cannot convert invalid blend op");
        }
    }

    D3D12_RENDER_TARGET_BLEND_DESC  to_d3d_blend_desc(const RenderTargetBlendDesc& desc)
    {
        //d3d12_blend_mode
        return  D3D12_RENDER_TARGET_BLEND_DESC{
            desc.blend_mode != BlendMode::OPAQUE, FALSE,
            to_d3d_blend(desc.src_blend), to_d3d_blend(desc.dst_blend), to_d3d_blend_op(desc.blend_op),
            to_d3d_blend(desc.src_blend_alpha), to_d3d_blend(desc.dst_blend_alpha), to_d3d_blend_op(desc.blend_op_alpha),
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };
    }

    D3D12_COMPARISON_FUNC to_d3d_comparison_func(const ComparisonFunc comparison_func)
    {
        switch (comparison_func) {
        case ComparisonFunc::LESS:
            return D3D12_COMPARISON_FUNC_LESS;
        case ComparisonFunc::LESS_OR_EQUAL:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonFunc::EQUAL:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonFunc::GREATER_OR_EQUAL:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonFunc::GREATER:
            return D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonFunc::ALWAYS:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        case ComparisonFunc::NEVER:
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    D3D12_STENCIL_OP to_d3d_stencil_op(const StencilOp stencil_op)
    {
        switch (stencil_op) {
        case StencilOp::KEEP:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::ZERO:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::REPLACE:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::INCR_SAT:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::DECR_SAT:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::INVERT:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::INCR:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::DECR:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_ZERO;
        }
    }

    D3D12_FILL_MODE getD3DFillMode(const FillMode mode)
    {
        switch (mode) {
        case FillMode::WIREFRAME:
            return D3D12_FILL_MODE_WIREFRAME;
        case FillMode::SOLID:
        default:
            return D3D12_FILL_MODE_SOLID;
        }
    }

    D3D12_DEPTH_STENCILOP_DESC to_d3d_stencil_op_desc(const StencilFuncDesc& func_desc)
    {
        return D3D12_DEPTH_STENCILOP_DESC{
            to_d3d_stencil_op(func_desc.on_stencil_fail),
            to_d3d_stencil_op(func_desc.on_stencil_pass_depth_fail),
            to_d3d_stencil_op(func_desc.on_stencil_pass_depth_pass),
            to_d3d_comparison_func(func_desc.comparison_func),
        };
    }

    D3D12_DEPTH_STENCIL_DESC to_d3d_depth_stencil_desc(const DepthStencilDesc& depthDesc)
    {
        D3D12_DEPTH_STENCIL_DESC desc = CD3DX12_DEPTH_STENCIL_DESC{};
        desc.DepthEnable = depthDesc.depth_cull_mode != ComparisonFunc::OFF;
        desc.DepthFunc = to_d3d_comparison_func(depthDesc.depth_cull_mode);

        desc.DepthWriteMask = depthDesc.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.StencilEnable = depthDesc.stencil_enabled;
        desc.StencilReadMask = depthDesc.stencil_read_mask;
        desc.StencilWriteMask = depthDesc.stencil_write_mask;
        desc.FrontFace = to_d3d_stencil_op_desc(depthDesc.stencil_front_face);
        desc.BackFace = to_d3d_stencil_op_desc(depthDesc.stencil_back_face);

        return desc;
    }

    D3D12_FILTER to_d3d_filter(const SamplerFilterType filter)
    {
        switch (filter) {
        case SamplerFilterType::MIN_POINT_MAG_POINT_MIP_POINT:
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        case SamplerFilterType::MIN_POINT_MAG_POINT_MIP_LINEAR:
            return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        case SamplerFilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:
            return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case SamplerFilterType::MIN_POINT_MAG_LINEAR_MIP_LINEAR:
            return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        case SamplerFilterType::MIN_LINEAR_MAG_POINT_MIP_POINT:
            return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        case SamplerFilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:
            return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        case SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_POINT:
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR:
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        case SamplerFilterType::ANISOTROPIC:
        default:
            return D3D12_FILTER_ANISOTROPIC;
        }
    }
    D3D12_TEXTURE_ADDRESS_MODE to_d3d_address_mode(const SamplerWrapMode mode)
    {
        switch (mode) {
        case SamplerWrapMode::WRAP:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerWrapMode::MIRROR:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerWrapMode::CLAMP:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerWrapMode::BORDER:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerWrapMode::MIRROR_ONCE:
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        }
    }
    D3D12_RESOURCE_STATES to_d3d_resource_state(const ResourceUsage usage)
    {
        switch (usage) {
        case RESOURCE_USAGE_VERTEX:
        case RESOURCE_USAGE_CONSTANT:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case RESOURCE_USAGE_INDEX:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case RESOURCE_USAGE_SHADER_READABLE:
            // TODO: This isn't accurate. We may need to use another enum instead of ResourceUsage
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case RESOURCE_USAGE_COMPUTE_WRITABLE:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case RESOURCE_USAGE_RENDER_TARGET:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case RESOURCE_USAGE_DEPTH_STENCIL:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case RESOURCE_USAGE_PRESENT:
            return D3D12_RESOURCE_STATE_PRESENT;
        case RESOURCE_USAGE_DYNAMIC:
            return D3D12_RESOURCE_STATE_COMMON;
        case RESOURCE_USAGE_UNUSED:
        default:
            throw std::runtime_error("Cannot do anything with UNUSED you dullard.");
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE to_d3d_topology_type(const TopologyType type)
    {
        switch (type) {
        case TopologyType::POINT:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case TopologyType::LINE:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case TopologyType::TRIANGLE:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            throw std::runtime_error("Cannot handle this topology type");
        }
    }
}