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

#include "../../global_uniforms.glsl"

// Inputs:
in vec3 position;
in float radius;
in vec3 color;
in float transparency;
in float selection;
uniform vec4 selection_color;
uniform vec2 unit_quad_triangle_strip[4];

// Outputs:
flat out vec4 color_fs;
out vec2 uv_fs;

void main()
{
    // The index of the quad corner.
    int corner = <VertexID>;

    // Transform particle center to view space.
	vec3 eye_position = (modelview_matrix * vec4(position, 1.0)).xyz;

    // Apply additional scaling due to model-view transformation to particle radius.
    float viewspace_radius = radius * length(modelview_matrix[0]);

	// Project corner vertex.
    gl_Position = projection_matrix * (vec4(eye_position, 1.0) + vec4(unit_quad_triangle_strip[corner] * viewspace_radius, 0.0, 0.0));

    // Forward particle color to fragment shader.
    color_fs = (selection != 0.0) ? selection_color : vec4(color, clamp(1.0 - transparency, 0.0, 1.0));

    // Pass UV quad coordinates to fragment shader.
    uv_fs = unit_quad_triangle_strip[corner];
}
