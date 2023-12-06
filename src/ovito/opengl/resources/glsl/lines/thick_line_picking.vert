////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "../picking.glsl"

// Uniforms:
uniform float line_thickness; // Half line width in viewport space.

// Inputs:
in vec4 position_from;
in vec4 position_to;

// Outputs:
out vec4 color_fs;
void main()
{
    // The index of the quad corner.
    int corner = <VertexID>;

	// Apply model-view-projection matrix to line points.
	vec4 proj_from = modelview_projection_matrix * position_from;
	vec4 proj_to   = modelview_projection_matrix * position_to;

	// Compute line direction vector.
	vec2 delta = normalize(proj_to.xy / proj_to.w - proj_from.xy / proj_from.w) * line_thickness;

	// Correct direction vector if one vertex is behind the viewer the other one in front.
	if(proj_to.w * proj_from.w < 0.0)
		delta = -delta;

	// Take into account aspect ratio of viewport:
	delta.y *= inverse_viewport_size.x / inverse_viewport_size.y;

	// Emit quad vertices.
	if(corner == 0)
		gl_Position = proj_from - vec4(delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
	else if(corner == 1)
		gl_Position = proj_from + vec4(delta.y * proj_from.w, -delta.x * proj_from.w, 0.0, 0.0);
	else if(corner == 2)
		gl_Position = proj_to - vec4(delta.y * proj_to.w, -delta.x * proj_to.w, 0.0, 0.0);
	else
		gl_Position = proj_to + vec4(delta.y * proj_to.w, -delta.x * proj_to.w, 0.0, 0.0);

	// Compute color from primitive index.
	color_fs = pickingModeColor(<InstanceID>);
}
