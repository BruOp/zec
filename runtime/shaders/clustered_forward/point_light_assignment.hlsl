#pragma pack_matrix( row_major )

static const float TWO_PI = 6.283185307179586;
static const uint NUM_THREADS = 64;

#include "light_helpers.hlsl"
#include "cluster_helpers.hlsl"

cbuffer light_grid_info : register(b0)
{
    ClusterSetup cluster_setup;
    uint spot_light_indices_list_idx;
    uint point_light_indices_list_idx;
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
    uint num_point_lights;
    uint spot_light_buffer_idx;
    uint point_light_buffer_idx;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);

RWByteAddressBuffer write_buffers_table[4096] : register(u0, space2);

float distance_squared_point_to_AABB(in float3 p, in float3 min_corner, in float3 max_corner)
{
    float distance_squared = 0.0;
    
    [unroll]
    for (uint i = 0; i < 3; ++i) {
        float v = p[i];
        if (v < min_corner[i]) {
            distance_squared += (min_corner[i] - v) * (min_corner[i] - v);
        }
        if (v > max_corner[i]) {
            distance_squared += (v - max_corner[i]) * (v - max_corner[i]);
        }
    }
    return distance_squared;
}

// Returns true if the two objects are intersecting
bool test_AABB_vs_sphere(
    in float3 min_corner,
    in float3 max_corner,
    in float4 sphere
) {
    float distance_squared = distance_squared_point_to_AABB(sphere.xyz, min_corner, max_corner);
    return distance_squared <= sphere.w * sphere.w;
}

groupshared float gs_light_pos_x[NUM_THREADS];
groupshared float gs_light_pos_y[NUM_THREADS];
groupshared float gs_light_pos_z[NUM_THREADS];
groupshared float gs_light_radius[NUM_THREADS];

[numthreads(8, 8, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
)
{
    float3 min_corner, max_corner;
    calculate_cluster_AABB(dispatch_id, cluster_setup, min_corner, max_corner);
    
    // so now our AABB is defined by min and max corner
    float4 bounding_sphere = float4(
        0.5 * (min_corner + max_corner),
        0.5 * distance(max_corner, min_corner)
    );

    uint local_visible_count = 0;
    uint visible_lights[MAX_LIGHTS_PER_CLUSTER];

    ByteAddressBuffer point_light_buffer = buffers_table[point_light_buffer_idx];
    uint num_batches = uint((num_point_lights + NUM_THREADS - 1) / NUM_THREADS);

    for (uint batch_idx = 0; batch_idx < num_batches; ++batch_idx) {
        uint batch_offset = batch_idx * NUM_THREADS;
        // Each thread loads a light, and then transforms it, and then stores it in shared memory
        PointLight light = point_light_buffer.Load<PointLight>((batch_offset + group_idx) * sizeof(PointLight));
        
        float3 position = mul(view, float4(light.position, 1.0f)).xyz;

        gs_light_pos_x[group_idx] = position.x;
        gs_light_pos_y[group_idx] = position.y;
        gs_light_pos_z[group_idx] = position.z;
        gs_light_radius[group_idx] = light.radius;

        // Ensure all threads have caught up
        GroupMemoryBarrierWithGroupSync();

        uint per_batch_count = min(NUM_THREADS, num_point_lights - batch_offset);
        for (uint light_idx = 0; light_idx < per_batch_count; ++light_idx) {
            float4 light_sphere = float4(gs_light_pos_x[light_idx], gs_light_pos_y[light_idx], gs_light_pos_z[light_idx], gs_light_radius[light_idx]);
            
            if (
                local_visible_count < MAX_LIGHTS_PER_CLUSTER
                && test_AABB_vs_sphere(min_corner, max_corner, light_sphere)
            ) {
                visible_lights[local_visible_count++] = batch_offset + light_idx;
            }
        }

        GroupMemoryBarrierWithGroupSync();
    }

    // So at this point each thread should have completed looping through the lights
    // Now we need to:
    // 1. Get an offset into our global light indices list
    // 2. Add the entries in visible_lights into our global light indices list
    // 3. Store the offset + count in our per cluster buffer

    RWByteAddressBuffer indices_list = write_buffers_table[point_light_indices_list_idx];
    
    uint linear_cluster_idx = get_linear_cluster_idx(cluster_setup, dispatch_id);
    uint offset = linear_cluster_idx * (MAX_LIGHTS_PER_CLUSTER + 1);
    // Store count inline
    indices_list.Store(offset * sizeof(uint), local_visible_count);

    for (uint i = 0; i < local_visible_count; i++) {
        uint idx = offset + i + 1; // 1 is to skip inline count
        indices_list.Store(idx * sizeof(uint), visible_lights[i]);
    }
}
