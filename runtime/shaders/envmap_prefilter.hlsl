cbuffer pass_constants : register(b0)
{
    uint src_texture_idx;
    uint out_texture_initial_idx;
    uint mip_idx;
    uint mip_width;
};

TextureCube texCube_table[4096] : register(t0, space1);
RWTexture2DArray<half4> rw_tex2D_array_table[4096] : register(u0, space2);

SamplerState cube_sampler : register(s0);


float3 to_world_coords(uint3 globalId, uint size)
{
    float2 uvc = (float2(globalId.xy) + 0.5) / float(size);
    uvc = 2.0 * uvc - 1.0;
    uvc.y *= -1.0;
    switch (globalId.z) {
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

[numthreads(8, 8, 6)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
) {
    TextureCube src_texture = texCube_table[src_texture_idx];
    RWTexture2DArray<half4> out_texture = rw_tex2D_array_table[out_texture_initial_idx + mip_idx];

    float3 dir = normalize(to_world_coords(dispatch_id, mip_width));

    if (dispatch_id.x < mip_width || dispatch_id.y < mip_width) {
        float4 color = src_texture.SampleLevel(cube_sampler, dir, float(mip_idx));
        out_texture[dispatch_id] = half4(color);
    }
}
