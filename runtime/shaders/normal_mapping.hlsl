//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma pack_matrix( row_major )

cbuffer draw_constants_buffer : register(b0)
{
    float4x4 model;
    float4x4 inv_model;
    uint albedo_texture_idx;
    uint normal_texture_idx;
};

cbuffer view_constants_buffer : register(b1)
{
    float4x4 view;
    float4x4 proj;
    float4x4 VP;
    float3 camera_pos;
    float time;
};

SamplerState default_sampler : register(s0);

Texture2D tex2D_table[4096] : register(t0, space1);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float3 normal_ws : NORMAL;
    float2 uv : TEXCOORD;
};

//=================================================================================================
// Helper Functions
//=================================================================================================
// Taken from http://www.thetenthplanet.de/archives/1180
// And https://github.com/microsoft/DirectXTK12/blob/master/Src/Shaders/Utilities.fxh#L20
float3x3 cotangent_frame(float3 N, float3 p, float2 uv)
{
    // Get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // Solve the linear system
    float3x3 M = float3x3(dp1, dp2, cross(dp1, dp2));
    float2x3 inverse_M = float2x3(cross(M[1], M[2]), cross(M[2], M[0]));
    float3 t = normalize(mul(float2(duv1.x, duv2.x), inverse_M));
    float3 b = normalize(mul(float2(duv1.y, duv2.y), inverse_M));
    
    // Construct a scale-invari�ant frame 
    return float3x3(t, b, N);
}

float3 perturb_normal(Texture2D normal_map, float3 N, float3 V, float2 texcoord)
{ // assume N, the interpolated vertex normal and
    // V, the view vector (vertex to eye)
    float3 map = normal_map.Sample(default_sampler, texcoord).xyz * 2.0 - 1.0;
    map.z = sqrt(1. - dot(map.xy, map.xy));
    float3x3 TBN = cotangent_frame(N, -V, texcoord);
    return normalize(mul(map, TBN));
}

float clamp_dot(float3 A, float3 B)
{
    return clamp(dot(A, B), 0.0, 1.0f);
}

//=================================================================================================
// Vertex Shader
//=================================================================================================

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position_ws = mul(model, float4(position, 1.0));
    result.position_cs = mul(proj, mul(view, float4(position, 1.0)));
    result.normal_ws = normalize(mul(inv_model, float4(normal, 0.0)).xyz);
    result.uv = uv;

    return result;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================

float4 PSMain(PSInput input) : SV_TARGET
{
    Texture2D albedo_texture = tex2D_table[albedo_texture_idx];
    Texture2D normal_texture = tex2D_table[normal_texture_idx];

    float3 view_dir = camera_pos - input.position_ws.xyz;

    const float3 light_pos = float3(5.0 * sin(time), 6.0, 5.0 * cos(time));
    const float light_intensity = 30.0f;
    const float3 light_color = float3(1.0, 1.0, 1.0);

    float3 base_color = albedo_texture.Sample(default_sampler, input.uv).rgb;
    float3 normal = perturb_normal(normal_texture, input.normal_ws, view_dir, input.uv);

    float3 light_dir = light_pos - input.position_ws.xyz;
    float light_dist = length(light_dir);
    light_dir = light_dir / light_dist;

    float attenuation = light_intensity / (light_dist * light_dist);
    float3 light_out = attenuation * light_color * clamp_dot(normal, light_dir);

    return float4(light_out * base_color, 1.0);
}
