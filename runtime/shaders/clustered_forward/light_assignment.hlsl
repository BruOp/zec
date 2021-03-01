#pragma pack_matrix( row_major )

static const uint MAX_NUM_VISIBLE = 32;
static const float TWO_PI = 6.283185307179586;


struct SpotLight
{
    float3 position;
    float radius;
    float3 direction;
    float umbra_angle;
    float penumbra_angle;
    float3 color;
};

struct TransformedSpotLight {
    float3 position;
    float radius;
    float3 direciton;
    float padding;
};

struct Sphere {
    float3 center;
    float radius;
};

cbuffer light_grid_info : register(b0)
{
    uint3 grid_bins;
    
    // Top right corner of the near plane
    float x_near;
    float y_near;
    float z_near;
    float z_far; // The z-value for the far plane
    
    uint indices_list_idx;
    uint cluster_offsets_idx;
    uint global_count_idx;
};

cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};


cbuffer scene_constants_buffer : register(b2)
{
    float time;
    uint radiance_map_idx;
    uint irradiance_map_idx;
    uint brdf_lut_idx;
    uint num_spot_lights;
    uint spot_light_buffer_idx;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);

RWByteAddressBuffer write_buffers_table[4096] : register(u0, space2);
RWTexture3D<uint2> write_tex3D_table[4096] : register(u0, space3);

// Taken from https://bartwronski.com/2017/04/13/cull-that-cone/
bool test_cone_vs_sphere(
    in float3 light_origin,
    in float3 light_dir,
    in float light_radius,
    in float light_umbra,
    in float4 test_sphere
) {
    const float3 V = test_sphere.xyz - light_origin;
    const float  VlenSq = dot(V, V);
    const float  V1len  = dot(V, light_dir);
    const float  distance_closest_point = cos(light_umbra) * sqrt(VlenSq - V1len*V1len) - V1len * sin(light_umbra);
 
    const bool angle_cull = distance_closest_point > test_sphere.w;
    const bool front_cull = V1len > test_sphere.w + light_radius;
    const bool back_cull  = V1len < -test_sphere.w;
    return !(angle_cull || front_cull || back_cull);
}

[numthreads(8, 8, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
)
{
    RWByteAddressBuffer global_count_buffer = write_buffers_table[global_count_idx];

    uint local_visible_count = 0;
    uint visible_lights[MAX_NUM_VISIBLE];

    ByteAddressBuffer spot_light_buffer = buffers_table[spot_light_buffer_idx];
    
    // Calculate view space AABB
    float tan_theta = y_near / -z_near;
    float h_0 = 2.0 * tan_theta / float(grid_bins.y);
    float cluster_near_z = z_near * pow(1.0 + h_0, float(dispatch_id.z));
    float cluster_far_z = z_near * pow(1.0 + h_0, float(dispatch_id.z + 1));

    float3 min_corner;
    float3 max_corner;
    min_corner.z = cluster_near_z;
    max_corner.z = cluster_far_z;

    float2 delta_XY = 2.0 / float2(grid_bins.xy);
    float2 left_bottom_ndc_xy = (float2(dispatch_id.xy) * delta_XY - 1.0);
    float2 right_top_ndc_xy = (float2(dispatch_id.xy + 1) * delta_XY - 1.0);
    
    min_corner.x = x_near / z_near * left_bottom_ndc_xy.x;
    max_corner.x = x_near / z_near * right_top_ndc_xy.x;

    if (left_bottom_ndc_xy.x < 0.5) {
        // We're on the left size of the frustum, so the left, far side will be min
        // And the near right side will be max
        min_corner.x *= cluster_far_z;
        max_corner.x *= cluster_near_z;
    } else {
        min_corner.x *= cluster_near_z;
        max_corner.x *= cluster_far_z;
    }

    min_corner.y = tan_theta * left_bottom_ndc_xy.y;
    max_corner.y = tan_theta * right_top_ndc_xy.y;
    if (left_bottom_ndc_xy.y < 0.5) {
        min_corner.y *= cluster_far_z;
        max_corner.y *= cluster_near_z;
    } else {
        min_corner.y *= cluster_near_z;
        max_corner.y *= cluster_far_z;
    }
    
    // so now our AABB is defined by min and max corner
    float4 bounding_sphere = float4(
        0.5 * (min_corner + max_corner),
        0.5 * distance(max_corner, min_corner)
    );

    // There's two options: we can transform each light on each thread,
    // resulting in a lot of wasted cycles
    // Or we can transform them ahead of time and index that light list instead,
    // Let's do the former for now, and add the latter later

    for (uint light_idx = 0; light_idx < num_spot_lights; ++light_idx) {
        SpotLight light = spot_light_buffer.Load<SpotLight>(light_idx * sizeof(SpotLight));
        float3 position = mul(view, float4(light.position, 1.0f)).xyz;
        float3 direction = normalize(mul(view, float4(light.direction, 0.0f)).xyz);

        if (
            local_visible_count < MAX_NUM_VISIBLE
            && test_cone_vs_sphere(position, direction, light.radius, light.umbra_angle, bounding_sphere)
        ) {
            visible_lights[local_visible_count++] = light_idx;
        };
    }

    // So at this point each thread should have completed looping through the lights
    // Now we need to:
    // 1. Get an offset into our global light indices list
    // 2. Add the entries in visible_lights into our global light indices list
    // 3. Store the offset + count in our per cluster buffer

    RWByteAddressBuffer indices_list = write_buffers_table[indices_list_idx];
    RWTexture3D<uint2> cluster_infos = write_tex3D_table[cluster_offsets_idx];
    
    uint offset = 0;
    // TODO: Do we even care about this?
    global_count_buffer.InterlockedAdd(0, local_visible_count, offset);
    
    uint MAX_INDEX = 0;
    indices_list.GetDimensions(MAX_INDEX);
    for (uint i = 0; i < local_visible_count; i++) {
        uint idx = min(offset + i, MAX_INDEX);
        indices_list.Store(idx * sizeof(uint), visible_lights[i]);
    }
    
    cluster_infos[dispatch_id] = uint2(offset, local_visible_count);
}
