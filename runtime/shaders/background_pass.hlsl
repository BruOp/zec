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

cbuffer background_constants : register(b0)
{
    float mip_level;
}

cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float3 camera_pos;
    uint envmap_idx;
    float time;
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

    result.position_cs = float4(position.x, position.y, 1.0, 1.0);

    result.direction_ws = mul(invVP, result.position_cs);
    result.direction_ws /= result.direction_ws.w;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    TextureCube src_texture = texCube_table[envmap_idx];
    return src_texture.SampleLevel(default_sampler, input.direction_ws.xyz, float(mip_level));
}
