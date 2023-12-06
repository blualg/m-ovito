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
#include <view_ray.vert>

// Inputs:
in vec3 position;
in float radius;

// Outputs:
flat out vec4 color_fs;
flat out vec3 particle_view_pos_fs;
flat out float particle_radius_squared_fs;

void main()
{
    // The index of the cube corner.
    int corner = <VertexID>;

    // Particle radius in view space.
    float particle_radius = radius * length(modelview_matrix[0]);
    particle_radius_squared_fs = particle_radius * particle_radius;

    // Particle position in view space.
    particle_view_pos_fs = (modelview_matrix * vec4(position, 1.0)).xyz;

    vec3 uv;
    if(is_perspective()) {
        // Calculate maximum projection of particle:
        vec3 sphere_dir = particle_view_pos_fs;
        float sphere_dist_sq = dot(sphere_dir, sphere_dir);
        float sphere_dist = sqrt(sphere_dist_sq);
        float tangent_dist = sqrt(sphere_dist_sq - particle_radius_squared_fs);
        float alpha = acos(tangent_dist / sphere_dist);
        vec3 dir = cross(sphere_dir, vec3(0.0, 1.0, 0.0));

        float scaling = sphere_dist * tan(alpha) * sqrt(2.0);
        if(corner == 0) uv = scaling * normalize(dir);
        else if(corner == 1) uv = scaling * normalize(cross(dir, sphere_dir));
        else if(corner == 2) uv = -scaling * normalize(cross(dir, sphere_dir));
        else uv = -scaling * normalize(dir);
    }
    else {
        float scaling = particle_radius * sqrt(2.0);
        if(corner == 0) uv = vec3(scaling, 0.0, 0.0);
        else if(corner == 1) uv = vec3(0.0,  scaling, 0.0);
        else if(corner == 2) uv = vec3(0.0, -scaling, 0.0);
        else uv = vec3(-scaling, 0.0, 0.0);
    }

	// Apply projection matrix to quad vertex.
    gl_Position = projection_matrix * vec4(particle_view_pos_fs + uv, 1.0);

    // Compute color from object ID.
    color_fs = pickingModeColor(<InstanceID>);

    // Calculate ray passing through the vertex (in view space).
    <calculate_view_ray_through_vertex>;
}
