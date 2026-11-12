#version 450
// Second Sight VGA LCD panel scaler for Vulkan: the same as lcd.frag.metal,
// which describes the texture layout. SDL's fragment resource sets for SPIR-V:
// sampled textures in set 2, uniform buffers in set 3.
// Build: glslangValidator -V -S frag lcd.frag.glsl -o lcd.frag.spv

layout(set = 2, binding = 0) uniform sampler2D u_source;
layout(set = 3, binding = 0, std140) uniform LcdPanel
{
    ivec4 tables;
} u;

layout(location = 0) out vec4 o_color;

float bytes2(vec2 b)
{
    return round(b.x * 255.0) + 256.0 * round(b.y * 255.0);
}

void main()
{
    const int ox = int(gl_FragCoord.x), oy = int(gl_FragCoord.y);
    const int vx = int(bytes2(texelFetch(u_source, ivec2(ox, u.tables.x), 0).ba));
    const int vy = int(bytes2(texelFetch(u_source, ivec2(oy, u.tables.y), 0).ba));
    const int first_x = vx & 4095, count_x = vx >> 12;
    const int first_y = vy & 4095, count_y = vy >> 12;
    float wx[15];
    for (int i = 0; i < count_x; ++i)
        wx[i] = (bytes2(texelFetch(u_source, ivec2(ox, u.tables.x + i), 0).rg) - 32768.0) / 4096.0;
    vec3 sum = vec3(0.0);
    for (int j = 0; j < count_y; ++j) {
        const float wy = (bytes2(texelFetch(u_source, ivec2(oy, u.tables.y + j), 0).rg) - 32768.0) / 4096.0;
        vec3 row = vec3(0.0);
        for (int i = 0; i < count_x; ++i)
            row += wx[i] * texelFetch(u_source, ivec2(first_x + i, first_y + j), 0).bgr;
        sum += wy * row;
    }
    o_color = vec4(clamp(sum, 0.0, 1.0), 1.0);
}
