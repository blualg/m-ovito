////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include "../../global_uniforms.glsl"
#include <shading.frag>

// Inputs:
flat in vec4 color_fs;
in vec2 uv_fs;
flat in vec2 radius_and_eyez_fs;

void main()
{
	// Test if fragment is within the unit circle.
	float rsq = dot(uv_fs, uv_fs);
	if(rsq >= 1.0) discard;

	// Calculate surface normal in view coordinate system.
	vec3 surface_normal = vec3(uv_fs, sqrt(1.0 - rsq));

	// Vary the depth value across the imposter to obtain proper intersections between particles.
	float ze = radius_and_eyez_fs.y + surface_normal.z * radius_and_eyez_fs.x;
	float zn = (projection_matrix[2][2] * ze + projection_matrix[3][2]) / (projection_matrix[2][3] * ze + projection_matrix[3][3]);
	float zdepth = 0.5 * (zn * gl_DepthRange.diff + (gl_DepthRange.far + gl_DepthRange.near));

	// Compute local surface color.
	outputShadedRayAndDepth(color_fs, surface_normal, vec3(0.0, 0.0, -1.0), zdepth);
}
