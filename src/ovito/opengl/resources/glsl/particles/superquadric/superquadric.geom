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

layout(points) in;
layout(triangle_strip, max_vertices=14) out;

#include "../../global_uniforms.glsl"
#include <view_ray.vert>

// Inputs:
in vec3 position_gs[1];
in vec4 color_gs[1];
in mat4 shape_orientation_gs[1];
in vec2 roundness_gs[1];
uniform vec3 unit_cube_triangle_strip[14];

// Outputs:
flat out vec4 color_fs;
flat out mat3 view_particle_matrix_fs;
flat out vec3 particle_view_pos_fs;
flat out vec2 particle_exponents_fs;

void main()
{
    mat3 shape_orientation = mat3(shape_orientation_gs[0]);

    // Compute particle center in view space.
    vec3 particle_view_pos = (modelview_matrix * vec4(position_gs[0], 1.0)).xyz;

    // Compute ellipsoid matrix.
    mat3 view_particle_matrix = <inverse_mat3>(mat3(modelview_matrix) * shape_orientation);

    // The x-component of the input vector is exponent 'e', the y-component is 'n'.
    vec2 particle_exponents;
    particle_exponents.x = 2.0 / (roundness_gs[0].x > 0.0 ? roundness_gs[0].x : 1.0);
    particle_exponents.y = 2.0 / (roundness_gs[0].y > 0.0 ? roundness_gs[0].y : 1.0);

    for(int corner = 0; corner < 14; corner++)
    {
        // Compute rotated and scaled unit cube corner coordinates.
        vec4 scaled_corner = vec4(position_gs[0] + shape_orientation * unit_cube_triangle_strip[corner], 1.0);

        // Apply model-view-projection matrix to particle position displaced by the cube vertex position.
        gl_Position = modelview_projection_matrix * scaled_corner;

        // Forward particle color to fragment shader.
        color_fs = color_gs[0];

        // Pass particle center position to fragment shader.
        particle_view_pos_fs = particle_view_pos;

        // Pass ellipsoid matrix to fragment shader.
        view_particle_matrix_fs = view_particle_matrix;

        // The x-component of the input vector is exponent 'e', the y-component is 'n'.
        particle_exponents_fs = particle_exponents;

        // Calculate ray passing through the vertex (in view space).
        <calculate_view_ray_through_vertex>;

        EmitVertex();
    }
    EndPrimitive();
}
