
struct PointLight
{
    float3 position;
    float radius;
    float3 color;
    float padding;
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

// Unitless intensity, [0 - 1]
float incoming_light_intensity(in SpotLight spot_light, in float3 light_dir, in float distance, in float3 normal)
{
    float cos_umbra = cos(spot_light.umbra_angle);
    // Taken from RTR v4 p115
    float t = saturate((dot(light_dir, -spot_light.direction) - cos_umbra) / (cos(spot_light.penumbra_angle) - cos_umbra));
    float directonal_factor = t * t;
    
    // Taken from KARIS 2013
    float distance_factor = pow(saturate(1.0 - pow(distance / spot_light.radius, 4.0)), 2.0) / (distance * distance + 0.01);
    
    return directonal_factor * distance_factor * saturate(dot(normal, light_dir));
}

// Unitless intensity, [0 - 1]
float incoming_light_intensity(in PointLight point_light, in float3 light_dir, in float distance, in float3 normal)
{
    // Taken from KARIS 2013
    float distance_factor = pow(saturate(1.0 - pow(distance / point_light.radius, 4.0)), 2.0) / (distance * distance + 0.01);
    
    return distance_factor * saturate(dot(normal, light_dir));
}
