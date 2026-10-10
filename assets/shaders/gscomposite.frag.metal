#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct GsComposite
{
    float4 b14[4];
    float4 b16[32];
    float4 f14[16];
    float4 f16[16];
    float4 m[3];
    float4 enc[3];
    uint4 lines[66];
    int2 src_size;
    uint force_mono;
    uint pad;
};

struct main0_out
{
    float4 _entryPointOutput_o_color [[color(0)]];
};

fragment main0_out main0(constant GsComposite& _51 [[buffer(0)]], texture2d<float> u_src [[texture(0)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float4 _517;
    do
    {
        int _317 = int(floor(gl_FragCoord.x));
        int _321 = int(floor(gl_FragCoord.y));
        int _323 = _321 >> 2;
        int _325 = _321 & 3;
        bool _337 = (_51.lines[_323][_325] & 2u) != 0u;
        if (!(((_51.lines[_323][_325] & 1u) != 0u) && (_51.force_mono == 0u)))
        {
            _517 = float4(fast::clamp(float3(dot(_51.enc[0].xyz, u_src.read(uint2(int3(_317, _321, 0).xy), 0).xyz)), float3(0.0), float3(1.0)), 1.0);
            break;
        }
        float3 _515;
        _515 = float3(0.0);
        float3 _518;
        for (int _514 = -15; _514 <= 15; _515 = _518, _514++)
        {
            int _375 = _317 + _514;
            if ((_375 < 0) || (_375 >= _51.src_size.x))
            {
                _518 = _515;
                continue;
            }
            float3 _395 = u_src.read(uint2(int3(_375, _321, 0).xy), 0).xyz;
            float2 _307 = select(_51.b14[_375 & 3].xy, _51.b16[_375 & 31].xy, bool2(_337));
            float _431 = (dot(_51.enc[0].xyz, _395) + (dot(_51.enc[1].xyz, _395) * _307.x)) + (dot(_51.enc[2].xyz, _395) * _307.y);
            _518 = _515 + (select(_51.f14[abs(_514)].xyz, _51.f16[abs(_514)].xyz, bool3(_337)) * float3(_431, _431 * _307.x, _431 * _307.y));
        }
        _517 = float4(fast::clamp(float3(dot(_51.m[0].xyz, _515), dot(_51.m[1].xyz, _515), dot(_51.m[2].xyz, _515)), float3(0.0), float3(1.0)), 1.0);
        break;
    } while(false);
    out._entryPointOutput_o_color = _517;
    return out;
}

