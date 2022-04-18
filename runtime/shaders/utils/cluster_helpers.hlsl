static const uint MAX_LIGHTS_PER_CLUSTER = 127;

struct ClusterSetup {
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
};

void calculate_cluster_AABB(
    in uint3 cluster_idx,
    in ClusterSetup cluster_setup,
    out float3 min_corner,
    out float3 max_corner
) {
    // Calculate view space AABB
    float tan_theta = cluster_setup.y_near / -cluster_setup.z_near;
    float cluster_near_z;
    float cluster_far_z;

    if (cluster_idx.z >= cluster_setup.pre_mid_depth) {
        // Ola Olsson 2012
        float h_0 = 2.0 * tan_theta / float(cluster_setup.grid_height);
        uint k = cluster_idx.z - cluster_setup.pre_mid_depth;
        cluster_near_z = cluster_setup.mid_plane * pow(1.0 + h_0, float(k));
        cluster_far_z = cluster_near_z * (1.0 + h_0);
    } else {
        // linear
        float dz = (cluster_setup.mid_plane - cluster_setup.z_near) / float(cluster_setup.pre_mid_depth);
        cluster_near_z = dz * float(cluster_idx.z) + cluster_setup.z_near;
        cluster_far_z = dz * float(cluster_idx.z + 1) + cluster_setup.z_near;
    }
    
    max_corner.z = cluster_near_z;
    min_corner.z = cluster_far_z;

    float2 delta_XY = 2.0 / float2(cluster_setup.grid_width, cluster_setup.grid_height);
    float2 left_bottom_ndc_xy = (float2(cluster_idx.xy) * delta_XY - 1.0);
    float2 right_top_ndc_xy = (float2(cluster_idx.xy + 1) * delta_XY - 1.0);
    
    float a = cluster_setup.x_near / cluster_setup.z_near;
    min_corner.x = a * left_bottom_ndc_xy.x;
    max_corner.x = a * right_top_ndc_xy.x;

    if (left_bottom_ndc_xy.x < 0.0) {
        // We're on the left size of the frustum, so the left, far side will be min
        // And the near right side will be max
        min_corner.x *= cluster_far_z;
        max_corner.x *= cluster_near_z;
    } else {
        min_corner.x *= cluster_near_z;
        max_corner.x *= cluster_far_z;
    }

    min_corner.y = -tan_theta * left_bottom_ndc_xy.y;
    max_corner.y = -tan_theta * right_top_ndc_xy.y;
    if (left_bottom_ndc_xy.y < 0.0) {
        min_corner.y *= cluster_far_z;
        max_corner.y *= cluster_near_z;
    } else {
        min_corner.y *= cluster_near_z;
        max_corner.y *= cluster_far_z;
    }
}

uint3 calculate_cluster_index(in ClusterSetup cluster_setup, in float3 position_ndc, in float depth_view_space)
{
    uint k;

    if (depth_view_space <= cluster_setup.mid_plane) {
        float tan_fov = (cluster_setup.y_near / -cluster_setup.z_near);
        // Beyond mid plane, switch to cubic partitioning
        // Ola Olsson
        k = cluster_setup.pre_mid_depth + uint(log2(depth_view_space / cluster_setup.mid_plane) / log2(1.0 + 2.0 * tan_fov / cluster_setup.grid_height));
    } else {
        //Linear depth partitioning
        // linear
        k = uint((depth_view_space - cluster_setup.z_near) / (cluster_setup.mid_plane - cluster_setup.z_near) * float(cluster_setup.pre_mid_depth));
    }

    uint3 cluster_idx = uint3(0.5 * (position_ndc.xy + 1.0) * float2(cluster_setup.grid_width, cluster_setup.grid_height), k);

    return cluster_idx;
}

uint get_linear_cluster_idx(in ClusterSetup cluster_setup, uint3 cluster_idx)
{
    return cluster_idx.x + cluster_idx.y * (cluster_setup.grid_width) + cluster_idx.z * (cluster_setup.grid_width * cluster_setup.grid_height);
}
