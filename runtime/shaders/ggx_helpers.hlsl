#ifndef GGX_HELPERS
#define GGX_HELPERS
static const float MIN_ROUGHNESS = 0.045;
static const float DIELECTRIC_SPECULAR = 0.04;

float D_ggx(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float a = NoH * alpha;
    float k = alpha / (1.0 - NoH * NoH + a * a);
    return k * k * INV_PI;
}

float3 F_schlick(float VoH, float3 f0)
{
    float f = pow(1.0 - VoH, 5.0);
    return f + f0 * (1.0 - f);
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_smith_ggx_correlated(float NoV, float NoL, float roughness)
{
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

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

    // Construct a scale-invariant frame
    return float3x3(t, b, N);
}

float3 perturb_normal(MaterialTextureInfo normal_info, float3 N, float3 V, float2 texcoord)
{ // assume N, the interpolated vertex normal and
    // V, the view vector (vertex to eye)
    Texture2D normal_texture = ResourceDescriptorHeap[normal_info.index];
    SamplerState normal_sampler = SamplerDescriptorHeap[normal_info.sampler_idx];
    float3 map = normal_texture.Sample(normal_sampler, texcoord).xyz * 2.0 - 1.0;
    map.z = -map.z;
    float3x3 TBN = cotangent_frame(N, V, texcoord);
    return normalize(mul(map, TBN));
}

float3 calc_diffuse(float3 base_color, float metallic)
{
    return base_color * (1.0 - DIELECTRIC_SPECULAR) * (1.0 - metallic);
}

float3 calc_specular(float3 light_dir, float3 view_dir, float3 f0, float3 normal, float roughness)
{
    float3 h = normalize(light_dir + view_dir);
    float NoV = clamp(dot(normal, view_dir), 0.00005, 1.0);
    float NoL = clamp_dot(normal, light_dir);
    float NoH = clamp_dot(normal, h);
    float VoH = clamp_dot(view_dir, h);

    // Needs to be a uniform

    float D = D_ggx(NoH, roughness);
    float3 F = F_schlick(VoH, f0);
    float V = V_smith_ggx_correlated(NoV, NoL, roughness);
    return D * V * F;
}
#endif // GGX_HELPERS