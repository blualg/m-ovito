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
#include <view_ray.frag>

// Inputs:
flat in vec4 color_fs;
flat in mat3 view_to_sphere_fs;
flat in mat3 sphere_to_view_fs;
flat in vec3 particle_view_pos_fs;

void main()
{
    // Calculate ray passing through the fragment (in view space).
    <calculate_view_ray_through_fragment>;

	vec3 sphere_dir = view_to_sphere_fs * (particle_view_pos_fs - ray_origin);

	// Ray direction in sphere coordinate system.
	vec3 ray_dir2 = normalize(view_to_sphere_fs * ray_dir_norm);

	// Perform ray-sphere intersection test.
	float b = dot(ray_dir2, sphere_dir);
	vec3 delta = ray_dir2 * b - sphere_dir;
	float x = dot(delta, delta);
	float disc = 1.0 - x;

	// Only calculate the intersection closest to the viewer.
	if(disc < 0.0)
		discard; // Ray missed sphere entirely, discard fragment

	// Calculate closest intersection position.
	float tnear = b - sqrt(disc);

	// Discard intersections located behind the viewer.
	if(tnear < 0.0)
		discard;

	// Calculate intersection point in sphere coordinate system.
	vec3 sphere_intersection_pnt = tnear * ray_dir2 - sphere_dir;

	// Calculate intersection point in view coordinate system.
    vec3 view_intersection_pnt = sphere_to_view_fs * sphere_intersection_pnt + particle_view_pos_fs;

	// Output the ray-sphere intersection point as the fragment depth
	// rather than the depth of the bounding box polygons.
	// The eye coordinate Z value must be transformed to normalized device
	// coordinates before being assigned as the final fragment depth.
	vec4 projected_intersection = projection_matrix * vec4(view_intersection_pnt, 1.0);
	<fragDepth> = (projected_intersection.z / projected_intersection.w + 1.0) * 0.5;

    // Use flat shading in picking mode.
    <fragColor> = color_fs;
}
