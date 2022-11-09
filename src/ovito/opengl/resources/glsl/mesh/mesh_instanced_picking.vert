////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

// Inputs:
in vec4 position;
in vec4 instance_tm_row1;
in vec4 instance_tm_row2;
in vec4 instance_tm_row3;

// Outputs:
flat out vec4 color_fs;

void main()
{
    // Apply instance transformation.
    vec3 instance_position = vec3(
        dot(instance_tm_row1, position), 
        dot(instance_tm_row2, position), 
        dot(instance_tm_row3, position));

	// Apply model-view-projection matrix to vertex.
    gl_Position = modelview_projection_matrix * vec4(instance_position, 1.0);

    // Compute color from object ID.
    color_fs = pickingModeColor(<InstanceID>);
}
