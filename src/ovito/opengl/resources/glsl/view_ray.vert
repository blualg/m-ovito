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

noperspective out vec3 ray_origin;
noperspective out vec3 ray_dir;

// This function is used called by vertex or geometry shaders to calculate the ray (in view space)
// that goes through the current vertex. The interpolated output is used by the fragment shader to
// calculate the view ray going through each fragment.
void calculate_view_ray_through_vertex()
{
    vec2 viewport_position = vec2(gl_Position.x / gl_Position.w, gl_Position.y / gl_Position.w);
    vec4 near = inverse_projection_matrix * vec4(viewport_position, -1.0, 1.0);
    vec4 far = near + inverse_projection_matrix[2];
    ray_origin = near.xyz / near.w;
    ray_dir = far.xyz / far.w - ray_origin;
}