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

cbuffer view_constants_buffer : register(b0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 VP;
    float3 camera_pos;
    float time;
}

cbuffer draw_constants_buffer : register(b1)
{
    float4x4 model;
    float4x4 inv_model;
}



struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float3 normal_ws : NORMAL;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position_ws = mul(model, float4(position, 1.0));
    result.position_cs = mul(proj, mul(view, float4(position, 1.0)));
    result.normal_ws = normalize(mul(inv_model, float4(normal, 0.0)).xyz);
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(input.position_ws.xyz, 1.0);
}
