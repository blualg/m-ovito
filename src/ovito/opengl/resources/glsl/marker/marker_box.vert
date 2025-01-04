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

#include "../global_uniforms.glsl"

// Uniforms:
uniform float marker_size;

// Inputs
in vec3 position;
void main()
{
	// Const array of vertex positions for the marker glyph.
	const vec3 marker[24] = vec3[24](
		vec3(-1.0, -1.0, -1.0), vec3( 1.0,-1.0,-1.0),
		vec3(-1.0, -1.0,  1.0), vec3( 1.0,-1.0, 1.0),
		vec3(-1.0, -1.0, -1.0), vec3(-1.0,-1.0, 1.0),
		vec3( 1.0, -1.0, -1.0), vec3( 1.0,-1.0, 1.0),
		vec3(-1.0,  1.0, -1.0), vec3( 1.0, 1.0,-1.0),
		vec3(-1.0,  1.0,  1.0), vec3( 1.0, 1.0, 1.0),
		vec3(-1.0,  1.0, -1.0), vec3(-1.0, 1.0, 1.0),
		vec3( 1.0,  1.0, -1.0), vec3( 1.0, 1.0, 1.0),
		vec3(-1.0, -1.0, -1.0), vec3(-1.0, 1.0,-1.0),
		vec3( 1.0, -1.0, -1.0), vec3( 1.0, 1.0,-1.0),
		vec3( 1.0, -1.0,  1.0), vec3( 1.0, 1.0, 1.0),
		vec3(-1.0, -1.0,  1.0), vec3(-1.0, 1.0, 1.0)
	);

	// Determine marker size.
	vec4 view_position = modelview_matrix * vec4(position, 1.0);
	float w = projection_matrix[0][3] * view_position.x + projection_matrix[1][3] * view_position.y
			+ projection_matrix[2][3] * view_position.z + projection_matrix[3][3];

	// The vertex coordinates in model space.
	vec3 delta = marker[<VertexID>] * (w * marker_size);

	// Apply model-view-projection matrix.
	gl_Position = modelview_projection_matrix * vec4(position + delta, 1.0);
}
