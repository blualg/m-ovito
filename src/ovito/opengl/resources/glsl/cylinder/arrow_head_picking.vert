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
#include <view_ray.vert>

// Inputs:
in vec3 base;
in vec3 head;
in float diameter;
uniform vec3 unit_box_triangle_strip[14];

// Outputs:
flat out vec4 color_fs;
flat out vec3 center;	// Transformed cone vertex in view coordinates
flat out vec3 axis;		// Transformed cone axis in view coordinates
flat out float cone_radius;	// The radius of the cone

const float cone_ratio = 1.8; // Ratio of height to radius of arrow head code.

void main()
{
    // The radius of the current arrow (in object coordinates).
    float radius = 0.5 * diameter;

    // The index of the box corner.
    int corner = <VertexID>;

    float arrowHeadRadius = radius * 2.5;
    float arrowHeadLength = cone_ratio * arrowHeadRadius;

    // Set up an axis tripod that is aligned with the cone.
    mat3 orientation_tm;
    orientation_tm[2] = head - base;
    float len = length(orientation_tm[2]);
    if(len != 0.0) {
        if(arrowHeadLength > len) {
            arrowHeadRadius *= len / arrowHeadLength;
            arrowHeadLength = len;
        }
        orientation_tm[2] *= arrowHeadLength / len;

        if(orientation_tm[2].y != 0.0 || orientation_tm[2].x != 0.0)
            orientation_tm[0] = normalize(vec3(orientation_tm[2].y, -orientation_tm[2].x, 0.0)) * arrowHeadRadius;
        else
            orientation_tm[0] = normalize(vec3(-orientation_tm[2].z, 0.0, orientation_tm[2].x)) * arrowHeadRadius;
        orientation_tm[1] = normalize(cross(orientation_tm[2], orientation_tm[0])) * arrowHeadRadius;
    }
    else {
        orientation_tm = mat3(0.0);
    }

	// Apply model-view-projection matrix to box vertex position.
    gl_Position = modelview_projection_matrix * vec4(head - orientation_tm[2] + (orientation_tm * unit_box_triangle_strip[corner]), 1.0);

    // Compute color from object ID.
    color_fs = pickingModeColor(<InstanceID>);

    // Apply additional scaling to cone radius due to model-view transformation.
	// Pass square of cylinder radius to fragment shader.
    cone_radius = arrowHeadRadius * length(modelview_matrix[0]);

	// Transform cone to eye coordinates.
    center = (modelview_matrix * vec4(head, 1.0)).xyz;
    axis = (modelview_matrix * vec4(-orientation_tm[2], 0.0)).xyz;

    // Calculate ray passing through the vertex (in view space).
    <calculate_view_ray_through_vertex>;
}
