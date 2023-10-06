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

// Uniforms:
layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 projection_matrix;
    mat4 inverse_projection_matrix;
    vec2 viewport_origin; 			// Corner of the current viewport rectangle in window coordinates.
    vec2 inverse_viewport_size;	    // One over the width/height of the viewport rectangle in window space.
    float znear;
    float zfar;
} GlobalUniforms;

bool is_perspective()
{
    return GlobalUniforms.projection_matrix[0][3] != 0.0
        || GlobalUniforms.projection_matrix[1][3] != 0.0
        || GlobalUniforms.projection_matrix[2][3] != 0.0
        || GlobalUniforms.projection_matrix[3][3] != 1.0;
}

void calculate_view_ray(in vec2 viewport_position, out vec3 ray_origin, out vec3 ray_dir)
{
    vec4 near = GlobalUniforms.inverse_projection_matrix * vec4(viewport_position, 0.0, 1.0);
    vec4 far = near + GlobalUniforms.inverse_projection_matrix[2];
    ray_origin = near.xyz / near.w;
    ray_dir = far.xyz / far.w - ray_origin;
}