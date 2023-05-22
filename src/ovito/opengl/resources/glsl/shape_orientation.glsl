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

vec3 rotate_vector(in vec4 quat, in vec3 vec)
{
    return vec + 2.0 * cross(cross(vec, quat.xyz) + quat.w * vec, quat.xyz);
}

mat3 calc_shape_orientation(in vec4 orientation, in vec3 aspherical_shape, in float radius)
{
    vec3 axes;
    if(aspherical_shape != vec3(0.0, 0.0, 0.0)) {
        axes = aspherical_shape;
    }
    else {
        axes = vec3(radius);
    }

    vec4 quat;
    float norm = length(orientation);
    if(norm <= 1e-9)
        quat = vec4(0.0, 0.0, 0.0, 1.0);
    else
        quat = orientation / norm;

    return mat3(
        rotate_vector(quat, vec3(axes.x, 0.0, 0.0)),
        rotate_vector(quat, vec3(0.0, axes.y, 0.0)),
        rotate_vector(quat, vec3(0.0, 0.0, axes.z))
    );
}
