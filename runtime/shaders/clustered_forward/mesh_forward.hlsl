#pragma pack_matrix( row_major )

struct VSConstants
{
    float4x4 model;
    row_major float3x4 normal;
};

struct MaterialData
{
    float4 base_color_factor;
    float3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    uint base_color_texture_idx;
    uint metallic_roughness_texture_idx;
    uint normal_texture_idx;
    uint occlusion_texture_idx;
    uint emissive_texture_idx;
    float padding[50];
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

SamplerState default_sampler : register(s0);

ByteAddressBuffer buffers_table[4096] : register(t0, space1);
Texture2D tex2D_table[4096] : register(t0, space2);
TextureCube tex_cube_table[4096] : register(t0, space3);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float3 position : POSITION0, float3 normal : NORMAL0, float2 uv : TEXCOORD0)
{
    VSConstants vs_cb = buffers_table[vs_cb_descriptor_idx].Load<VSConstants>(renderable_idx * sizeof(VSConstants));
    
    PSInput res;
    res.position_ws = mul(vs_cb.model, float4(position, 1.0));
    res.position_cs = mul(VP, res.position_ws);
    res.uv = uv;
    // This sucks!
    float3x3 normal_transform = float3x3(vs_cb.normal._11_12_13, vs_cb.normal._21_22_23, vs_cb.normal._31_32_33);
    res.normal = mul(normal_transform, normal).xyz;
    return res;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = float4(input.normal, 1.0);
    return color;
};
