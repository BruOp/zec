#pragma pack_matrix( row_major )

static const uint MAX_NUM_VISIBLE = 16;

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
    uint3 num_grid_bins;
    
    // Top right corner of the near plane
    float x_near;
    float y_near;
    float z_near;
    float z_far; // The z-value for the far plane
    
    uint indices_list_idx;
    uint cluster_offsets_idx;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);
Texture2D tex2D_table[4096] : register(t0, space2);
TextureCube tex_cube_table[4096] : register(t0, space3);
Texture3D<uint2> tex3D_table[4096] : register(t0, space4);

//=================================================================================================
// Helpers
//=================================================================================================

uint3 calculate_cluster_index(float3 position_ndc, float depth_view_space) {
    uint3 cluster_idx;
    float a = x_near / y_near;
    float tan_fov = (y_near / -z_near);
    float h = 2.0 * tan_fov / float(num_grid_bins.y);
    
    uint k = uint(log(depth_view_space / -z_near) / log(1.0 + h));
    cluster_idx = uint3(0.5 * (position_ndc.xy + 1.0) * num_grid_bins.xy, k);

    return cluster_idx;
}

//=================================================================================================
// Vertex Shader
//=================================================================================================

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 position_ws : POSITIONWS;
    float4 position_clip : POSITION_CLIP;
};

PSInput VSMain(float3 position : POSITION)
{
    VSConstants vs_cb = buffers_table[vs_cb_descriptor_idx].Load<VSConstants>(renderable_idx * sizeof(VSConstants));
    
    PSInput res;
    res.position_ws = mul(vs_cb.model, float4(position, 1.0));
    res.position_cs = mul(VP, res.position_ws);
    res.position_clip = res.position_cs;
    return res;
};

//=================================================================================================
// Pixel Shader
//=================================================================================================

float4 PSMain(PSInput input) : SV_TARGET
{
    ByteAddressBuffer spot_lights_buffer = buffers_table[spot_light_buffer_idx];
    ByteAddressBuffer indices_buffer = buffers_table[indices_list_idx];
    Texture3D<uint2> cluster_offsets = tex3D_table[cluster_offsets_idx];

    float3 position_ndc = input.position_clip.xyz /= input.position_clip.w;
    position_ndc.y *= -1.0;
    
    float depth_vs = input.position_clip.w;
    // uint3 cluster_idx;
    // float a = x_near / y_near;
    // float tan_fov = (y_near / -z_near);
    // uint k = uint(log(depth_vs / -z_near) / log(1.0 + 2.0 * tan_fov / num_grid_bins.y));
    // float cluster_max_z = z_near * pow(1.0 + 2.0 * tan_fov / float(num_grid_bins.y), float(k) + 1.0);
    // float height_at_max_z = 2.0 * cluster_max_z * tan_fov;
    // float width_at_max_z = a * height_at_max_z;

    // float y = (position_ndc.y + 1.0) * (-depth_vs * tan_fov);
    // float x = (position_ndc.x + 1.0) * (a * -depth_vs * tan_fov);
    // cluster_idx.x = uint(x / width_at_max_z * float(num_grid_bins.x));
    // cluster_idx.y = uint(y / height_at_max_z * float(num_grid_bins.y));
    // cluster_idx.z = k;
    
    uint3 cluster_idx = calculate_cluster_index(position_ndc, depth_vs);

    uint2 offset_and_count = cluster_offsets[cluster_idx];
    uint light_offset = offset_and_count.x;
    uint light_count = offset_and_count.y;
    
    float3 color = float3(
        (float(light_count) / float(MAX_NUM_VISIBLE)),
        0.0,
        1.0 - (float(light_count) / float(MAX_NUM_VISIBLE)));
    
    return float4(color, 1.0);
}
