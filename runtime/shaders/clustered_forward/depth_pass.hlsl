#pragma pack_matrix( row_major )

static const uint INVALID_TEXTURE_IDX = 0xffffffff;
static const float DIELECTRIC_SPECULAR = 0.04;
static const float MIN_ROUGHNESS = 0.045;
static const float PI = 3.141592653589793;
static const float INV_PI = 0.318309886;


//=================================================================================================
// Bindings
//=================================================================================================

struct VSConstants
{
    float4x4 model;
    row_major float3x4 normal;
};

cbuffer draw_call_constants0 : register(b0)
{
    uint renderable_idx;

}
cbuffer draw_call_constants1 : register(b1)
{
    uint vs_cb_descriptor_idx;
};

cbuffer view_constants_buffer : register(b2)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);


//=================================================================================================
// Vertex Shader
//=================================================================================================

struct Output
{
    float4 position_cs : SV_POSITION;
};

Output VSMain(float3 position : POSITION0)
{
    VSConstants vs_cb = buffers_table[vs_cb_descriptor_idx].Load<VSConstants>(renderable_idx * sizeof(VSConstants));
    
    Output res;
    res.position_cs = mul(VP, mul(vs_cb.model, float4(position, 1.0f)));
    return res;
};

