#pragma pack_matrix( row_major )

#include "light_helpers.hlsl"
#include "cluster_helpers.hlsl"

static const uint VIZ_UPPER_BOUND = 32;

//=================================================================================================
// Bindings
//=================================================================================================

struct VSConstants
{
    float4x4 model;
    row_major float3x4 normal;
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
    uint num_point_lights;
    uint spot_light_buffer_idx;
    uint point_light_buffer_idx;
};

cbuffer clustered_lighting_constants : register(b4)
{
    ClusterSetup cluster_setup;
    uint spot_light_indices_list_idx;
    uint point_light_indices_list_idx;
};

ByteAddressBuffer buffers_table[4096] : register(t0, space1);

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

// Taken from
// https://www.shadertoy.com/view/WlfXRN
float3 viridis(float t) {

    const float3 c0 = float3(0.2777273272234177, 0.005407344544966578, 0.3340998053353061);
    const float3 c1 = float3(0.1050930431085774, 1.404613529898575, 1.384590162594685);
    const float3 c2 = float3(-0.3308618287255563, 0.214847559468213, 0.09509516302823659);
    const float3 c3 = float3(-4.634230498983486, -5.799100973351585, -19.33244095627987);
    const float3 c4 = float3(6.228269936347081, 14.17993336680509, 56.69055260068105);
    const float3 c5 = float3(4.776384997670288, -13.74514537774601, -65.35303263337234);
    const float3 c6 = float3(-5.435455855934631, 4.645852612178535, 26.3124352495832);

    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}

float3 plasma(float t) {

    const float3 c0 = float3(0.05873234392399702, 0.02333670892565664, 0.5433401826748754);
    const float3 c1 = float3(2.176514634195958, 0.2383834171260182, 0.7539604599784036);
    const float3 c2 = float3(-2.689460476458034, -7.455851135738909, 3.110799939717086);
    const float3 c3 = float3(6.130348345893603, 42.3461881477227, -28.51885465332158);
    const float3 c4 = float3(-11.10743619062271, -82.66631109428045, 60.13984767418263);
    const float3 c5 = float3(10.02306557647065, 71.41361770095349, -54.07218655560067);
    const float3 c6 = float3(-3.658713842777788, -22.93153465461149, 18.19190778539828);

    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));

}

float3 magma(float t) {

    const float3 c0 = float3(-0.002136485053939582, -0.000749655052795221, -0.005386127855323933);
    const float3 c1 = float3(0.2516605407371642, 0.6775232436837668, 2.494026599312351);
    const float3 c2 = float3(8.353717279216625, -3.577719514958484, 0.3144679030132573);
    const float3 c3 = float3(-27.66873308576866, 14.26473078096533, -13.64921318813922);
    const float3 c4 = float3(52.17613981234068, -27.94360607168351, 12.94416944238394);
    const float3 c5 = float3(-50.76852536473588, 29.04658282127291, 4.23415299384598);
    const float3 c6 = float3(18.65570506591883, -11.48977351997711, -5.601961508734096);

    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));

}

float3 inferno(float t) {

    const float3 c0 = float3(0.0002189403691192265, 0.001651004631001012, -0.01948089843709184);
    const float3 c1 = float3(0.1065134194856116, 0.5639564367884091, 3.932712388889277);
    const float3 c2 = float3(11.60249308247187, -3.972853965665698, -15.9423941062914);
    const float3 c3 = float3(-41.70399613139459, 17.43639888205313, 44.35414519872813);
    const float3 c4 = float3(77.162935699427, -33.40235894210092, -81.80730925738993);
    const float3 c5 = float3(-71.31942824499214, 32.62606426397723, 73.20951985803202);
    const float3 c6 = float3(25.13112622477341, -12.24266895238567, -23.07032500287172);

    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 position_ndc = input.position_clip.xyz /= input.position_clip.w;
    float depth_vs = -input.position_clip.w;
    
    uint3 cluster_idx = calculate_cluster_index(cluster_setup, position_ndc, depth_vs);
    uint linear_cluster_idx = get_linear_cluster_idx(cluster_setup, cluster_idx);
    uint light_offset = (MAX_LIGHTS_PER_CLUSTER + 1) * linear_cluster_idx;

    // Spot Lights
    ByteAddressBuffer spot_lights_buffer = buffers_table[spot_light_buffer_idx];
    ByteAddressBuffer spot_light_indices_buffer = buffers_table[spot_light_indices_list_idx];
    uint spot_light_count = spot_light_indices_buffer.Load<uint>(light_offset * sizeof(uint));

    // Point Lights
    ByteAddressBuffer point_lights_buffer = buffers_table[point_light_buffer_idx];
    ByteAddressBuffer point_light_indices_buffer = buffers_table[point_light_indices_list_idx];
    uint point_light_count = point_light_indices_buffer.Load<uint>(light_offset * sizeof(uint));

    float3 color = pow(viridis(smoothstep(0, VIZ_UPPER_BOUND, spot_light_count + point_light_count)), 2.2);
    
    return float4(color, 1.0);
}
