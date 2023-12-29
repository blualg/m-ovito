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
#include "../../shape_orientation.glsl"
#include <view_ray.vert>

// Inputs:
in vec3 position;
in float radius;
in vec3 color;
in float transparency;
in float selection;
uniform vec4 selection_color;
in vec3 aspherical_shape;
in vec4 orientation;
uniform vec3 unit_cube_triangle_strip[14];

// Outputs:
flat out vec4 color_fs;
flat out mat3 view_to_sphere_fs;
flat out mat3 sphere_to_view_fs;
flat out vec3 particle_view_pos_fs;

void main()
{
    // The index of the box corner.
    int corner = <VertexID>;

    // Prepare matrix that describes the aspherical shape and orientation of the particle.
    mat3 shape_orientation = calc_shape_orientation(orientation, aspherical_shape, radius);

    // Compute rotated and scaled unit cube corner coordinates.
    vec4 scaled_corner = vec4(position + shape_orientation * unit_cube_triangle_strip[corner], 1.0);

	// Apply model-view-projection matrix to particle position displaced by the cube vertex position.
    gl_Position = modelview_projection_matrix * scaled_corner;

    // Forward particle color to fragment shader.
    color_fs = (selection != 0.0) ? selection_color : vec4(color, clamp(1.0 - transparency, 0.0, 1.0));

    // Pass particle center position to fragment shader.
	particle_view_pos_fs = (modelview_matrix * vec4(position, 1.0)).xyz;

    // Matrices for converting to/from unit sphere space.
    sphere_to_view_fs = mat3(modelview_matrix) * shape_orientation;
    view_to_sphere_fs = <inverse_mat3>(sphere_to_view_fs);

    // Calculate ray passing through the vertex (in view space).
    <calculate_view_ray_through_vertex>;
}
