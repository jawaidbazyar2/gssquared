// Apple IIgs composite output: the VGC's RGB frame through a modeled
// RGB-to-composite encoder (the MC1377 role), then the same YIQ decoder as
// ntsc.frag.
//
// Input: the 910x263 RGB frame (frame_vsg), drawn 1:1 into a 910x263 target.
// Each source pixel is one composite sample: 4fsc on 14-dot lines, 16/14 x 4fsc
// on 16-dot (SHR) lines. Encode: s = Y + I sin(phi) + Q cos(phi), with
// (Y, I, Q) = enc * rgb. Decode: filter s * (1, sin(phi), cos(phi)) with the
// Y / I / Q taps of the line's sample rate, then m * yiq. Lines without
// colorburst (text, MONOCOLOR, Monochrome engine) show the encoder's luma of
// each pixel: on hardware those are solid grays with no dot pattern or blur.
//
// Edit this file, then run scripts/compile_shaders_macos.sh to regenerate
// gscomposite.frag.metal.

cbuffer GsComposite : register(b0, space3) {
    float4 b14[4];         // (sin, cos) of 14-dot sample phase (x & 3)
    float4 b16[32];        // (sin, cos) of 16-dot sample phase (x & 31)
    float4 f14[16];        // Y, I, Q taps by |offset|, 14-dot lines
    float4 f16[16];        // Y, I, Q taps by |offset|, 16-dot lines
    float4 m[3];           // YIQ -> RGB rows
    float4 enc[3];         // RGB -> YIQ rows (encoder)
    uint4 lines[66];       // line y in lines[y >> 2][y & 3]: bit 0 colorburst, bit 1 16-dot clock
    int2 src_size;         // source texture size in pixels
    uint force_mono;       // decode every line as luma
    uint pad;
};

Texture2D u_src : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

// SDL_GPU's vertex shaders remap COLOR0 to TEXCOORD0 and TEXCOORD0 to TEXCOORD1.
struct PSInput {
    float4 v_color : TEXCOORD0;
    float2 v_uv : TEXCOORD1;
    float4 pos : SV_Position;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

static const int NUM_TAPS = 15;  // GS_NUM_TAPS

PSOutput main(PSInput input) {
    PSOutput output;

    int x = (int)floor(input.pos.x);
    int y = (int)floor(input.pos.y);

    uint info = lines[y >> 2][y & 3];
    bool burst = (info & 1u) != 0u && force_mono == 0u;
    bool clock16 = (info & 2u) != 0u;

    if (!burst) {
        float l = dot(enc[0].xyz, u_src.Load(int3(x, y, 0)).rgb);
        output.o_color = float4(saturate(float3(l, l, l)), 1.0);
        return output;
    }

    float3 yiq = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int o = -NUM_TAPS; o <= NUM_TAPS; o++) {
        int sx = x + o;
        if (sx < 0 || sx >= src_size.x) {
            continue;
        }
        float3 rgb = u_src.Load(int3(sx, y, 0)).rgb;
        float2 sc = clock16 ? b16[sx & 31].xy : b14[sx & 3].xy;
        float s = dot(enc[0].xyz, rgb)
                + dot(enc[1].xyz, rgb) * sc.x
                + dot(enc[2].xyz, rgb) * sc.y;
        float3 f = clock16 ? f16[abs(o)].xyz : f14[abs(o)].xyz;
        yiq += f * float3(s, s * sc.x, s * sc.y);
    }

    float3 rgb_out = float3(dot(m[0].xyz, yiq), dot(m[1].xyz, yiq), dot(m[2].xyz, yiq));
    output.o_color = float4(saturate(rgb_out), 1.0);
    return output;
}
