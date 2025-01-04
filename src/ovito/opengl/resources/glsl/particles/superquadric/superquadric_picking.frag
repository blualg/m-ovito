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

#include "../../global_uniforms.glsl"
#include <view_ray.frag>

// Inputs:
flat in vec4 color_fs;
flat in mat3 view_particle_matrix_fs;
flat in vec3 particle_view_pos_fs;
flat in vec2 particle_exponents_fs;

const int PLANECOUNT = 9;
const vec4 planes[PLANECOUNT] = vec4[](
	vec4(1.0, 1.0, 0.0, 0.0),
	vec4(1.0,-1.0, 0.0, 0.0),
	vec4(1.0, 0.0, 1.0, 0.0),
	vec4(1.0, 0.0,-1.0, 0.0),
	vec4(0.0, 1.0, 1.0, 0.0),
	vec4(0.0, 1.0,-1.0, 0.0),
	vec4(1.0, 0.0, 0.0, 0.0),
	vec4(0.0, 1.0, 0.0, 0.0),
	vec4(0.0, 0.0, 1.0, 0.0));

const float EPSILON = 1.0e-10;
const float MIN_VALUE = -1.01;
const float MAX_VALUE =  1.01;
const float BOUND_HUGE = 2.0e+10;
const float DEPTH_TOLERANCE = 1.0e-4; // Minimal intersection depth for a valid intersection.
const float ZERO_TOLERANCE = 1.0e-10; // If |x| < ZERO_TOLERANCE, x is regarded to be 0.
const int   MAX_ITERATIONS = 20;

/// Intersect a ray with an axis aligned unit box.
bool intersect_box(in vec3 P, in vec3 D, out float dmin, out float dmax)
{
    float tmin = 0.0, tmax = 0.0;

    // Left/right.
    if(abs(D.x) > EPSILON) {
        if(D.x > EPSILON) {
            dmin = (MIN_VALUE - P.x) / D.x;
            dmax = (MAX_VALUE - P.x) / D.x;
        }
        else {
            dmax = (MIN_VALUE - P.x) / D.x;
            dmin = (MAX_VALUE - P.x) / D.x;
        }

        if(dmin > dmax) return false;
    }
    else {
        if((P.x < MIN_VALUE) || (P.x > MAX_VALUE))
			return false;
        dmin = -BOUND_HUGE;
        dmax =  BOUND_HUGE;
    }

    // Top/bottom.
    if(abs(D.y) > EPSILON) {
        if(D.y > EPSILON) {
            tmin = (MIN_VALUE - P.y) / D.y;
            tmax = (MAX_VALUE - P.y) / D.y;
        }
        else {
            tmax = (MIN_VALUE - P.y) / D.y;
            tmin = (MAX_VALUE - P.y) / D.y;
        }

        if(tmax < dmax) {
            if(tmin > dmin) {
                if(tmin > tmax) return false;
                dmin = tmin;
            }
            else {
                if(dmin > tmax) return false;
            }

            dmax = tmax;
        }
        else {
            if(tmin > dmin) {
                if(tmin > dmax) return false;
                dmin = tmin;
            }
        }
    }
    else {
        if((P.y < MIN_VALUE) || (P.y > MAX_VALUE)) {
            return false;
        }
    }

    // Front/back.
    if(abs(D.z) > EPSILON) {
        if(D.z > EPSILON) {
            tmin = (MIN_VALUE - P.z) / D.z;
            tmax = (MAX_VALUE - P.z) / D.z;
        }
        else {
            tmax = (MIN_VALUE - P.z) / D.z;
            tmin = (MAX_VALUE - P.z) / D.z;
        }

        if(tmax < dmax) {
            if(tmin > dmin) {
                if(tmin > tmax) return false;
                dmin = tmin;
            }
            else {
                if(dmin > tmax) return false;
            }

            dmax = tmax;
        }
        else {
            if(tmin > dmin) {
                if(tmin > dmax) return false;
                dmin = tmin;
            }
        }
    }
    else {
        if((P.z < MIN_VALUE) || (P.z > MAX_VALUE)) {
            return false;
        }
    }

    return true;
}

float evaluate_g(in float x, in float y, in float e)
{
    float g = 0.0;

    if(x > y) {
        g = 1.0 + pow(y / x, e);
        if(g != 1.0)
            g = pow(g, 1.0 / e);
        g *= x;
    }
    else if(y != 0.0) {
        g = 1.0 + pow(x / y, e);
        if(g != 1.0)
            g = pow(g, 1.0 / e);
        g *= y;
    }

    return g;
}

/// Computes the superellipsoid value at the given location.
float evaluate_superellipsoid(in vec3 P)
{
    return evaluate_g(evaluate_g(abs(P.x), abs(P.y), particle_exponents_fs.x), abs(P.z), particle_exponents_fs.y) - 1.0;
}

// Home in on the root of a superquadric using a combination of secant and bisection methods.
// This routine requires that the sign of the function be different at P0 and P1, it will fail drastically if this isn't the case.
void solve_hit1(in float v0, in vec3 tP0, in float v1, in vec3 tP1, out vec3 P)
{
    int i;
    float x, v2, v3;
    vec3 P0, P1, P2, P3;

    P0 = tP0;
    P1 = tP1;

    // The sign of v0 and v1 changes between P0 and P1, this means there is an intersection point in there somewhere.
    for(i = 0; i < MAX_ITERATIONS; i++) {
        if(abs(v0) < ZERO_TOLERANCE) {
            // Near point is close enough to an intersection - just use it.
            P = P0;
            break;
        }

        if(abs(v1) < ZERO_TOLERANCE) {
            // Far point is close enough to an intersection.
            P = P1;
            break;
        }

        // Look at the chord connecting P0 and P1.
        // Assume a line between the points.
        x = abs(v0) / abs(v1 - v0);
        P2 = P1 - P0;
        P2 = P0 + x * P2;
        v2 = evaluate_superellipsoid(P2);

        // Look at the midpoint between P0 and P1.
        P3 = P1 - P0;
        P3 = P0 + 0.5 * P3;
        v3 = evaluate_superellipsoid(P3);

        if(v2 * v3 < 0.0) {
            // We can move both ends.
            v0 = v2;
            P0 = P2;
            v1 = v3;
            P1 = P3;
        }
        else {
            if(abs(v2) < abs(v3)) {
                // secant method is doing better.
                if(v0 * v2 < 0.0) {
                    v1 = v2;
                    P1 = P2;
                }
                else {
                    v0 = v2;
                    P0 = P2;
                }
            }
            else {
                // bisection method is doing better.
                if(v0 * v3 < 0.0) {
                    v1 = v3;
                    P1 = P3;
                }
                else {
                    v0 = v3;
                    P0 = P3;
                }
            }
        }
    }

    if(i == MAX_ITERATIONS) {
        // The loop never quite closed in on the result - just use the point
        // closest to zero.  This really shouldn't happen since the max number
        // of iterations is enough to converge with straight bisection.
        if(abs(v0) < abs(v1)) {
            P = P0;
        }
        else {
            P = P1;
        }
    }
}


/// Try to find the root of a superquadric using Newtons method.
bool check_hit2(in vec3 P, in vec3 D, in float t0, inout vec3 P0, in float v0, in float t1, out float t, out vec3 Q)
{
    int i;
    float dt0, dt1, v1, deltat, maxdelta;

	const float eps = 1.0e-5;

    dt0 = t0;
    dt1 = t0 + 1.0e-4 * (t1 - t0);
    maxdelta = t1 - t0;

    for(i = 0; (dt0 < t1) && (i < MAX_ITERATIONS); i++) {
        vec3 P1 = P + dt1 * D;
        v1 = evaluate_superellipsoid(P1);

        if(v0 * v1 < 0.0) {
            // Found a crossing point, go back and use normal root solving.
            solve_hit1(v0, P0, v1, P1, Q);
            P0 = Q - P;
            t = length(P0);
            return true;
        }
        else {
            if(abs(v1) < eps) {
                Q = P + dt1 * D;
                t = dt1;
                return true;
            }
            else {
                if(((v0 > 0.0) && (v1 > v0)) || ((v0 < 0.0) && (v1 < v0))) {
                    // We definitely failed.
                    break;
                }
                else {
                    if(v1 == v0) {
                        break;
                    }
                    else {
                        deltat = v1 * (dt1 - dt0) / (v1 - v0);
                    }
                }
            }
        }

        if(abs(deltat) > maxdelta) {
            break;
		}

        v0 = v1;
        dt0 = dt1;
        dt1 -= deltat;
    }

    return false;
}
void main()
{
    // Calculate ray passing through the fragment (in view space).
    <calculate_view_ray_through_fragment>;

	vec3 ray_origin_shifted = ray_origin;
	// This is to improve numeric precision of intersection calculation:
	if(!is_perspective())
		ray_origin_shifted.z = particle_view_pos_fs.z;

    // Transform the ray into the superellipsoid space.
	vec3 P = view_particle_matrix_fs * (ray_origin_shifted - particle_view_pos_fs);
	vec3 D = view_particle_matrix_fs * ray_dir_norm;
	float len = length(D);
	D /= len;

    // Intersect bounding box.
	float t1, t2;
    if(!intersect_box(P, D, t1, t2)) {
		discard;
	}

    // Test if superellipsoid lies 'behind' the ray origin.
	if(is_perspective()) {
		if(t2 < DEPTH_TOLERANCE)
			discard;
		if(t1 < DEPTH_TOLERANCE)
			t1 = DEPTH_TOLERANCE;
	}
	else {
		// When using parallel projection, make sure intersection point is always at t>0 by shifting the ray base point.
		if(t1 < DEPTH_TOLERANCE) {
			P -= D * (DEPTH_TOLERANCE - t1);
			ray_origin_shifted -= ray_dir_norm * ((DEPTH_TOLERANCE - t1) / len);
			t2 += DEPTH_TOLERANCE - t1;
			t1 = DEPTH_TOLERANCE;
		}
	}

	int cnt = 2;
    float dists[PLANECOUNT + 2];
    dists[0] = t1;
    dists[1] = t2;

    // Intersect ray with planes cutting superellipsoids in pieces.
	// Find all the places where the ray intersects the set of
	// subdividing planes through the superquadric.  Return the
	// number of valid hits (within the bounding box).

    // Since min and max dist are the distance to two of the bounding planes
    // we are considering, there is a high probablity of missing them due to
    // round off error. Therefore we adjust min and max.
    float margin = EPSILON * (t2 - t1);
    float mindist = t1 - margin;
    float maxdist = t2 + margin;

    // Check the sets of planes that cut apart the superquadric.
	int i;
    for(i = 0; i < PLANECOUNT; i++) {
        float d = dot(D, planes[i].xyz);

        if(abs(d) < EPSILON)
            continue; // Can't possibly get a hit for this combination of ray and plane.

        float t = (planes[i].w - dot(P, planes[i].xyz)) / d;

        if((t >= mindist) && (t <= maxdist)) {
            dists[cnt++] = t;
        }
    }

    // Sort the results for further processing.
	// Todo: Replace this bubble sort implemention with something more efficient.
	bool done;
	do {
		done = true;
		for(i = 1; i < cnt; i++) {
			if(dists[i] < dists[i-1]) {
				float temp = dists[i];
				dists[i] = dists[i-1];
				dists[i-1] = temp;
				done = false;
			}
		}
	}
	while(!done);

	vec3 P0 = P + dists[0] * D;
	float v0 = evaluate_superellipsoid(P0);

	float tnear = BOUND_HUGE;
	if(abs(v0) < ZERO_TOLERANCE) {
		tnear = dists[0] / len;
	}
	else {
		for(i = 1; i < cnt; i++) {
			vec3 P1 = P + dists[i] * D;
			float v1 = evaluate_superellipsoid(P1);

			if(abs(v1) < ZERO_TOLERANCE) {
				tnear = dists[i] / len;
				break;
			}
			else {
				if(v0 * v1 < 0.0) {
					// Opposite signs: there must be a root between.
					vec3 P2;
					solve_hit1(v0, P0, v1, P1, P2);

					vec3 P3 = P2 - P;
					float t = length(P3);

					tnear = t / len;
					break;
				}
				else {
					// Although there was no sign change, we may actually be approaching
					// the surface. In this case, we are being fooled by the shape of the
					// surface into thinking there isn't a root between sample points.
					float t;
					vec3 P2;
					if(check_hit2(P, D, dists[i-1], P0, v0, dists[i], t, P2)) {
						tnear = t / len;
						break;
					}
				}
			}

			v0 = v1;
			P0 = P1;
		}
	}

	// Discard intersections behind the view point.
	if(tnear == BOUND_HUGE || tnear < 0.0) {
		discard;
	}

	// Calculate intersection point in view coordinate system.
	vec3 view_intersection_pnt = ray_origin_shifted + tnear * ray_dir_norm;

	// Output the ray-sphere intersection point as the fragment depth
	// rather than the depth of the bounding box polygons.
	// The eye coordinate Z value must be transformed to normalized device
	// coordinates before being assigned as the final fragment depth.
	vec4 projected_intersection = projection_matrix * vec4(view_intersection_pnt, 1.0);
	<fragDepth> = (projected_intersection.z / projected_intersection.w + 1.0) * 0.5;

	// Use flat shading in picking mode.
    <fragColor> = color_fs;
}
