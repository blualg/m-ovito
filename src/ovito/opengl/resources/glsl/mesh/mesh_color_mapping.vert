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
uniform float color_range_min;
uniform float color_range_max;

// Inputs:
in vec3 position;
in vec3 normal;
in vec2 pseudocolor;

// Outputs:
out float pseudocolor_fs;
flat out float selected_face_fs;
out vec3 normal_fs;

void main()
{
	// Apply model-view-projection matrix to vertex.
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);

    // Pass vertex pseudo-color on to fragment shader.
    pseudocolor_fs = (pseudocolor.r - color_range_min) / (color_range_max - color_range_min);

    // Pass face selection state to fragment shader.
    selected_face_fs = pseudocolor.g;

    // Transform vertex normal from object to view space.
    normal_fs = vec3(normal_tm * vec4(normal, 0.0));
}
