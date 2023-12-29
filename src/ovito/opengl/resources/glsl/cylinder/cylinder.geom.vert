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

// Inputs:
in vec3 base;
in vec3 head;
in float diameter;
in vec3 color1;
in vec3 color2;
in float transparency1;
in float transparency2;

// Outputs:
out vec3 base_gs;
out vec3 head_gs;
out float radius_gs;
out vec4 color1_gs;
out vec4 color2_gs;

void main()
{
    // Forward data to geometry shader.
    base_gs = base;
    head_gs = head;
    radius_gs = 0.5 * diameter;
    color1_gs = vec4(color1, clamp(1.0 - transparency1, 0.0, 1.0));
    color2_gs = vec4(color2, clamp(1.0 - transparency2, 0.0, 1.0));
}
