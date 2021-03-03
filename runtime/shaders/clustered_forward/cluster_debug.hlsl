#pragma pack_matrix( row_major )

static const uint MAX_NUM_VISIBLE = 32;

//=================================================================================================
// Bindings
//=================================================================================================

struct VSConstants
{
    float4x4 model;
    row_major float3x4 normal;
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
    // position_ndc.y *= -1.0;
    
    float depth_vs = input.position_clip.w;
    
    uint3 cluster_idx = calculate_cluster_index(position_ndc, depth_vs);

    uint2 offset_and_count = cluster_offsets[cluster_idx];
    uint light_offset = offset_and_count.x;
    uint light_count = offset_and_count.y;

    float unused_counter = 0.0;
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
        float distance_attenuation = pow(saturate(1.0 - pow(light_dist / spot_light.radius, 4.0)), 2.0) / (light_dist * light_dist + 0.01);
        
        float attenuation = directonal_attenuation * distance_attenuation;
        if (attenuation == 0) {
            unused_counter += 1.0;
        }
    }


    float3 color = float3(
        float(light_count - unused_counter) / float(MAX_NUM_VISIBLE),
        unused_counter / float(MAX_NUM_VISIBLE),
        0.0);
    
    return float4(color, 1.0);
}
