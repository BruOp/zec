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

cbuffer pass_constants : register(b0)
{
    uint src_texture_idx;
    float exposure;
};

SamplerState default_sampler : register(s0);

Texture2D tex2D_table[4096] : register(t0, space1);

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float2 uv : TEXCOORD;
};


float3 convertRGB2XYZ(float3 _rgb)
{
	// Reference:
	// RGB/XYZ Matrices
	// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    float3 xyz;
    xyz.x = dot(float3(0.4124564, 0.3575761, 0.1804375), _rgb);
    xyz.y = dot(float3(0.2126729, 0.7151522, 0.0721750), _rgb);
    xyz.z = dot(float3(0.0193339, 0.1191920, 0.9503041), _rgb);
    return xyz;
}

float3 convertXYZ2RGB(float3 _xyz)
{
    float3 rgb;
    rgb.x = dot(float3(3.2404542, -1.5371385, -0.4985314), _xyz);
    rgb.y = dot(float3(-0.9692660, 1.8760108, 0.0415560), _xyz);
    rgb.z = dot(float3(0.0556434, -0.2040259, 1.0572252), _xyz);
    return rgb;
}

float3 convertXYZ2Yxy(float3 _xyz)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
    float inv = 1.0 / dot(_xyz, float3(1.0, 1.0, 1.0));
    return float3(_xyz.y, _xyz.x * inv, _xyz.y * inv);
}

float3 convertYxy2XYZ(float3 _Yxy)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
    float3 xyz;
    xyz.x = _Yxy.x * _Yxy.y / _Yxy.z;
    xyz.y = _Yxy.x;
    xyz.z = _Yxy.x * (1.0 - _Yxy.y - _Yxy.z) / _Yxy.z;
    return xyz;
}

float3 convertRGB2Yxy(float3 _rgb)
{
    return convertXYZ2Yxy(convertRGB2XYZ(_rgb));
}

float3 convertYxy2RGB(float3 _Yxy)
{
    return convertXYZ2RGB(convertYxy2XYZ(_Yxy));
}

float Tonemap_ACES(float x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position_cs = float4(position.x, position.y, 1.0, 1.0);
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    Texture2D src_texture = tex2D_table[src_texture_idx];
    float4 color = src_texture.Sample(default_sampler, input.uv);
    float3 rgb = color.rgb;
    
    float3 Yxy = convertRGB2Yxy(rgb);

    Yxy.x *= exposure;

    rgb = convertYxy2RGB(Yxy);

    rgb.x = Tonemap_ACES(rgb.x);
    rgb.y = Tonemap_ACES(rgb.y);
    rgb.z = Tonemap_ACES(rgb.z);
    return float4(rgb, color.a);
}
