#pragma pack_matrix( row_major )

cbuffer constant_buffer : register(b0)
{
    float4x4 model_transform;
    float4x4 view_matrix;
    float4x4 projection_matrix;
    float4x4 VP;
};


struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float3 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = mul(VP, mul(model_transform, float4(position, 1.0)));
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
