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

// Inputs:
in vec4 position;
in vec3 normal;
in vec4 color;
in vec4 instance_tm_row1;
in vec4 instance_tm_row2;
in vec4 instance_tm_row3;

// Outputs:
out vec4 color_fs;
out vec3 normal_fs;

void main()
{
    // Apply instance transformation.
    vec3 instance_position = vec3(
        dot(instance_tm_row1, position),
        dot(instance_tm_row2, position),
        dot(instance_tm_row3, position));

	// Apply model-view-projection matrix to vertex.
    gl_Position = modelview_projection_matrix * vec4(instance_position, 1.0);

    // Pass vertex color on to fragment shader.
    color_fs = color;

    // Apply instance transformation to normal vector.
    // Note: We are assuming a pure rotation here.
    vec3 instance_normal = vec3(
        dot(instance_tm_row1, vec4(normal, 0.0)),
        dot(instance_tm_row2, vec4(normal, 0.0)),
        dot(instance_tm_row3, vec4(normal, 0.0)));

    // Transform vertex normal from object to view space.
    normal_fs = vec3(normal_tm * vec4(instance_normal, 0.0));
}
