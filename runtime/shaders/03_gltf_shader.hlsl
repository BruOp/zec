#include "common.hsh"
#include "mesh_common.hsh"
#include "ggx_helpers.hlsl"

//=================================================================================================
// Bindings
//=================================================================================================

cbuffer draw_call_constants0 : register(b0)
{
    float3x4 model;
    float3x4 normal_transform;
    PBRMaterialData material;
};


cbuffer view_constants_buffer : register(b1)
{
    float4x4 view;
    float4x4 proj;
    float4x4 VP;
    float3 camera_pos;
    float time;
};

//=================================================================================================
// Helper Functions
//=================================================================================================

struct PixelMaterialData
{
    float4 albedo;
    float3 emissive;
    float metalness;
    float3 normal;
    float roughness;
    float occlusion;
};

PixelMaterialData gather_material_data(PBRMaterialData material, float3 normal_ws, float2 uv, float3 view_dir)
{
    PixelMaterialData out_data;
    out_data.albedo = material.albedo;
    if (is_texture_id_valid(material.albedo_texture.index))
    {
        SamplerState albedo_sampler = SamplerDescriptorHeap[material.albedo_texture.sampler_idx];
        Texture2D albedo_texture = ResourceDescriptorHeap[material.albedo_texture.index];
        out_data.albedo *= albedo_texture.Sample(albedo_sampler, uv);
    }

#if defined (ALPHA_CUTOUT)
    if (out_data.albedo.a < material.alpha_cutoff)
    {
        discard;
    }
#endif

    out_data.normal = normal_ws;
    if (is_texture_id_valid(material.normal_texture.index))
    {
        out_data.normal = perturb_normal(material.normal_texture, out_data.normal, view_dir, uv);
    }
    else
    {
        out_data.normal = normalize(normal_ws);
    }

    out_data.occlusion = 1.0f;
    out_data.roughness = material.roughness;
    out_data.metalness = material.metalness;

    if (is_texture_id_valid(material.metalness_roughness_ao_texture.index))
    {
        SamplerState metallic_roughness_sampler = SamplerDescriptorHeap[material.metalness_roughness_ao_texture.sampler_idx];
        Texture2D metalness_roughness_ao_texture = ResourceDescriptorHeap[material.metalness_roughness_ao_texture.index];
        float4 mr_texture_read = metalness_roughness_ao_texture.Sample(metallic_roughness_sampler, uv);
        out_data.occlusion *= mr_texture_read.r;
        out_data.metalness *= mr_texture_read.b;
        out_data.roughness *= mr_texture_read.g;
    }
    out_data.roughness = max(MIN_ROUGHNESS, out_data.roughness);

    out_data.emissive = 0.f.xxx; //material.emissive;
    //if (is_texture_id_valid(material.emissive_texture.index))
    //{
    //    Texture2D emissive_texture = ResourceDescriptorHeap[material.emissive_texture.index];
    //    out_data.emissive *= emissive_texture.Sample(default_sampler, uv).rgb;
    //}
    return out_data;
}

struct PSInput
{
    float4 position : SV_POSITION;
    //float4 position_cs : POSITIONCS;
    float4 position_ws : POSITIONWS;
    float3 normal_ws : NORMAL;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL0, float2 uv : TEXCOORD0)
{
    PSInput result;
    
    result.position_ws = float4(mul(model, float4(position, 1.f)), 1.f);
    result.position = mul(VP, result.position_ws);
    float3x3 normalT = float3x3(normal_transform._11_12_13, normal_transform._21_22_23, normal_transform._31_32_33);
    result.normal_ws = mul(normalT, normal);
    result.uv = uv;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 view_dir = normalize(camera_pos - input.position_ws.xyz);
    float3 light_dir = -normalize(float3(1.f, 1.f, 0.f));

    PixelMaterialData mat_data = gather_material_data(material, normalize(input.normal_ws), input.uv, view_dir);

    float3 out_color = 0.f.xxx;
    float3 brdf_diffuse = calc_diffuse(mat_data.albedo.rgb, mat_data.metalness);
    float3 f0 = lerp(float3_splat(DIELECTRIC_SPECULAR), mat_data.albedo.rgb, mat_data.metalness);
    float3 brdf_specular = calc_specular(light_dir, view_dir, f0, mat_data.normal, mat_data.roughness);
    float3 light_out = float3(0.0, 0.0, 0.0);
    // TODO: Support light colors + intensity
    float3 light_in = clamp_dot(mat_data.normal, light_dir);
    light_out += light_in * (INV_PI * brdf_diffuse + brdf_specular);

    // Analytic integration constant, see RTR
    out_color += light_out * PI;
#if defined(ALPHA_BLEND)
    return float4(out_color * mat_data.albedo.a, mat_data.albedo.a);
#else
    return float4(remap(mat_data.normal, -1.f, 1.f, 0.f, 1.f), 1.0);
#endif
}
