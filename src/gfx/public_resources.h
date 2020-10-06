#pragma once
#include "pch.h"
#include "constants.h"

namespace zec
{
    constexpr u32 k_invalid_handle = UINT32_MAX;

    // Copied from BGFX and others
#define RESOURCE_HANDLE(_name)      \
	struct _name { u32 idx = k_invalid_handle; }; \
    inline bool is_valid(_name _handle) { return _handle.idx != zec::k_invalid_handle; }; \
    inline bool operator==(_name handle1, _name handle2) { return handle1.idx == handle2.idx; };

#define INVALID_HANDLE \
    { zec::k_invalid_handle }

    // ---------- Handles ----------
    RESOURCE_HANDLE(BufferHandle);
    RESOURCE_HANDLE(TextureHandle);
    RESOURCE_HANDLE(MeshHandle);
    RESOURCE_HANDLE(ResourceLayoutHandle);
    RESOURCE_HANDLE(PipelineStateHandle);

    // ---------- Enums ----------
    enum ResourceUsage : u16
    {
        RESOURCE_USAGE_UNUSED = 0,
        RESOURCE_USAGE_VERTEX = (1 << 0),
        RESOURCE_USAGE_INDEX = (1 << 1),
        RESOURCE_USAGE_CONSTANT = (1 << 2),
        RESOURCE_USAGE_SHADER_READABLE = (1 << 3),
        RESOURCE_USAGE_COMPUTE_WRITABLE = (1 << 4),
        RESOURCE_USAGE_DYNAMIC = (1 << 5),
        RESOURCE_USAGE_RENDER_TARGET = (1 << 6),
        RESOURCE_USAGE_DEPTH_STENCIL = (1 << 7),
    };

    enum MeshAttribute : u16
    {
        MESH_ATTRIBUTE_INVALID = 0,
        MESH_ATTRIBUTE_POSITION = (1 << 0),
        MESH_ATTRIBUTE_NORMAL = (1 << 1),
        MESH_ATTRIBUTE_TEXCOORD = (1 << 2),
        MESH_ATTRIBUTE_COLOR = (1 << 3),
        MESH_ATTRIBUTE_BLENDINDICES = (1 << 4),
        MESH_ATTRIBUTE_BLENDWEIGHTS = (1 << 5)
    };

    enum struct BufferType : u16
    {
        DEFAULT = 0,
        TYPED,
        STRUCTURED,
        RAW
    };

    enum struct BufferFormat : u16
    {
        INVALID = 0,
        UNKOWN,
        D32,
        UINT16,
        UINT32,
        UNORM8_2,
        UNORM16_2,
        FLOAT_2,
        FLOAT_3,
        UINT8_4,
        UNORM8_4,
        UINT16_4,
        UNORM16_4,
        HALF_4,
        FLOAT_4,

        // SRGB values
        UNORM8_4_SRGB,

        // Aliases
        R8G8B8A8_UINT = UINT8_4,
        R8G8B8A8_UNORM = UNORM8_4,
        R16G16B16A16_UINT = UINT16_4,
        R16G16B16A16_UNORM = UNORM16_4,
        R16G16B16A16_FLOAT = HALF_4,
        R32G32B32A32_FLOAT = FLOAT_4,

        R8G8B8A8_UNORM_SRGB = UNORM8_4_SRGB,
        // TODO: STRUCTURED?
    };

    enum struct ShaderVisibility : u8
    {
        PIXEL = 0,
        ALL,
        VERTEX,
    };

    enum struct ResourceLayoutEntryType : u8
    {
        INVALID = 0,
        CONSTANT,
        CONSTANT_BUFFER,
        TABLE
    };

    enum struct ResourceLayoutRangeUsage : u8
    {
        UNUSED = 0,
        READ = 1 << 0,
        WRITE = 1 << 1,
        READ_WRITE = READ & WRITE,
    };

    enum struct ComparisonFunc : u8
    {
        OFF = 0,
        NEVER,
        LESS,
        LESS_OR_EQUAL,
        EQUAL,
        GREATER_OR_EQUAL,
        GREATER,
        ALWAYS,
    };

    enum struct StencilOp : u8
    {
        KEEP = 0,
        ZERO,
        REPLACE,
        INCR_SAT,
        DECR_SAT,
        INVERT,
        INCR,
        DECR
    };

    enum struct FillMode : u8
    {
        WIREFRAME,
        SOLID
    };

    enum struct CullMode : u8
    {
        NONE,
        FRONT_CW,
        FRONT_CCW,
        BACK_CW,
        BACK_CCW
    };

    enum RasterizerFlags : u8
    {
        DEPTH_CLIP_ENABLED = 1 << 0,
        MULTISAMPLE = 1 << 1,
        ANTIALIASED_LINES = 1 << 2,
    };

    enum struct BlendMode : u8
    {
        OPAQUE,
        MASKED,
        BLENDED
    };

    enum struct Blend : u8
    {
        ZERO = 0,
        ONE,
        SRC_COLOR,
        INV_SRC_COLOR,
        SRC_ALPHA,
        INV_SRC_ALPHA,
        DEST_ALPHA,
        INV_DEST_ALPHA,
        DEST_COLOR,
        INV_DEST_COLOR,
        SRC_ALPHA_SAT,
        BLEND_FACTOR,
        INV_BLEND_FACTOR,
        SRC1_COLOR,
        INV_SRC1_COLOR,
        SRC1_ALPHA,
        INV_SRC1_ALPHA,
    };

    enum struct BlendOp : u8
    {
        // Add source 1 and source 2.
        ADD = 0,
        // Subtract source 1 from source 2.
        SUBTRACT,
        // Subtract source 2 from source 1.
        REV_SUBTRACT,
        // Find the minimum of source 1 and source 2.
        MIN,
        // Find the maximum of source 1 and source 2.
        MAX
    };

    enum PipelineStage : u8
    {
        PIPELINE_STAGE_INVALID = 0,
        PIPELINE_STAGE_VERTEX = 1 << 0,
        PIPELINE_STAGE_PIXEL = 1 << 1,
        PIPELINE_STAGE_COMPUTE = 1 << 2
    };

    enum struct SamplerFilterType : u8
    {
        MIN_POINT_MAG_POINT_MIP_POINT,
        MIN_POINT_MAG_POINT_MIP_LINEAR,
        MIN_POINT_MAG_LINEAR_MIP_POINT,
        MIN_POINT_MAG_LINEAR_MIP_LINEAR,
        MIN_LINEAR_MAG_POINT_MIP_POINT,
        MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        MIN_LINEAR_MAG_LINEAR_MIP_POINT,
        MIN_LINEAR_MAG_LINEAR_MIP_LINEAR,
        ANISOTROPIC,
    };

    enum struct SamplerWrapMode : u8
    {
        WRAP,
        MIRROR,
        CLAMP,
        BORDER,
        MIRROR_ONCE
    };

    // ---------- Creation Descriptors ---------- 
    struct RendererDesc
    {
        u32 width = 0;
        u32 height = 0;
        HWND window = {};
        bool fullscreen = false;
        bool vsync = true;
    };

    struct BufferDesc
    {
        u16 usage = RESOURCE_USAGE_UNUSED;
        BufferType type = BufferType::DEFAULT;
        u32 byte_size = 0;
        u32 stride = 0;
        void* data = nullptr;
    };

    struct InputAssemblyDesc
    {
        struct InputElementDesc
        {
            MeshAttribute attribute_type = MESH_ATTRIBUTE_INVALID;
            u32 semantic_index = 0;
            BufferFormat format = BufferFormat::INVALID;
            u32 input_slot = 0;
            u32 aligned_byte_offset = 0;
            // TODO: Support instancing?
            // input_slot_class (used to distinguish between per vertex data and per instance data)
            // instance_data_step_rate (i'm not sure what this is for just yet)
        };

        InputElementDesc elements[MAX_NUM_MESH_VERTEX_BUFFERS];
    };

    struct MeshDesc
    {
        BufferDesc index_buffer_desc;
        BufferDesc vertex_buffer_descs[MAX_NUM_MESH_VERTEX_BUFFERS];
    };

    struct TextureDesc
    {

        void* data = nullptr;
        u32 width = 0;
        u32 height = 0;
        u32 depth = 0;
        u32 num_mips = 0;
        u32 array_size = 0;
        u16 is_cubemap = 0;
        u16 is_3d = 0;
        BufferFormat format;
        u16 usage = 0;
    };

    struct SamplerDesc
    {
        SamplerFilterType filtering = SamplerFilterType::MIN_LINEAR_MAG_LINEAR_MIP_LINEAR;
        SamplerWrapMode wrap_u = SamplerWrapMode::CLAMP;
        SamplerWrapMode wrap_v = SamplerWrapMode::CLAMP;
        SamplerWrapMode wrap_w = SamplerWrapMode::CLAMP;
        u32 binding_slot;
    };

    struct ResourceLayoutRangeDesc
    {
        static constexpr u32 UNBOUNDED_COUNT = 4096;
        ResourceLayoutRangeUsage usage = ResourceLayoutRangeUsage::UNUSED;
        u32 count = 0;
        u32 space = 0;
    };

    struct ResourceTableEntryDesc
    {
        ResourceLayoutRangeDesc ranges[32] = {};
        ShaderVisibility visibility = ShaderVisibility::ALL;
    };

    struct ResourceLayoutConstantsDesc
    {
        ShaderVisibility visibility = ShaderVisibility::ALL;
        u32 num_constants = 1;
    };

    struct ResourceLayoutEntryDesc
    {
        // Table, Constant or Constant Buffer
        ShaderVisibility visibility = ShaderVisibility::ALL;
    };

    struct ResourceLayoutDesc
    {
        static constexpr u64 MAX_ENTRIES = 16;
        ResourceLayoutConstantsDesc constants[MAX_ENTRIES] = {};
        u32 num_constants;
        ResourceLayoutEntryDesc constant_buffers[MAX_ENTRIES] = {};
        u32 num_constant_buffers;
        ResourceTableEntryDesc tables[MAX_ENTRIES] = {};
        u32 num_resource_tables;
        // TODO: Add regular sampler descriptors? Not just static samplers?

        SamplerDesc static_samplers[4] = {};
        u32 num_static_samplers = 0;
    };

    struct StencilFuncDesc
    {
        // The stencil operation to perform when stencil testing fails.
        StencilOp on_stencil_fail = StencilOp::KEEP;
        // The stencil operation to perform when stencil testing passes and depth testing fails.
        StencilOp on_stencil_pass_depth_fail = StencilOp::KEEP;
        // The stencil operation to perform when stencil testing and depth testing both pass.
        StencilOp on_stencil_pass_depth_pass = StencilOp::KEEP;
        // How to compare the existing stencil value vs the value output by the func
        ComparisonFunc comparison_func = ComparisonFunc::NEVER;
    };

    struct DepthStencilDesc
    {
        ComparisonFunc depth_cull_mode = ComparisonFunc::OFF;
        bool depth_write = false;
        bool stencil_enabled = false;
        u8 stencil_write_mask = 0xFF;
        u8 stencil_read_mask = 0xFF;
        StencilFuncDesc stencil_back_face;
        StencilFuncDesc stencil_front_face;
    };

    struct RasterStateDesc
    {
        FillMode fill_mode = FillMode::SOLID;
        CullMode cull_mode = CullMode::BACK_CCW;
        int32_t depth_bias = 0;
        float depth_bias_clamp = 0.0f;
        float slope_scaled_depth_bias = 0.0f;
        u8 flags = RasterizerFlags::DEPTH_CLIP_ENABLED;
    };

    struct RenderTargetBlendDesc
    {
        BlendMode blend_mode = BlendMode::OPAQUE;
        // RGB value from pixel shader output
        Blend    src_blend = Blend::ONE;
        // RGB Value stored in render target
        Blend    dst_blend = Blend::ZERO;
        // Describes how the two RGB values are combined
        BlendOp blend_op = BlendOp::ADD;
        // Alpha value from pixel shader output
        Blend    src_blend_alpha = Blend::ONE;
        // Alpha value stored in render target
        Blend    dst_blend_alpha = Blend::ZERO;
        // Describes how the two alpha values are combined
        BlendOp blend_op_alpha = BlendOp::ADD;
        // A write mask -- can be used to mask the values after the
        // blending operation before they are stored in the render target
        // NOT IMPLEMENTED
        //u8 renderTargetWriteMask = 0x0F;
    };

    struct BlendStateDesc
    {
        bool alpha_to_coverage_enable = false;
        bool independent_blend_enable = false;
        RenderTargetBlendDesc render_target_blend_descs[8] = { };
    };

    /// Can be used to create either a computer shader pipeline or a graphics pipeline
    /// 
    /// To create a Graphics pipeline, you probably just need to define the input assembly
    /// and set used_stages to PIXEL and/or VERTEX
    /// 
    /// To create a Compute pipeline, the used_stages must equel COMPUTE
    /// 
    /// In both cases, the shader file path and the resource layout handle must be set
    /// to appropriate values
    struct PipelineStateObjectDesc
    {
        ResourceLayoutHandle resource_layout = INVALID_HANDLE;
        InputAssemblyDesc input_assembly_desc = {};
        DepthStencilDesc depth_stencil_state = {};
        RasterStateDesc raster_state_desc = {};
        BlendStateDesc blend_state = {};
        BufferFormat rtv_formats[8] = {};
        BufferFormat depth_buffer_format = BufferFormat::INVALID;
        u8 used_stages = PIPELINE_STAGE_INVALID;
        std::wstring shader_file_path = L"";
    };

    // ---------- Other Descriptions ----------

    struct Viewport
    {
        float left = 0;
        float top = 0;
        float width = 0;
        float height = 0;
        float min_depth = 0;
        float max_depth = 1;
    };

    struct Scissor
    {
        u64 left = 0;
        u64 top = 0;
        u64 right = 0;
        u64 bottom = 0;
    };
}
