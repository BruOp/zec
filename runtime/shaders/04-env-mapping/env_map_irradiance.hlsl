static const float TWO_PI = 6.283185307179586;
static const float HALF_PI = 1.5707963267948966;
static const float PI = 3.141592653589793;
static const float NUM_SAMPLES_ALONG_AXIS = 64.0;

cbuffer pass_constants : register(b0)
{
    uint src_texture_idx;
    uint out_texture_idx;
};

TextureCube texCube_table[4096] : register(t0, space1);
RWTexture2DArray<half4> rw_tex2D_array_table[4096] : register(u0, space2);

SamplerState cube_sampler : register(s0);

float3 to_world_coords(uint3 globalId, uint size)
{
    float2 uvc = (float2(globalId.xy) + 0.5) / float(size);
    uvc = 2.0 * uvc - 1.0;
    uvc.y *= -1.0;
    switch (globalId.z)
    {
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

[numthreads(8, 8, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
)
{
    TextureCube src_texture = texCube_table[src_texture_idx];
    RWTexture2DArray<half4> out_texture = rw_tex2D_array_table[out_texture_idx];
    
    float2 src_dims;
    src_texture.GetDimensions(src_dims.x, src_dims.y);
    
    uint3 dims;
    out_texture.GetDimensions(dims.x, dims.y, dims.z);
    float3 N = normalize(to_world_coords(dispatch_id, dims.x));

    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    const float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float3 color = float3(0.0, 0.0, 0.0);
    uint total_weight = 0;
    float delta = 1.0 / NUM_SAMPLES_ALONG_AXIS;
    float mip_level = max(0.5 * log2((6.0 * src_dims.x * src_dims.y) / (4.0 * PI * NUM_SAMPLES_ALONG_AXIS * NUM_SAMPLES_ALONG_AXIS)), 0.0);
    for (float e1 = 0.0; e1 < 1.0f; e1 += delta)
    {
        for (float e2 = 0.0; e2 < 1.0f; e2 += delta)
        {
            float phi = TWO_PI * e2;
            float sin_theta = sqrt(1.0 - e1 * e1);
            float cos_theta = sqrt(1.0f - sin_theta * sin_theta);
            float3 dir = float3(sin_theta * cos(phi), sin_theta * sin(phi), e1);
            // Transform from tangent space into world space
            float3 sample_dir = right * dir.x + up * dir.y + N * dir.z;
            color += src_texture.SampleLevel(cube_sampler, sample_dir, mip_level).rgb * sin_theta * cos_theta;
            total_weight++;
        }
    }
    out_texture[dispatch_id] = half4(PI * color / float(total_weight), 1.0);
}
