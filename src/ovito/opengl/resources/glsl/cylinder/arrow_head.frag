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

#include "../global_uniforms.glsl"
#include <shading.frag>
#include <view_ray.frag>

// Inputs:
flat in vec4 color_fs;
flat in vec3 center;					// Transformed cone vertex in view coordinates
flat in vec3 axis;						// Transformed cone axis in view coordinates
flat in float cone_radius;				// The radius of the cone

// Constants:
const float cone_ratio = 1.8; // Ratio of height to radius of arrow head code.
const float cone_angle = atan(1.0 / cone_ratio);
const float cone_cos_squared = cos(cone_angle) * cos(cone_angle); // squared cosine of the cone angle

void main()
{
    // Calculate ray passing through the fragment (in view space).
    <calculate_view_ray_through_fragment>;

	float zmin;
	vec3 ray_origin_shifted = ray_origin;
	// This is to improve numeric precision of intersection calculation:
	if(!is_perspective()) {
		zmin = dot(ray_dir_norm, ray_origin - center);
		ray_origin_shifted.z = center.z;
	}
	else {
		zmin = 0.0;
	}

	vec3 axis_normed = normalize(axis);
	float AdD = dot(axis_normed, ray_dir_norm);
	vec3 E = ray_origin_shifted - center;
	float AdE = dot(axis_normed, E);
	float DdE = dot(ray_dir_norm, E);
	float EdE = dot(E, E);
	float c2 = AdD*AdD - cone_cos_squared;
	float c1 = AdD*AdE - cone_cos_squared*DdE;
	float c0 = AdE*AdE - cone_cos_squared*EdE;

	// Solve the quadratic. Keep only those X for which dot(A,X-V) >= 0.
	float ray_t = zmin;

	float epsilon = 1e-9 * cone_radius * cone_radius;
	if(abs(c2) >= epsilon) {
		float discr = c1*c1 - c0*c2;
		if(discr < -epsilon) {
			// Q(t) = 0 has no real-valued roots. The ray does not
			// intersect the double-sided cone.
			discard;
		}
		else if(discr > epsilon) {
			// Q(t) = 0 has two distinct real-valued roots.  However, one or
			// both of them might intersect the portion of the double-sided
			// cone "behind" the vertex.  We are interested only in those
			// intersections "in front" of the vertex.
			float root = sqrt(discr);
			float height_sq = dot(axis, axis);
			float t = (-c1 - root) / c2;
			E = ray_origin_shifted + t * ray_dir_norm - center;
			float ddot = dot(E, axis);
			if(ddot > 0.0 && ddot < height_sq && t > zmin) {
				ray_t = t;
			}
			t = (-c1 + root) / c2;
			vec3 E2 = ray_origin_shifted + t * ray_dir_norm - center;
			ddot = dot(E2, axis);
			if(ddot > 0.0 && ddot < height_sq && t > zmin) {
				ray_t = t;
				E = E2;
			}
		}
		else {
			// One repeated real root (line is tangent to the cone).
			float t = -(c1/c2);
			E = ray_origin_shifted + t * ray_dir_norm - center;
			if(dot(E, axis) > 0.0) {
				ray_t = t;
			}
		}
	}
	else if(abs(c1) >= epsilon) {
		// c2 = 0, c1 != 0 (D is a direction vector on the cone boundary)
		float t = -(0.5*c0/c1);
		E = ray_origin_shifted + t * ray_dir_norm - center;
		if(dot(E, axis) > 0.0) {
			ray_t = t;
		}
	}
	else if(abs(c0) >= epsilon) {
		// c2 = c1 = 0, c0 != 0
		discard;
	}
	else if(DdE > 0.0) {
		// c2 = c1 = c0 = 0, cone contains ray V+t*D where V is cone vertex
		// and D is the line direction.
		ray_t = DdE;
	}
	if(ray_t <= zmin)
		discard;

	// Intersection point with cone:
	vec3 view_intersection_pnt = E + center;
	// Surface normal at cone intersection point:
	vec3 surface_normal = cross(E, cross(E, axis));

	// Compute intersection with disc.
	vec3 disc_center = center + axis;
	vec3 normal = axis;
	float d = -dot(disc_center, normal);
	float t = -(d + dot(normal, ray_origin_shifted));
	float td = dot(normal, ray_dir_norm);
	t /= td;
	if(t > zmin && t < ray_t) {
		vec3 hitpnt = ray_origin_shifted + t * ray_dir_norm - disc_center;
		if(dot(hitpnt,hitpnt) < cone_radius*cone_radius) {
			view_intersection_pnt = ray_origin_shifted + t * ray_dir_norm;
			surface_normal = normal;
		}
	}

	// Output the ray-cylinder intersection point as the fragment depth
	// rather than the depth of the bounding box polygons.
	// The eye coordinate Z value must be transformed to normalized device
	// coordinates before being assigned as the final fragment depth.
	vec4 projected_intersection = projection_matrix * vec4(view_intersection_pnt, 1.0);
	float zdepth = (projected_intersection.z / projected_intersection.w + 1.0) * 0.5;

    outputShadedRayAndDepth(color_fs, normalize(surface_normal), ray_dir_norm, zdepth);
}
