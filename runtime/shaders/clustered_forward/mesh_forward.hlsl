#pragma pack_matrix( row_major )

static const uint INVALID_TEXTURE_IDX = 0xffffffff;
static const float DIELECTRIC_SPECULAR = 0.04;
static const float MIN_ROUGHNESS = 0.045;
static const float PI = 3.141592653589793;
static const float INV_PI = 0.318309886;


//=================================================================================================
// Bindings
//=================================================================================================

struct VSConstants
{
    float4x4 model;
    row_major float3x4 normal;
};

struct MaterialData
{
    float4 base_color_factor;
    float3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    uint base_color_texture_idx;
    uint metallic_roughness_texture_idx;
    uint normal_texture_idx;
    uint occlusion_texture_idx;
    uint emissive_texture_idx;
    float padding[18];
};

struct SpotLight
{
    float3 position;
    float radius;
    float3 direction;
    float umbra_angle;
    float penumbra_angle;
    float3 color;
};

cbuffer draw_call_constants0 : register(b0)
{
    uint renderable_idx;
    uint material_idx;

}
cbuffer draw_call_constants1 : register(b1)
{
    uint vs_cb_descriptor_idx;
    uint material_buffer_descriptor_idx;
};

cbuffer view_constants_buffer : register(b2)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};

cbuffer scene_constants_buffer : register(b3)
{
    float time;
    uint radiance_map_idx;
    uint irradiance_map_idx;
    uint brdf_lut_idx;
    uint num_spot_lights;
    uint spot_light_buffer_idx;
};

cbuffer clustered_lighting_constants : register(b4)
{
    uint grid_width;
    uint grid_height;
    uint pre_mid_depth;
    uint post_mid_depth;
    
    // Top right corner of the near plane
    float x_near;
    float y_near;
    float z_near;
    float z_far; // The z-value for the far plane
    float mid_plane; // The z-value that defines a transition from one partitioning scheme to another;
    
    uint indices_list_idx;
    uint cluster_offsets_idx;
};

SamplerState default_sampler : register(s0);

ByteAddressBuffer buffers_table[4096] : register(t0, space1);
Texture2D tex2D_table[4096] : register(t0, space2);
TextureCube tex_cube_table[4096] : register(t0, space3);
Texture3D<uint2> tex3D_table[4096] : register(t0, space4);


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

uint3 calculate_cluster_index(float3 position_ndc, float depth_view_space) {
    uint3 cluster_idx;
    float a = x_near / y_near;
    float tan_fov = (y_near / -z_near);

    uint k;
    if (-depth_view_space <= mid_plane) {
        // Beyond mid plane, switch to cubic partitioning
        // Ola Olsson
        k = pre_mid_depth + uint(log2(-depth_view_space / mid_plane) / log2(1.0 + 2.0 * tan_fov / grid_height));
    } else {
        //Linear depth partitioning
        // linear
        k = uint((-depth_view_space - z_near) / (mid_plane - z_near) * float(pre_mid_depth));
    }

    cluster_idx = uint3(0.5 * (position_ndc.xy + 1.0) * float2(grid_width, grid_height), k);

    return cluster_idx;
}


//=================================================================================================
// Vertex Shader
//=================================================================================================

struct PSInput
{
    float4 sv_position : SV_POSITION;
    float4 position_cs : POSITIONCS;
    float4 position_ws : POSITIONWS;
    float3 normal_ws : NORMAL;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float3 position : POSITION0, float3 normal : NORMAL0, float2 uv : TEXCOORD0)
{
    VSConstants vs_cb = buffers_table[vs_cb_descriptor_idx].Load<VSConstants>(renderable_idx * sizeof(VSConstants));
    
    PSInput res;
    res.position_ws = mul(vs_cb.model, float4(position, 1.0));
    res.sv_position = mul(VP, res.position_ws);
    res.position_cs = res.sv_position;
    res.uv = uv;
    // This sucks!
    float3x3 normal_transform = float3x3(vs_cb.normal._11_12_13, vs_cb.normal._21_22_23, vs_cb.normal._31_32_33);
    res.normal_ws = mul(normal_transform, normal).xyz;
    return res;
};

//=================================================================================================
// Pixel Shader
//=================================================================================================

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 view_dir = normalize(camera_pos - input.position_ws.xyz);
    
    MaterialData mat = buffers_table[material_buffer_descriptor_idx].Load<MaterialData>(material_idx * sizeof(MaterialData));
    
    float4 base_color = mat.base_color_factor;
    if (mat.base_color_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D base_color_texture = tex2D_table[mat.base_color_texture_idx];
        base_color *= base_color_texture.Sample(default_sampler, input.uv);
    }

    float3 normal = input.normal_ws;
    if (mat.normal_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D normal_texture = tex2D_table[mat.normal_texture_idx];
        normal = perturb_normal(normal_texture, normal, view_dir, input.uv);
    }
    else
    {
        normal = normalize(input.normal_ws);
    }

    float occlusion = 0.0f;
    float roughness = mat.roughness_factor;
    float metallic = mat.metallic_factor;

    if (mat.metallic_roughness_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D metallic_roughness_texture = tex2D_table[mat.metallic_roughness_texture_idx];
        float4 mr_texture_read = metallic_roughness_texture.Sample(default_sampler, input.uv);
        metallic *= mr_texture_read.b;
        roughness *= mr_texture_read.g;
    }
    roughness = max(MIN_ROUGHNESS, roughness);
    if (mat.occlusion_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D occlusion_texture = tex2D_table[mat.occlusion_texture_idx];
        occlusion = occlusion_texture.Sample(default_sampler, input.uv).r;
    }

    float3 emissive = mat.emissive_factor;
    if (mat.emissive_texture_idx != INVALID_TEXTURE_IDX)
    {
        Texture2D emissive_texture = tex2D_table[mat.emissive_texture_idx];
        emissive *= emissive_texture.Sample(default_sampler, input.uv).rgb;
    }

    float3 brdf_diffuse = calc_diffuse(base_color.xyz, metallic);
    float3 f0 = lerp(float3_splat(DIELECTRIC_SPECULAR), base_color.rgb, metallic);
    
    float3 color = float3(0.0, 0.0, 0.0);
    
    // if (irradiance_map_idx != INVALID_TEXTURE_IDX && radiance_map_idx != INVALID_TEXTURE_IDX)
    // {
    //     Texture2D brdf_lut = tex2D_table[brdf_lut_idx];
    //     TextureCube radiance_map = tex_cube_table[radiance_map_idx];
    //     TextureCube irradiance_map = tex_cube_table[irradiance_map_idx];

    //     float NoV = clamp(dot(normal, view_dir), 0.00005, 1.0);
    //     float2 f_ab = brdf_lut.Sample(default_sampler, float2(NoV, roughness)).rg;
    //     float3 sample_dir = reflect(-view_dir, normal);
    //     float num_env_levels, width, height;
    //     radiance_map.GetDimensions(0, width, height, num_env_levels);
    //     float3 radiance = radiance_map.SampleLevel(default_sampler, sample_dir, roughness * num_env_levels).rgb;
    //     float3 irradiance = irradiance_map.Sample(default_sampler, sample_dir).rgb;

    //     // Multiple scattering, from Fdez-Aguera
    //     // See https://bruop.github.io/ibl/ for additional explanation
    //     //float3 Fr = max(float3_splat(1.0 - roughness), f0) - f0;
    //     //float3 k_S = f0 + Fr * pow(1.0 - NoV, 5.0);
    //     //float3 FssEss = k_S * f_ab.x + f_ab.y;
    //     // Typically, simply:
    //     float3 FssEss = f0 * f_ab.x + f_ab.y;
    //     float Ems = (1.0 - (f_ab.x + f_ab.y));
    //     float3 f_avg = f0 + (1.0 - f0) / 21.0;
    //     float3 FmsEms = Ems * FssEss * f_avg / (1.0 - f_avg * Ems);
    //     float3 k_D = base_color.rgb * (1.0 - FssEss - FmsEms);

    //     color += occlusion * (FssEss * radiance + base_color.rgb * irradiance);
    // }

    // Get light info
    float3 position_ndc = input.position_cs.xyz /= input.position_cs.w;
    float depth_vs = input.position_cs.w;

    uint3 cluster_idx = calculate_cluster_index(position_ndc, depth_vs);

    ByteAddressBuffer spot_lights_buffer = buffers_table[spot_light_buffer_idx];
    ByteAddressBuffer indices_buffer = buffers_table[indices_list_idx];
    Texture3D<uint2> cluster_offsets = tex3D_table[cluster_offsets_idx];

    uint2 offset_and_count = cluster_offsets[cluster_idx];
    uint light_offset = offset_and_count.x;
    uint light_count = offset_and_count.y;

    float3 light_out = float3(0.0, 0.0, 0.0);
    for (uint i =0; i < light_count; ++i) {
        uint light_idx = indices_buffer.Load<uint>((i + light_offset) * sizeof(uint));
        SpotLight spot_light = spot_lights_buffer.Load<SpotLight>(light_idx * sizeof(SpotLight));
        float3 light_dir = spot_light.position - input.position_ws.xyz;
        float light_dist = length(light_dir);
        light_dir = light_dir / light_dist;
        
        float cos_umbra = cos(spot_light.umbra_angle);
        // Taken from RTR v4 p115
        float t = saturate((dot(light_dir, -spot_light.direction) - cos_umbra) / (cos(spot_light.penumbra_angle) - cos_umbra));
        float directonal_attenuation = t * t;
        
        // Taken from KARIS 2013
        // float distance_attenuation = 1.0 / (light_dist * light_dist + 0.01);
        float distance_attenuation = pow(saturate(1.0 - pow(light_dist / spot_light.radius, 4.0)), 2.0) / (light_dist * light_dist + 0.01);
        
        float attenuation = directonal_attenuation * distance_attenuation * clamp_dot(normal, light_dir);
        if (attenuation == 0) {
            continue;
        }
        float3 light_in = attenuation * spot_light.color;
    
        float3 brdf_specular = calc_specular(light_dir, view_dir, f0, normal, roughness);
        
        light_out += light_in * (INV_PI *  brdf_diffuse + brdf_specular);
    }
    color += light_out * PI;

    return float4(color, 1.0);
}
