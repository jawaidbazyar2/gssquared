#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_Context
{
    float2 resolution;
};

struct main0_out
{
    float4 out_var_SV_Target [[color(0)]];
};

struct main0_in
{
    float4 in_var_COLOR0 [[user(locn0)]];
    float2 in_var_TEXCOORD0 [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant type_Context& Context [[buffer(0)]], texture2d<float> u_texture [[texture(0)]], sampler u_sampler [[sampler(0)]])
{
    main0_out out = {};
    float2 _49 = in.in_var_TEXCOORD0 - float2(0.5);
    float2 _53 = in.in_var_TEXCOORD0 + (_49 * (dot(_49, _49) * 0.0500000007450580596923828125));
    float4 _75 = u_texture.sample(u_sampler, in.in_var_TEXCOORD0) * in.in_var_COLOR0;
    float2 _80 = _53 * (float2(1.0) - _53);
    float3 _88 = (_75.xyz * (mix(1.0, (sin((_53.y * Context.resolution.y) * 3.1415927410125732421875) * 0.5) + 0.5, 0.25) * mix(1.0, (fmod(_53.x * Context.resolution.x, 3.0) < 1.5) ? 0.949999988079071044921875 : 1.0499999523162841796875, 0.02500000037252902984619140625))).xyz * (mix(1.0, (_80.x * _80.y) * 15.0, 0.3499999940395355224609375) * 1.2000000476837158203125);
    out.out_var_SV_Target = float4(_88.x, _88.y, _88.z, _75.w);
    return out;
}

