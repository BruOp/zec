#pragma pack_matrix( row_major )

static const uint NUM_SAMPLES = 1024;
static const float PI = 3.141592653589793;

cbuffer pass_constants : register(b0)
{
    uint src_texture_idx;
    uint out_texture_initial_idx;
    uint mip_idx;
    uint img_width;
};

TextureCube texCube_table[4096] : register(t0, space1);
RWTexture2DArray<half4> rw_tex2D_array_table[4096] : register(u0, space2);

SamplerState cube_sampler : register(s0);

float3 to_world_coords(uint3 dispatch_id, uint size)
{
    float2 uvc = (float2(dispatch_id.xy) + 0.5) / float(size);
    uvc = 2.0 * uvc - 1.0;
    uvc.y *= -1.0;
    switch (dispatch_id.z) {
    case 0:
        return float3(1.0, uvc.y, -uvc.x);
    case 1:
        return float3(-1.0, uvc.y, uvc.x);
    case 2:
        return float3(uvc.x, 1.0, -uvc.y);
    case 3:
        return float3(uvc.x, -1.0, uvc.y);
    case 4:
        return float3(uvc.x, uvc.y, 1.0);
    default:
        return float3(-uvc.x, uvc.y, -1.0);
    }
}

float D_GGX(float NoH, float alpha)
{
    float a = NoH * alpha;
    float k = alpha / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

// Taken from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/genbrdflut.frag
// Based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, float numSamples) {
    uint bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return float2(i / numSamples, bits / exp2(32));
}

// Based on Karis 2014
float3 importance_sample_GGX(float2 Xi, float alpha, float3 N)
{
    // Sample in spherical coordinates
    float phi = 2.0 * PI * Xi.x;
    float cos_theta = sqrt((1 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    // Construct tangent space vector
    float3 H;
    H.x = sin_theta * cos(phi);
    H.y = sin_theta * sin(phi);
    H.z = cos_theta;

    // Tangent to world space
    float3 up_vector = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent_x = normalize(cross(up_vector, N));
    float3 tangent_y = cross(N, tangent_x);
    return tangent_x * H.x + tangent_y * H.y + N * H.z;
}

// From Karis, 2014
float3 prefilter_env_map(TextureCube src_texture, float roughness, float3 R, float img_size)
{
    // Isotropic approximation: we lose stretchy reflections :(
    float3 N = R;
    float3 V = R;

    float3 prefiltered_color = float3(0.0, 0.0, 0.0);
    float total_weight = 0.0;
    float mip_bias = 0.5 * log2(6 * img_size * img_size / NUM_SAMPLES / PI);
    float alpha = roughness * roughness;

    for (uint i = 0u; i < NUM_SAMPLES; i++) {
        float2 Xi = hammersley(i, NUM_SAMPLES);
        float3 H = importance_sample_GGX(Xi, alpha, N);
        float NoH = saturate(dot(N, H)); // Since N = V in our approximation
        // Use microfacet normal H to find L
        float3 L = 2 * dot(H, V) * H - V;
        float NoL = saturate(dot(N, L));

        if (NoL > 0.0) {
            // Based off https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
            // Typically you'd have the following:
            // float pdf = D_GGX(NoH, roughness) * NoH / (4.0 * VoH);
            // but since V = N => VoH == NoH
            // Solid angle of current sample -- bigger for less likely samples
            float mip_level = 1.0 + max(mip_bias - 0.5 * log2(D_GGX(NoH, alpha)), 0.0);
            prefiltered_color += src_texture.SampleLevel(cube_sampler, L, mip_level).rgb * NoL;
            total_weight += NoL;
        }
    }
    return prefiltered_color / total_weight;
}

[numthreads(8, 8, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
) {
    uint mip_width = img_width >> mip_idx;
    if (dispatch_id.x >= mip_width || dispatch_id.y >= mip_width) {
        return;
    }

    TextureCube src_texture = texCube_table[src_texture_idx];
    RWTexture2DArray<half4> out_texture = rw_tex2D_array_table[out_texture_initial_idx + mip_idx];

    float3 dir = normalize(to_world_coords(dispatch_id, mip_width));
    
    if (mip_idx == 0) {
        float4 color = src_texture.SampleLevel(cube_sampler, dir, float(mip_idx));
        out_texture[dispatch_id] = half4(color);
    } else {
        float roughness = float(mip_idx) / log2(img_width);
        out_texture[dispatch_id] = half4(prefilter_env_map(src_texture, roughness, dir, img_width), 1.0f);
    }
}
