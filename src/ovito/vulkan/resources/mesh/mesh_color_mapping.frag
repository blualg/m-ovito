#version 440

#include "../global_uniforms.glsl"
#include "../shading.glsl"

// Tabulated color map:
layout(std140, set = 1, binding = 0) uniform ColorMapObject {
    vec4 table[256];
} ColorMap;

// Inputs:
layout(location = 0) in vec2 pseudocolor_fs;
layout(location = 1) in vec3 normal_fs;

// Outputs:
layout(location = 0) out vec4 fragColor;

void main()
{
    if(pseudocolor_fs.y >= 0.0) {
        // Compute index into color lookup table.
        int index = int(clamp(pseudocolor_fs.x * 256.0, 0.0, 255.0));
        // Perform surface shading.
        fragColor = shadeSurfaceColor(normalize(normal_fs), vec4(ColorMap.table[index].xyz, pseudocolor_fs.y));
    }
    else {
        // It's a selected face. Use a red highlighting color to render it.
        fragColor = shadeSurfaceColor(normalize(normal_fs), vec4(1.0, 0.0, 0.0, 1.0));
    }
}
