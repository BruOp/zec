#pragma pack_matrix( row_major )

#include "ggx_helpers.hlsl"

static const uint NUM_SAMPLES = 1024;
static const float TWO_PI = 6.283185307179586;

cbuffer pass_constants : register(b0)
{
    uint out_texture_idx;
};

RWTexture2D<float2> rw_tex2D_table[4096] : register(u0, space1);

// Taken from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/genbrdflut.frag
// Based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, float NUM_SAMPLES)
{
    uint bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return float2(i / NUM_SAMPLES, bits / exp2(32));
}

// Based on Karis 2014
float3 importance_sample_GGX(float2 Xi, float alpha, float3 N)
{
    // Sample in spherical coordinates
    float phi = TWO_PI * Xi.x;
    float cos_theta = sqrt((1 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    // Construct tangent space vector
    float3 H;
    H.x = sin_theta * cos(phi);
    H.y = sin_theta * sin(phi);
    H.z = cos_theta;

    // Tangent to world space
    float3 up_vector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent_x = normalize(cross(up_vector, N));
    float3 tangent_y = cross(N, tangent_x);
    return tangent_x * H.x + tangent_y * H.y + N * H.z;
}

// Karis 2014
float2 integrateBRDF(float roughness, float NoV)
{
    float3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos

    // N points straight upwards for this integration
    const float3 N = float3(0.0, 0.0, 1.0);
    float alpha = roughness * roughness;

    float A = 0.0;
    float B = 0.0;
    for (uint i = 0u; i < NUM_SAMPLES; i++)
    {
        float2 Xi = hammersley(i, NUM_SAMPLES);
        // Sample microfacet direction
        float3 H = importance_sample_GGX(Xi, alpha, N);

        // Get the light direction
        float3 L = 2.0 * dot(V, H) * H - V;

        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));

        if (NoL > 0.0)
        {
            // Terms besides V are from the GGX PDF we're dividing by
            float V_pdf = V_smith_ggx_correlated(NoV, NoL, roughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * V_pdf;
            B += Fc * V_pdf;
        }
    }

    return 4.0 * float2(A, B) / float(NUM_SAMPLES);
}

[numthreads(8, 8, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
)
{
    RWTexture2D<float2> out_texture = rw_tex2D_table[out_texture_idx];
    float2 dims;
    out_texture.GetDimensions(dims.x, dims.y);
    // Normalized pixel coordinates (from 0 to 1)
    float2 uv = (float2(dispatch_id.xy)) / dims;
    float NoV = uv.x;
    float roughness = max(uv.y, MIN_ROUGHNESS);
    
    float2 res = integrateBRDF(roughness, NoV);

    // Scale and Bias for F0 (as per Karis 2014)
    out_texture[dispatch_id.xy] = res;
}
