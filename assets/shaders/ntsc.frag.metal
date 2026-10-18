#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Ntsc
{
    float4 w[60];
    float4 m[3];
    float2 region_origin;
    uint black_padding;
    uint force_mono;
};

struct main0_out
{
    float4 _entryPointOutput_o_color [[color(0)]];
};

fragment main0_out main0(constant Ntsc& _102 [[buffer(0)]], texture2d<float> u_bits [[texture(0)]], float4 gl_FragCoord [[position]])
{
    main0_out out = {};
    float4 _739;
    do
    {
        int _381 = int(floor(gl_FragCoord.x - _102.region_origin.x));
        int _388 = int(floor(gl_FragCoord.y - _102.region_origin.y));
        uint4 _530 = uint4(rint(u_bits.read(uint2(int3(18, _388, 0).xy), 0) * 255.0));
        uint _559 = ((_530.x | (_530.y << uint(8))) | (_530.z << uint(16))) | (_530.w << uint(24));
        int _400 = int((_559 >> uint(16)) & 255u);
        if ((_102.force_mono != 0u) || ((_559 & 255u) == 0u))
        {
            int _409 = _381 >> 5;
            uint _738;
            do
            {
                if ((_409 < 0) || (_409 >= 18))
                {
                    _738 = 0u;
                    break;
                }
                uint4 _579 = uint4(rint(u_bits.read(uint2(int3(_409, _388, 0).xy), 0) * 255.0));
                _738 = ((_579.x | (_579.y << uint(8))) | (_579.z << uint(16))) | (_579.w << uint(24));
                break;
            } while(false);
            float _419 = float(((_738 >> (uint(_381) & 31u)) & 1u) != 0u);
            _739 = float4(_419, _419, _419, 1.0);
            break;
        }
        if ((_102.black_padding != 0u) && ((_381 < _400) || (_381 >= (_400 + 560))))
        {
            _739 = float4(0.0, 0.0, 0.0, 1.0);
            break;
        }
        int _442 = _381 - 7;
        int _444 = _442 >> 5;
        uint _733;
        do
        {
            if ((_444 < 0) || (_444 >= 18))
            {
                _733 = 0u;
                break;
            }
            uint4 _628 = uint4(rint(u_bits.read(uint2(int3(_444, _388, 0).xy), 0) * 255.0));
            _733 = ((_628.x | (_628.y << uint(8))) | (_628.z << uint(16))) | (_628.w << uint(24));
            break;
        } while(false);
        int _449 = _444 + 1;
        uint _734;
        do
        {
            if ((_449 < 0) || (_449 >= 18))
            {
                _734 = 0u;
                break;
            }
            uint4 _677 = uint4(rint(u_bits.read(uint2(int3(_449, _388, 0).xy), 0) * 255.0));
            _734 = ((_677.x | (_677.y << uint(8))) | (_677.z << uint(16))) | (_677.w << uint(24));
            break;
        } while(false);
        int _455 = _442 - (_444 * 32);
        int _461 = ((_381 - _400) + int((_559 >> uint(8)) & 255u)) & 3;
        float3 _740;
        if (((((_455 < 32) ? _733 : _734) >> (uint(_455) & 31u)) & 1u) != 0u)
        {
            _740 = _102.w[_461 * 15].xyz;
        }
        else
        {
            _740 = float3(0.0);
        }
        int _751 = _455 + 1;
        float3 _767;
        if (((((_751 < 32) ? _733 : _734) >> (uint(_751) & 31u)) & 1u) != 0u)
        {
            _767 = _740 + _102.w[(_461 * 15) + 1].xyz;
        }
        else
        {
            _767 = _740;
        }
        int _776 = _455 + 2;
        float3 _792;
        if (((((_776 < 32) ? _733 : _734) >> (uint(_776) & 31u)) & 1u) != 0u)
        {
            _792 = _767 + _102.w[(_461 * 15) + 2].xyz;
        }
        else
        {
            _792 = _767;
        }
        int _801 = _455 + 3;
        float3 _817;
        if (((((_801 < 32) ? _733 : _734) >> (uint(_801) & 31u)) & 1u) != 0u)
        {
            _817 = _792 + _102.w[(_461 * 15) + 3].xyz;
        }
        else
        {
            _817 = _792;
        }
        int _826 = _455 + 4;
        float3 _842;
        if (((((_826 < 32) ? _733 : _734) >> (uint(_826) & 31u)) & 1u) != 0u)
        {
            _842 = _817 + _102.w[(_461 * 15) + 4].xyz;
        }
        else
        {
            _842 = _817;
        }
        int _851 = _455 + 5;
        float3 _867;
        if (((((_851 < 32) ? _733 : _734) >> (uint(_851) & 31u)) & 1u) != 0u)
        {
            _867 = _842 + _102.w[(_461 * 15) + 5].xyz;
        }
        else
        {
            _867 = _842;
        }
        int _876 = _455 + 6;
        float3 _892;
        if (((((_876 < 32) ? _733 : _734) >> (uint(_876) & 31u)) & 1u) != 0u)
        {
            _892 = _867 + _102.w[(_461 * 15) + 6].xyz;
        }
        else
        {
            _892 = _867;
        }
        int _901 = _455 + 7;
        float3 _917;
        if (((((_901 < 32) ? _733 : _734) >> (uint(_901) & 31u)) & 1u) != 0u)
        {
            _917 = _892 + _102.w[(_461 * 15) + 7].xyz;
        }
        else
        {
            _917 = _892;
        }
        int _926 = _455 + 8;
        float3 _942;
        if (((((_926 < 32) ? _733 : _734) >> (uint(_926) & 31u)) & 1u) != 0u)
        {
            _942 = _917 + _102.w[(_461 * 15) + 8].xyz;
        }
        else
        {
            _942 = _917;
        }
        int _951 = _455 + 9;
        float3 _967;
        if (((((_951 < 32) ? _733 : _734) >> (uint(_951) & 31u)) & 1u) != 0u)
        {
            _967 = _942 + _102.w[(_461 * 15) + 9].xyz;
        }
        else
        {
            _967 = _942;
        }
        int _976 = _455 + 10;
        float3 _992;
        if (((((_976 < 32) ? _733 : _734) >> (uint(_976) & 31u)) & 1u) != 0u)
        {
            _992 = _967 + _102.w[(_461 * 15) + 10].xyz;
        }
        else
        {
            _992 = _967;
        }
        int _1001 = _455 + 11;
        float3 _1017;
        if (((((_1001 < 32) ? _733 : _734) >> (uint(_1001) & 31u)) & 1u) != 0u)
        {
            _1017 = _992 + _102.w[(_461 * 15) + 11].xyz;
        }
        else
        {
            _1017 = _992;
        }
        int _1026 = _455 + 12;
        float3 _1042;
        if (((((_1026 < 32) ? _733 : _734) >> (uint(_1026) & 31u)) & 1u) != 0u)
        {
            _1042 = _1017 + _102.w[(_461 * 15) + 12].xyz;
        }
        else
        {
            _1042 = _1017;
        }
        int _1051 = _455 + 13;
        float3 _1067;
        if (((((_1051 < 32) ? _733 : _734) >> (uint(_1051) & 31u)) & 1u) != 0u)
        {
            _1067 = _1042 + _102.w[(_461 * 15) + 13].xyz;
        }
        else
        {
            _1067 = _1042;
        }
        int _1076 = _455 + 14;
        float3 _1092;
        if (((((_1076 < 32) ? _733 : _734) >> (uint(_1076) & 31u)) & 1u) != 0u)
        {
            _1092 = _1067 + _102.w[(_461 * 15) + 14].xyz;
        }
        else
        {
            _1092 = _1067;
        }
        _739 = float4(floor(fast::clamp(float3(dot(_102.m[0].xyz, _1092), dot(_102.m[1].xyz, _1092), dot(_102.m[2].xyz, _1092)), float3(0.0), float3(1.0)) * 255.0) * float3(0.0039215688593685626983642578125), 1.0);
        break;
    } while(false);
    out._entryPointOutput_o_color = _739;
    return out;
}

