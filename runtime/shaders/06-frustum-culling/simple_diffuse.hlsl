#pragma pack_matrix( row_major )

cbuffer draw_constants_buffer : register(b0)
{
    float4x4 model;
    float3x3 normal_transform;
};


cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float3 camera_pos;
};

SamplerState default_sampler : register(s0);
SamplerState lut_sampler : register(s1);

Texture2D tex2D_table[4096] : register(t0, space1);
TextureCube tex_cube_table[4096] : register(t0, space2);

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

float3 float3_splat(float x)
{
    return float3(x, x, x);
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
    result.position_cs = mul(VP, result.position_ws);
    result.normal_ws = normalize(mul(normal_transform, normal));
    result.uv = uv;

    return result;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 view_dir = normalize(camera_pos - input.position_ws.xyz);
    
    float light_in = clamp_dot(input.normal_ws, view_dir);

    return float4(light_in, light_in, light_in, 1.0);
}
