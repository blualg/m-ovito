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

    mat3 rot = mat3(1.0 - 2.0*(quat.y*quat.y + quat.z*quat.z),       2.0*(quat.x*quat.y + quat.w*quat.z),       2.0*(quat.x*quat.z - quat.w*quat.y),
                          2.0*(quat.x*quat.y - quat.w*quat.z), 1.0 - 2.0*(quat.x*quat.x + quat.z*quat.z),       2.0*(quat.y*quat.z + quat.w*quat.x),
                          2.0*(quat.x*quat.z + quat.w*quat.y),       2.0*(quat.y*quat.z - quat.w*quat.x), 1.0 - 2.0*(quat.x*quat.x + quat.y*quat.y));
    rot[0] *= axes.x;
    rot[1] *= axes.y;
    rot[2] *= axes.z;
    return rot;
}
