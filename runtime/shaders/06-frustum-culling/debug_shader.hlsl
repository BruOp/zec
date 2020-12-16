#pragma pack_matrix( row_major )

cbuffer constant_buffer : register(b0)
{
    float4x4 model;
};

cbuffer constant_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float camera_position;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(float3 position : POSITION)
{
    PSInput result;

    result.position = mul(VP, mul(model, float4(position, 1.0)));

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
