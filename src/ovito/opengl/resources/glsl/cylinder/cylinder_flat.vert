////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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

#include "../global_uniforms.glsl"

// Uniforms:
uniform vec3 view_dir_eye_pos; // Either camera viewing direction (parallel) or camera position (perspective) in object space coordinates.

// Inputs:
in vec3 base;
in vec3 head;
in float diameter;
in vec3 color1;
in vec3 color2;
in float transparency1;
in float transparency2;
uniform vec2 unit_quad_triangle_strip[4];

// Outputs:
out vec4 color_fs;

void main()
{
    // The radius of the current cylinder (in object coordinates).
    float radius = 0.5 * diameter;

    // The index of the quad corner.
    int corner = <VertexID>;

    // Vector pointing from camera to cylinder base in object space:
	vec3 view_dir;
	if(!is_perspective())
		view_dir = view_dir_eye_pos;
	else
		view_dir = view_dir_eye_pos - base;

	// Build local coordinate system in object space.
    mat3 uv_tm;
	uv_tm[0] = 0.5 * (head - base);
    uv_tm[1] = normalize(cross(view_dir, uv_tm[0])) * radius;
    uv_tm[2] = vec3(0);

	// Project corner vertex.
    gl_Position = modelview_projection_matrix * vec4(base + uv_tm[0] + uv_tm * vec3(unit_quad_triangle_strip[corner], 0.0), 1.0);

    // Forward primitive color to fragment shader.
    if(corner == 0 || corner == 2) {
        color_fs = vec4(color1, clamp(1.0 - transparency1, 0.0, 1.0));
    }
    else {
        color_fs = vec4(color2, clamp(1.0 - transparency2, 0.0, 1.0));
    }
}
