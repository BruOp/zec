#pragma pack_matrix( row_major )

struct VSConstantBuffer
{
    float4x4 model;
    uint vertex_positions_idx;
    uint vertex_uvs_idx;
    float padding[34 + 12];
};

cbuffer draw_call_constants : register(b0)
{
    uint renderable_idx;
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

StructuredBuffer<float3> vertex_buffers[4096] : register(t0, space1);
StructuredBuffer<float2> uv_buffers[4096] : register(t0, space2);
StructuredBuffer<VSConstantBuffer> vs_constant_buffers_table[4096] : register(t0, space3);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float2 uv : TEXCOORD;
};

PSInput VSMain(uint vert_id : SV_VertexID)
{
    StructuredBuffer<VSConstantBuffer> vs_contant_buffers = vs_constant_buffers_table[vs_cb_descriptor_idx];
    VSConstantBuffer vs_cb = vs_contant_buffers[renderable_idx];
    float4 vert_position = float4(
        vertex_buffers[vs_cb.vertex_positions_idx][vert_id],
        1.0f);
    float2 uv = uv_buffers[vs_cb.vertex_uvs_idx].Load(vert_id);

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
