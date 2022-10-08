#version 440

// Push constants:
layout(push_constant) uniform constants_fragment {
	// mat4 mvp; -> used in the vertex shader
    // vec4 view_dir_eye_pos; -> used in the vertex shader
    layout(offset = 80) vec2 color_range;
} PushConstants;

// Tabulated color map:
layout(std140, set = 1, binding = 0) uniform ColorMapObject {
    vec4 table[256];
} ColorMap;

// Inputs:
layout(location = 0) in vec4 color_fs;

// Outputs:
layout(location = 0) out vec4 fragColor;

void main()
{
	// Interpolatated input color.
	vec4 color = color_fs;

	// If pseudocolor mapping is used, apply tabulated transfer function to pseudocolor value,
	// which is stored in the R component of the input color.
	if(PushConstants.color_range.x != PushConstants.color_range.y) {
		// Normalize pseudocolor value.
		float pseudocolor_value = (color.r - PushConstants.color_range.x) / (PushConstants.color_range.y - PushConstants.color_range.x);
        // Compute index into color lookup table.
        int index = int(clamp(pseudocolor_value * 256.0, 0.0, 255.0));
		// Replace RGB value.
		color.xyz = ColorMap.table[index].xyz;
	}

    // Flat shading:
    fragColor = color;
}
