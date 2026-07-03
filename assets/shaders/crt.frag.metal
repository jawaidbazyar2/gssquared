#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Context
{
    float2 resolution;
};

struct main0_out
{
    float4 _entryPointOutput_o_color [[color(0)]];
};

struct main0_in
{
    float4 input_v_color [[user(locn0)]];
    float2 input_v_uv [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], constant Context& _46 [[buffer(0)]], texture2d<float> u_texture [[texture(0)]], sampler u_sampler [[sampler(0)]])
{
    main0_out out = {};
    float2 _178 = in.input_v_uv - float2(0.5);
    float2 _187 = in.input_v_uv + (_178 * (dot(_178, _178) * 0.20000000298023223876953125));
    float4 _217 = u_texture.sample(u_sampler, in.input_v_uv) * in.input_v_color;
    float3 _223 = _217.xyz * (mix(1.0, (sin((_187.y * _46.resolution.y) * 3.1415927410125732421875) * 0.5) + 0.5, 0.25) * mix(1.0, (mod(_187.x * _46.resolution.x, 3.0) < 1.5) ? 0.949999988079071044921875 : 1.0499999523162841796875, 0.100000001490116119384765625));
    float4 _276 = _217;
    _276.x = _223.x;
    _276.y = _223.y;
    _276.z = _223.z;
    float2 _234 = _187 * (float2(1.0) - _187);
    float3 _247 = _276.xyz * (mix(1.0, (_234.x * _234.y) * 15.0, 0.070000000298023223876953125) * 1.2000000476837158203125);
    float4 _284 = _276;
    _284.x = _247.x;
    _284.y = _247.y;
    _284.z = _247.z;
    out._entryPointOutput_o_color = _284;
    return out;
}

