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
layout(triangle_strip, max_vertices=4) out;

#include "../../global_uniforms.glsl"
#include <view_ray.vert>

// Inputs:
in vec3 position_gs[1];
in float radius_gs[1];
in vec4 color_gs[1];

// Outputs:
flat out vec4 color_fs;
flat out vec3 particle_view_pos_fs;
flat out float particle_radius_squared_fs;

void main()
{
    // Precompute squared particle radius in view space.
    float particle_radius = radius_gs[0] * length(modelview_matrix[0]);
    float particle_radius_squared = particle_radius * particle_radius;

    // Particle position in view space:
    vec3 view_space_particle_pos = (modelview_matrix * vec4(position_gs[0], 1.0)).xyz;

    vec3 dirs[4];
    if(is_perspective()) {
        // Calculate maximum projection of particle:
        vec3 sphere_dir = view_space_particle_pos;
        float sphere_dist_sq = dot(sphere_dir, sphere_dir);
        float sphere_dist = sqrt(sphere_dist_sq);
        float tangent_dist = sqrt(sphere_dist_sq - particle_radius_squared);
        float alpha = acos(tangent_dist / sphere_dist);
        vec3 dir = cross(sphere_dir, vec3(0.0, 1.0, 0.0));

        float scaling = sphere_dist * tan(alpha) * sqrt(2.0);
        dirs[0] = scaling * normalize(dir);
        dirs[1] = scaling * normalize(cross(dir, sphere_dir));
        dirs[2] = -dirs[1];
        dirs[3] = -dirs[0];
    }
    else {
        float scaling = particle_radius * sqrt(2.0);
        dirs[0] = vec3(scaling, 0.0, 0.0);
        dirs[1] = vec3(0.0, scaling, 0.0);
        dirs[2] = vec3(0.0, -scaling, 0.0);
        dirs[3] = vec3(-scaling, 0.0, 0.0);
    }

    for(int corner = 0; corner < 4; corner++)
    {
        gl_Position = projection_matrix * vec4(view_space_particle_pos + dirs[corner], 1.0);

        // Forward particle color to fragment shader.
        color_fs = color_gs[0];

        // Pass particle radius and center position to fragment shader.
        particle_radius_squared_fs = particle_radius_squared;
        particle_view_pos_fs = view_space_particle_pos;

        // Calculate ray passing through the vertex (in view space).
        <calculate_view_ray_through_vertex>;

        EmitVertex();
    }
    EndPrimitive();
}
