#pragma pack_matrix( row_major )

cbuffer background_constants : register(b0)
{
    float mip_level;
}


cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};


cbuffer scene_constants_buffer : register(b2)
{
    float time;
    uint radiance_map_idx;
    uint irradiance_map_idx;
    uint brdf_lut_idx;
    uint num_spot_lights;
    uint spot_light_buffer_idx;
};
SamplerState default_sampler : register(s0);

TextureCube texCube_table[4096] : register(t0, space1);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 direction_ws : DIRECTION;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position_cs = float4(position.x, position.y, 0.0, 1.0);

    result.direction_ws = mul(invVP, result.position_cs);
    result.direction_ws /= result.direction_ws.w;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    TextureCube src_texture = texCube_table[radiance_map_idx];
    return src_texture.SampleLevel(default_sampler, input.direction_ws.xyz, float(mip_level));
}
