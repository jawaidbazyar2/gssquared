// Apple II composite (NTSC) and monochrome decoder.
//
// Input: the FrameBits packed dot stream as a 19x192 RGBA8 texture. Each texel
// is one little-endian uint32 word; texels 0..17 of a row hold that line's dots
// (dot c = bit (c & 31) of word (c >> 5)), texel 18 holds the line mode:
// byte 0 colorburst, byte 1 phase_offset, byte 2 line_start.
//
// The draw covers the display region of the 910x263 frame; region_origin is
// its top-left pixel. Each fragment decodes one output dot with the same math
// as g_hgr_LUT (see build_ntsc_gpu_weights in src/display/ntsc.cpp).
//
// Edit this file, then run scripts/compile_shaders_macos.sh to regenerate
// ntsc.frag.metal.

cbuffer Ntsc : register(b0, space3) {
    float4 w[60];          // [phase * 15 + tap] YIQ weight of a lit dot at tap - 7
    float4 m[3];           // YIQ -> RGB rows
    float2 region_origin;  // frame pixel of region column 0, line 0
    uint black_padding;    // colorburst lines: black outside [line_start, line_start + 560)
    uint force_mono;       // decode every line as white/black dots
};

Texture2D u_bits : register(t0, space2);
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

static const int NUM_TAPS = 7;
static const int DOT_WORDS = 18;
static const int DATA_DOTS = 560;

uint load_texel(int x, int y) {
    uint4 b = (uint4)round(u_bits.Load(int3(x, y, 0)) * 255.0);
    return b.x | (b.y << 8) | (b.z << 16) | (b.w << 24);
}

// Dot words outside the line read as zero (no dots).
uint load_word(int x, int y) {
    if (x < 0 || x >= DOT_WORDS) {
        return 0u;
    }
    return load_texel(x, y);
}

PSOutput main(PSInput input) {
    PSOutput output;

    int c = (int)floor(input.pos.x - region_origin.x);
    int y = (int)floor(input.pos.y - region_origin.y);

    uint mode = load_texel(DOT_WORDS, y);
    uint colorburst = mode & 0xFFu;
    int phase_offset = (int)((mode >> 8) & 0xFFu);
    int line_start = (int)((mode >> 16) & 0xFFu);

    if (force_mono != 0u || colorburst == 0u) {
        uint word = load_word(c >> 5, y);
        float v = ((word >> (uint(c) & 31u)) & 1u) != 0u ? 1.0 : 0.0;
        output.o_color = float4(v, v, v, 1.0);
        return output;
    }

    if (black_padding != 0u && (c < line_start || c >= line_start + DATA_DOTS)) {
        output.o_color = float4(0.0, 0.0, 0.0, 1.0);
        return output;
    }

    // Taps c-7 .. c+7 span at most two words.
    int first = c - NUM_TAPS;
    int w0 = first >> 5;
    uint lo = load_word(w0, y);
    uint hi = load_word(w0 + 1, y);
    int bit = first - w0 * 32;

    int p = (c - line_start + phase_offset) & 3;
    float3 yiq = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int k = 0; k <= 2 * NUM_TAPS; k++) {
        int b = bit + k;
        uint word = b < 32 ? lo : hi;
        if (((word >> (uint(b) & 31u)) & 1u) != 0u) {
            yiq += w[p * 15 + k].xyz;
        }
    }

    float3 rgb = float3(dot(m[0].xyz, yiq), dot(m[1].xyz, yiq), dot(m[2].xyz, yiq));
    rgb = floor(saturate(rgb) * 255.0) / 255.0;
    output.o_color = float4(rgb, 1.0);
    return output;
}
