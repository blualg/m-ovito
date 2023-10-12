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
#include <shading.frag>
#include <view_ray.frag>

// Inputs:
flat in vec4 color_fs;
flat in vec3 particle_view_pos_fs;
flat in float particle_radius_squared_fs;

void main()
{
    // Calculate ray passing through the fragment (in view space).
    <calculate_view_ray_through_fragment>;

	vec3 sphere_dir = particle_view_pos_fs - ray_origin;

	// Perform ray-sphere intersection test.
	float b = dot(ray_dir_norm, sphere_dir);
	vec3 delta = ray_dir_norm * b - sphere_dir;
	float x = dot(delta, delta);
	float disc = particle_radius_squared_fs - x;

	// Only calculate the intersection closest to the viewer.
	if(disc < 0.0)
		discard; // Ray missed sphere entirely, discard fragment

	// Calculate closest intersection position.
	float tnear = b - sqrt(disc);

	// Discard intersections located behind the viewer.
	if(tnear < 0.0)
		discard;

	// Calculate intersection point in view coordinate system.
	vec3 view_intersection_pnt = ray_origin + tnear * ray_dir_norm;

	// Output the ray-sphere intersection point as the fragment depth
	// rather than the depth of the bounding box polygons.
	// The eye coordinate Z value must be transformed to normalized device
	// coordinates before being assigned as the final fragment depth.
	vec4 projected_intersection = projection_matrix * vec4(view_intersection_pnt, 1.0);
	float zdepth = (projected_intersection.z / projected_intersection.w + 1.0) * 0.5;

    // Calculate surface normal in view coordinate system.
    vec3 surface_normal = normalize(view_intersection_pnt - particle_view_pos_fs);
    outputShadedRayAndDepth(color_fs, surface_normal, ray_dir_norm, zdepth);
}
