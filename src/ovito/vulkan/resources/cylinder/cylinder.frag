#version 440

#include "../global_uniforms.glsl"
#include "../shading.glsl"

// Push constants:
layout(push_constant) uniform constants_fragment {
	// mat4 mvp; -> used in the vertex shader
    // layout(row_major) mat4x3 modelview_matrix; -> used in the vertex shader
    layout(offset = 112) vec2 color_range;
} PushConstants;

// Tabulated color map:
layout(std140, set = 1, binding = 0) uniform ColorMapObject {
    vec4 table[256];
} ColorMap;

// Inputs:
layout(location = 0) flat in vec4 color1_fs;
layout(location = 1) flat in vec4 color2_fs;
layout(location = 2) flat in vec3 cylinder_view_base;		// Transformed cylinder position in view coordinates
layout(location = 3) flat in vec3 cylinder_view_axis;		// Transformed cylinder axis in view coordinates
layout(location = 4) flat in float cylinder_radius_sq_fs;	// The squared radius of the cylinder
layout(location = 5) flat in float cylinder_length;			// The length of the cylinder
layout(location = 6) noperspective in vec3 ray_origin;
layout(location = 7) noperspective in vec3 ray_dir;

// Outputs:
layout(location = 0) out vec4 fragColor;

void main()
{
	vec3 ray_dir_norm = normalize(ray_dir);

	// Perform ray-cylinder intersection test.
	vec3 n = cross(ray_dir_norm, cylinder_view_axis);
	float ln = length(n);
	vec3 RC = ray_origin - cylinder_view_base;

	vec3 view_intersection_pnt = ray_origin;
	vec3 surface_normal;

	bool skip = false;
	float x;	// Normalized location along cylinder (used for color interpolation). 

	if(ln < 1e-7 * cylinder_length) {
		// Handle case where view ray is parallel to cylinder axis:

		float t = dot(RC, ray_dir_norm);
		float v = dot(RC, RC);
		if(v-t*t > cylinder_radius_sq_fs) {
			discard;
		}
		else {
			view_intersection_pnt -= t * ray_dir_norm;
			surface_normal = -cylinder_view_axis;
			float tfar = dot(cylinder_view_axis, ray_dir_norm);
			if(tfar < 0.0) {
				view_intersection_pnt += tfar * ray_dir_norm;
				surface_normal = cylinder_view_axis;
				x = 1.0;
			}
			else x = 0.0;
		}
	}
	else {

		n /= ln;
		float d = dot(RC,n);
		d *= d;

		// Test if ray missed the cylinder.
		if(d > cylinder_radius_sq_fs) {
			discard;
		}
		else {

			// Calculate closest intersection position.
			float t = dot(cross(cylinder_view_axis, RC), n) / ln;
			float s = abs(sqrt(cylinder_radius_sq_fs - d) / dot(cross(n, cylinder_view_axis), ray_dir_norm) * cylinder_length);
			float tnear = t - s;

			// Calculate intersection point in view coordinate system.
			view_intersection_pnt += tnear * ray_dir_norm;

			// Find intersection position along cylinder axis.
			float anear = dot(view_intersection_pnt - cylinder_view_base, cylinder_view_axis) / (cylinder_length*cylinder_length);
			if(anear >= 0.0 && anear <= 1.0) {

				// Calculate surface normal in view coordinate system.
				surface_normal = (view_intersection_pnt - (cylinder_view_base + anear * cylinder_view_axis));
				x = anear;
			}
			else {
				// Calculate second intersection point.
				float tfar = t + s;
				vec3 far_view_intersection_pnt = ray_origin + tfar * ray_dir_norm;
				float afar = dot(far_view_intersection_pnt - cylinder_view_base, cylinder_view_axis) / (cylinder_length*cylinder_length);

				// Compute intersection with cylinder caps.
				if(anear < 0.0 && afar > 0.0) {
					view_intersection_pnt += (anear / (anear - afar) * 2.0 * s + 1e-6 * ln) * ray_dir_norm;
					surface_normal = -cylinder_view_axis;
					x = 0.0;
				}
				else if(anear > 1.0 && afar < 1.0) {
					view_intersection_pnt += ((anear - 1.0) / (anear - afar) * 2.0 * s + 1e-6 * ln) * ray_dir_norm;
					surface_normal = cylinder_view_axis;
					x = 1.0;
				}
				else {
					discard;
				}
			}
		}
	}

	// Output the ray-cylinder intersection point as the fragment depth
	// rather than the depth of the bounding box polygons.
	// The eye coordinate Z value must be transformed to normalized device
	// coordinates before being assigned as the final fragment depth.
	vec4 projected_intersection = GlobalUniforms.projection_matrix * vec4(view_intersection_pnt, 1.0);
	gl_FragDepth = projected_intersection.z / projected_intersection.w;

	// Perform linear interpolation of color.
	vec4 color = mix(color1_fs, color2_fs, x);

	// If pseudocolor mapping is used, apply tabulated transfer function to pseudocolor value,
	// which is stored in the R component of the input color.
	if(PushConstants.color_range.x != PushConstants.color_range.y) {
		// Compute normalized pseudocolor value.
		float pseudocolor_value = (color.r - PushConstants.color_range.x) / (PushConstants.color_range.y - PushConstants.color_range.x);
        // Compute index into color lookup table.
        int index = int(clamp(pseudocolor_value * 256.0, 0.0, 255.0));
		// Replace RGB value.
		color.xyz = ColorMap.table[index].xyz;
	}

	// Perform surface shading.
	fragColor = shadeSurfaceColor(normalize(surface_normal), color);
}
