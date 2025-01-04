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

layout(points) in;
layout(triangle_strip, max_vertices=14) out;

#include "../global_uniforms.glsl"
#include <view_ray.vert>

// Inputs:
in vec3 base_gs[1];
in vec3 head_gs[1];
in float radius_gs[1];
in vec4 color_gs[1];
uniform vec3 unit_box_triangle_strip[14];

// Outputs:
flat out vec4 color_fs;
flat out vec3 cylinder_view_base;		// Transformed cylinder position in view coordinates
flat out vec3 cylinder_view_axis;		// Transformed cylinder axis in view coordinates
flat out float cylinder_radius_sq_fs;	// The squared radius of the cylinder
flat out float cylinder_length;			// The length of the cylinder

void main()
{
    // Set up an axis tripod that is aligned with the cylinder.
    mat3 orientation_tm;
    orientation_tm[2] = head_gs[0] - base_gs[0];
    if(orientation_tm[2] != vec3(0.0)) {
        if(orientation_tm[2].y != 0.0 || orientation_tm[2].x != 0.0)
            orientation_tm[0] = normalize(vec3(orientation_tm[2].y, -orientation_tm[2].x, 0.0)) * radius_gs[0];
        else
            orientation_tm[0] = normalize(vec3(-orientation_tm[2].z, 0.0, orientation_tm[2].x)) * radius_gs[0];
        orientation_tm[1] = normalize(cross(orientation_tm[2], orientation_tm[0])) * radius_gs[0];
    }
    else {
        orientation_tm = mat3(0.0);
    }

    // Apply additional scaling to cylinder radius due to model-view transformation.
    float viewspace_radius = radius_gs[0] * length(modelview_matrix[0]);

    // Transform cylinder to eye coordinates.
    vec3 cylinder_view_base_ = (modelview_matrix * vec4(base_gs[0], 1.0)).xyz;
    vec3 cylinder_view_axis_ = (modelview_matrix * vec4(orientation_tm[2], 0.0)).xyz;

    for(int corner = 0; corner < 14; corner++)
    {
        // Apply model-view-projection matrix to box vertex position.
        gl_Position = modelview_projection_matrix * vec4(base_gs[0] + (orientation_tm * unit_box_triangle_strip[corner]), 1.0);

        // Forward cylinder color to fragment shader.
        color_fs = color_gs[0];

        // Pass square of cylinder radius to fragment shader.
        cylinder_radius_sq_fs = viewspace_radius * viewspace_radius;

        // Transform cylinder to eye coordinates.
        cylinder_view_base = cylinder_view_base_;
        cylinder_view_axis = cylinder_view_axis_;

        // Pass cylinder length to fragment shader.
        cylinder_length = length(cylinder_view_axis_);

        // Calculate ray passing through the vertex (in view space).
        <calculate_view_ray_through_vertex>;

        EmitVertex();
    }
    EndPrimitive();
}
