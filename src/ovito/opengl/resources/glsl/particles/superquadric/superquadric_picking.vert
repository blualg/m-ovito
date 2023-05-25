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

#include "../../global_uniforms.glsl"
#include "../../picking.glsl"
#include "../../shape_orientation.glsl"
#include <view_ray.vert>

// Inputs:
in vec3 position;
in float radius;
in vec3 aspherical_shape;
in vec4 orientation;
in vec2 roundness;
uniform vec3 unit_cube_triangle_strip[14];

// Outputs:
flat out vec4 color_fs;
flat out mat3 view_particle_matrix_fs;
flat out vec3 particle_view_pos_fs;
flat out vec2 particle_exponents_fs;

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

    // Compute color from object ID.
    color_fs = pickingModeColor(<InstanceID>);

    // Pass ellipsoid matrix and center position to fragment shader.
	particle_view_pos_fs = (modelview_matrix * vec4(position, 1.0)).xyz;
    view_particle_matrix_fs = <inverse_mat3>(mat3(modelview_matrix) * shape_orientation);

	// The x-component of the input vector is exponent 'e', the y-component is 'n'.
	particle_exponents_fs.x = 2.0 / (roundness.x > 0.0 ? roundness.x : 1.0);
	particle_exponents_fs.y = 2.0 / (roundness.y > 0.0 ? roundness.y : 1.0);

    // Calculate ray passing through the vertex (in view space).
    <calculate_view_ray_through_vertex>;
}
