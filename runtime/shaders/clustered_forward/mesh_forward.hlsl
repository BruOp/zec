#pragma pack_matrix( row_major )

struct VSConstants
{
    float4x4 model;
    uint vertex_positions_idx;
    uint vertex_uvs_idx;
    float padding[46];
};

// Two constants
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
    float3 camera_pos;
    uint radiance_map_idx;
    uint irradiance_map_idx;
    uint brdf_lut_idx;
    float num_env_levels;
    float time;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float2 uv : TEXCOORD;
};

PSInput VSMain(uint vert_id : SV_VertexID)
{
    VSConstants vs_cb = buffers_table[vs_cb_descriptor_idx].Load<VSConstants>(renderable_idx * sizeof(VSConstants));
    float4 vert_position = float4(
        buffers_table[vs_cb.vertex_positions_idx].Load<float3>(vert_id * sizeof(float3)),
        1.0f);
    float2 uv = buffers_table[vs_cb.vertex_uvs_idx].Load<float2>(vert_id * sizeof(float2));

    PSInput res;
    res.position_ws = mul(vs_cb.model, vert_position);
    res.position_cs = mul(VP, res.position_ws);
    res.uv = uv;
    return res;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = float4(input.uv, 0.0, 1.0);
    return color;
};
