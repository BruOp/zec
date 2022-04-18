#pragma pack_matrix( row_major )

cbuffer cloud_pass_constants : register(b0)
{
    uint cloud_shape_texture_idx;
    uint cloud_erosion_texture_idx;
}

cbuffer view_constants_buffer : register(b1)
{
    float4x4 VP;
    float4x4 invVP;
    float4x4 view;
    float3 camera_pos;
};

SamplerState sampler_3d : register(s0);

Texture3D tex3D_table[4096] : register(t0, space1);
// Texture2D tex2D_table[4096] : register(t0, space2);

static const float2 CLOUD_MIN_MAX = float2(1500.0, 4000.0);
static const float EARTH_RADIUS = 6371000.0;
static const float MAX_DISTANCE = 4096;
static const uint MAX_STEPS = 128;
static const uint MIN_STEPS = 32;
static const float SCALE = 120000;

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float4 direction_ws : DIRECTION;
    float2 uv : TEXCOORD;
};

// Modified version of intersection from https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
// Used to test against the top and bottom of our cloud layer in the atmosphere
// Returns a float2 representing the intersection
float4 sphIntersect2( in float3 ray_origin, in float3 ray_direction, in float2 height_min_max, in float earth_radius)
{
    float2 rtest_min_max = earth_radius + height_min_max;
	float3 oc = ray_origin + float3(0.0, earth_radius, 0.0);
	float b = dot( oc, ray_direction );
	float2 c = dot( oc, oc ) - rtest_min_max * rtest_min_max;
	float2 h = b * b - c;
    float2 mask = h >= 0.0;
    h = sqrt( h );
    return float4(mask, mask) * float4( -b-h, -b+h ) + float4(mask, mask) * (1.0).xxxx;
}

float get_height_fraction_for_point(in float3 position, in float2 cloud_min_max)
{
    float height_fraction = (position.y - cloud_min_max.x)  / (cloud_min_max.y - cloud_min_max.x);
    return saturate(height_fraction);
}

float get_density_height_gradient_for_point(in float3 position, in float2 cloud_min_max)
{
    float height_gradient = get_height_fraction_for_point(position, cloud_min_max);
    return height_gradient * height_gradient;
}

// the remap function used in the shaders as described in Gpu Pro 7
float remap(in float originalValue, in float originalMin, in float originalMax, in float newMin, in float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}


float sample_cloud_density(in Texture3D shape_texture, in float3 position)
{
    float height_fraction = get_height_fraction_for_point(position, CLOUD_MIN_MAX);
    float3 uvw = float3(position.xz / SCALE, height_fraction);
    float4 noise = shape_texture.SampleLevel(sampler_3d, uvw, 0.0f);

    // pack the channels for direct usage in shader
    float lowFreqFBM = dot(noise.gba, float3(0.625f, 0.25f, 0.125f));
    float baseCloud = noise.r;
    float density = remap(baseCloud, (1.0f - lowFreqFBM), 1.0f, 0.0f, 1.0f);
    // Saturate
    return saturate(density);

    // TODO: add density height gradient
    // float density_height_gradient = get_density_height_gradient_for_point(position, CLOUD_MIN_MAX);    
}

float sample_cloud_density_full(in Texture3D shape_texture, in Texture3D erosion_texture, in float3 position)
{
    // TODO: Move this to our constant buffer
    const float3 light_dir = float3(0.0, -1.0, 0.0);

    float density = sample_cloud_density(shape_texture, position);
    return density;
}

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position_cs = float4(position.x, position.y, 0.0, 1.0);

    result.direction_ws = mul(invVP, result.position_cs);
    result.direction_ws /= result.direction_ws.w;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    Texture3D cloud_shape_texture = tex3D_table[cloud_shape_texture_idx];
    Texture3D cloud_erosion_texture = tex3D_table[cloud_erosion_texture_idx];

    float3 ray_dir = normalize(input.direction_ws.xyz);

    float start_t;
    float end_t;
    float4 color = 0.0.xxxx;
    if (ray_dir.y == 0.0) {
        start_t = 0.0;
        end_t = MAX_DISTANCE;
    }
    else {
        float2 t = (CLOUD_MIN_MAX - camera_pos.y) / ray_dir.y;
        start_t = min(t.x, t.y);
        end_t = max(t.x, t.y);
    }
    start_t = max(start_t, 0.0); // Don't want to start from "behind" our origin
    
    if (end_t <= 0.0) {
        return color;
    }
    
    // end_t = clamp(end_t, start_t, start_t + MAX_DISTANCE); // End must be greater than or equal to start
    float3 start_position = camera_pos + ray_dir * start_t;

    // float density = sample_cloud_density_full(cloud_shape_texture, cloud_erosion_texture, start_position);
    // float distance_falloff = min(1.0, CLOUD_MIN_MAX.x / (distance(camera_pos, start_position) + 1.0));
    // return float4(1.0.xxx, density * density);

    float density = 0.0;
    float dt = (CLOUD_MIN_MAX.y - CLOUD_MIN_MAX.x) / MIN_STEPS;
    float3 sample_pos = start_position;
    float3 projection = ray_dir / ray_dir.y;
    for (uint i = 0; i < MIN_STEPS; ++i)
    {
        float sampled_density = sample_cloud_density_full(cloud_shape_texture, cloud_erosion_texture, sample_pos);
        density += sampled_density * sampled_density;
        sample_pos += projection * dt;
        // if (cloud_test > 0.0) {
        //     // Sample density the expensive way
        //     if (sample_density == 0.0)
        //     {
        //         zero_density_samples++;
        //     }
        // }
        // else {
            // Sample density the cheap way
            // cloud_test = sample_cloud_boundary(sample_pos);
        // }

        // Calculate Density
        // sample_cloud_density(cloud_shape_texture, input.direction_ws);

        // Lighting
        
        // Blending!
    }

    return float4(density.xxx, saturate(density));

    // return float4(density, density, density, density);
}
