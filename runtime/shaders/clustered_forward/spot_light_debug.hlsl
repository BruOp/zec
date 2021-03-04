#pragma pack_matrix( row_major )


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

cbuffer view_constants_buffer : register(b0)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};

cbuffer scene_constants_buffer : register(b1)
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

struct PSInput
{
    float4 sv_position : SV_POSITION;
};

float4 quaternion_from_unit_vectors(float3 v1, float3 v2) {
    float4 quat;
    float V1dotV2 = dot(v1, v2);
    if (V1dotV2 < 0.0001) {
        quat.w = 0;
        quat.xyz = v1;
    } else {
        quat.w = 1.0 + V1dotV2;
        quat.xyz = cross(v1, v2);
    }
    return normalize(quat);
}

float4 quaternion_mul(float4 q1, float4 q2)
{
    return float4(
        q2.xyz * q1.w + q1.xyz * q2.w + cross(q1.xyz, q2.xyz),
        q1.w * q2.w - dot(q1.xyz, q2.xyz)
    );
}

PSInput VSMain(uint vert_idx : SV_VERTEXID, uint instance_id : SV_INSTANCEID)
{
    SpotLight spot_light = buffers_table[spot_light_buffer_idx].Load<SpotLight>(instance_id * sizeof(SpotLight));
    
    float4 vert_position;
        vert_position.w = 1.0;

    if (vert_idx == 0) {
        vert_position = float4(spot_light.position, 1.0);
    } else {
        float3 world_up = abs(spot_light.direction.y) > 0.9 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        float3 right = normalize(cross(spot_light.direction, world_up));
        float3 up = normalize(cross(spot_light.direction, right));

        // How far the bounding frustum will extend in any direction
        float cap_extent = spot_light.radius * tan(spot_light.umbra_angle);
        float right_extent = vert_idx & 1 ? cap_extent : -cap_extent;
        float up_extent = vert_idx > 2 ? -cap_extent : cap_extent;
        // Translate the vertex to one of the four corners of our frustum
        vert_position.xyz = spot_light.direction * spot_light.radius + right_extent * right + up_extent * up;

        vert_position.xyz += spot_light.position;
    }
    
    
    PSInput res;
    res.sv_position = mul(VP, vert_position);
    return res;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
