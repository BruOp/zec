#pragma pack_matrix( row_major )


static const uint INVALID_TEXTURE_IDX = 0xffffffff;
static const float DIELECTRIC_SPECULAR = 0.04;
static const float MIN_ROUGHNESS = 0.045;
static const float PI = 3.141592653589793;


cbuffer draw_constants_buffer : register(b0)
{
    float4x4 model;
    float3x3 normal_transform;
    float4 base_color_factor;
    float3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    uint base_color_texture_idx;
    uint metallic_roughness_texture_idx;
    uint normal_texture_idx;
    uint occlusion_texture_idx;
    uint emissive_texture_idx;
};


cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float3 camera_pos;
    uint radiance_map_idx;
    uint irradiance_map_idx;
    uint brdf_lut_idx;
    float num_env_levels;
    float time;
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

float3 perturb_normal(Texture2D normal_map, float3 N, float3 V, float2 texcoord)
{ // assume N, the interpolated vertex normal and
    // V, the view vector (vertex to eye)
    float3 map = normal_map.Sample(default_sampler, texcoord).xyz * 2.0 - 1.0;
    map.z = sqrt(1. - dot(map.xy, map.xy));
    float3x3 TBN = cotangent_frame(N, -V, texcoord);
    return normalize(mul(map, TBN));
}


float D_GGX(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float a = NoH * alpha;
    float k = alpha / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

float3 F_Schlick(float VoH, float3 f0)
{
    float f = pow(1.0 - VoH, 5.0);
    return f + f0 * (1.0 - f);
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
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

    float D = D_GGX(NoH, roughness);
    float3 F = F_Schlick(VoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);
    return D * V * F;
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

    const float3 light_pos = float3(10.0 * sin(time), 10.0, 10.0 * cos(time));
    const float light_intensity = 100.0f;
    const float3 light_color = float3_splat(1.0);

    float4 base_color = base_color_factor;
    if (base_color_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D base_color_texture = tex2D_table[base_color_texture_idx];
        base_color *= base_color_texture.Sample(default_sampler, input.uv);
    }

    float3 normal = input.normal_ws;
    if (normal_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D normal_texture = tex2D_table[normal_texture_idx];
        normal = perturb_normal(normal_texture, normal, view_dir, input.uv);
    }
    else
    {
        normal = normalize(input.normal_ws);
    }

    float occlusion = 0.0f;
    float roughness = roughness_factor;
    float metallic = metallic_factor;

    if (metallic_roughness_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D metallic_roughness_texture = tex2D_table[metallic_roughness_texture_idx];
        float4 mr_texture_read = metallic_roughness_texture.Sample(default_sampler, input.uv);
        metallic *= mr_texture_read.b;
        roughness *= mr_texture_read.g;
    }
    roughness = max(MIN_ROUGHNESS, roughness);
    if (occlusion_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D occlusion_texture = tex2D_table[occlusion_texture_idx];
        occlusion = occlusion_texture.Sample(default_sampler, input.uv).r;
    }

    float3 emissive = emissive_factor;
    if (emissive_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D emissive_texture = tex2D_table[emissive_texture_idx];
        emissive *= emissive_texture.Sample(default_sampler, input.uv).rgb;
    }

    float3 light_dir = light_pos - input.position_ws.xyz;
    float light_dist = length(light_dir);
    light_dir = light_dir / light_dist;

    float attenuation = light_intensity / (light_dist * light_dist);
    float3 light_in = attenuation * light_color * clamp_dot(normal, light_dir);


    float3 f0 = lerp(float3_splat(DIELECTRIC_SPECULAR), base_color.rgb, metallic);
    float3 light_out = float3(0.0, 0.0, 0.0);

    if (irradiance_map_idx != INVALID_TEXTURE_IDX && radiance_map_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D brdf_lut = tex2D_table[brdf_lut_idx];
        TextureCube radiance_map = tex_cube_table[radiance_map_idx];
        TextureCube irradiance_map = tex_cube_table[irradiance_map_idx];

        float NoV = clamp(dot(normal, view_dir), 0.00005, 1.0);
        float2 f_ab = brdf_lut.Sample(lut_sampler, float2(NoV, roughness)).rg;
        float3 sample_dir = reflect(-view_dir, normal);
        float3 radiance = radiance_map.SampleLevel(default_sampler, sample_dir, roughness * num_env_levels).rgb;
        float3 irradiance = irradiance_map.Sample(default_sampler, sample_dir).rgb;

        // Multiple scattering, from Fdez-Aguera
        // See https://bruop.github.io/ibl/ for additional explanation
        //float3 Fr = max(float3_splat(1.0 - roughness), f0) - f0;
        //float3 k_S = f0 + Fr * pow(1.0 - NoV, 5.0);
        //float3 FssEss = k_S * f_ab.x + f_ab.y;
        // Typically, simply:
        float3 FssEss = f0 * f_ab.x + f_ab.y;
        float Ems = (1.0 - (f_ab.x + f_ab.y));
        float3 f_avg = f0 + (1.0 - f0) / 21.0;
        float3 FmsEms = Ems * FssEss * f_avg / (1.0 - f_avg * Ems);
        float3 k_D = base_color.rgb * (1.0 - FssEss - FmsEms);

        light_out += occlusion * (FssEss * radiance + base_color.rgb * irradiance);
    }

    //float3 diffuse = calc_diffuse(base_color.xyz, metallic);
    //float3 specular = calc_specular(light_dir, view_dir, f0, normal, roughness);
    //light_out += occlusion * light_in * (diffuse + PI * specular);
    light_out += emissive;

    return float4(light_out, 1.0);
}
