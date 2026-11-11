// Second Sight VGA LCD panel scaler (src/devices/secondsight/ss_lcd_gpu.hpp).
// Each shown pixel is the sum of its column taps times its row taps over the
// monitor's samples, the same integer taps as ss_lcd::Panel on the CPU.
//
// u_source rows 0 to tables.x - 1 hold the samples (bytes B, G, R, A of each
// XRGB pixel, so .bgr is the color). Row tables.x + k holds tap k of each
// shown column, row tables.y + k tap k of each shown row: bytes 0-1 are the
// weight in 1/4096 plus 32768, bytes 2-3 the first sample plus 4096 times the
// tap count. Hand-written, like crt.frag.metal; lcd.frag.glsl is the Vulkan copy.

#include <metal_stdlib>

using namespace metal;

struct LcdPanel
{
    int4 tables;
};

struct main0_out
{
    float4 color [[color(0)]];
};

static float bytes2(float2 b)
{
    return rint(b.x * 255.0) + 256.0 * rint(b.y * 255.0);
}

fragment main0_out main0(constant LcdPanel& u [[buffer(0)]], texture2d<float> u_source [[texture(0)]],
                         float4 position [[position]])
{
    const int ox = int(position.x), oy = int(position.y);
    const int vx = int(bytes2(u_source.read(uint2(ox, u.tables.x)).ba));
    const int vy = int(bytes2(u_source.read(uint2(oy, u.tables.y)).ba));
    const int first_x = vx & 4095, count_x = vx >> 12;
    const int first_y = vy & 4095, count_y = vy >> 12;
    float wx[15];
    for (int i = 0; i < count_x; ++i)
        wx[i] = (bytes2(u_source.read(uint2(ox, u.tables.x + i)).rg) - 32768.0) / 4096.0;
    float3 sum = float3(0.0);
    for (int j = 0; j < count_y; ++j) {
        const float wy = (bytes2(u_source.read(uint2(oy, u.tables.y + j)).rg) - 32768.0) / 4096.0;
        float3 row = float3(0.0);
        for (int i = 0; i < count_x; ++i)
            row += wx[i] * u_source.read(uint2(first_x + i, first_y + j)).bgr;
        sum += wy * row;
    }
    main0_out out = {};
    out.color = float4(clamp(sum, 0.0, 1.0), 1.0);
    return out;
}
