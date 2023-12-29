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

#include "../../global_uniforms.glsl"
#include "../../shape_orientation.glsl"

// Inputs:
in vec3 position;
in float radius;
in vec3 color;
in float transparency;
in float selection;
uniform vec4 selection_color;
in vec3 aspherical_shape;
in vec4 orientation;
in vec2 roundness;

// Outputs:
out vec3 position_gs;
out vec4 color_gs;
out mat4 shape_orientation_gs;
out vec2 roundness_gs;

void main()
{
    // Forward particle position to geometry shader.
    position_gs = position;

    // Forward particle color to geometry shader.
    color_gs = (selection != 0.0) ? selection_color : vec4(color, clamp(1.0 - transparency, 0.0, 1.0));

    // Forward particle shape and orientation to geometry shader.
    shape_orientation_gs = mat4(calc_shape_orientation(orientation, aspherical_shape, radius));

    // Forward roundness parameters to geometry shader.
    roundness_gs = roundness;
}
