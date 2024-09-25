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

// Uniforms:
uniform sampler2D color_map;
uniform float opacity;
uniform vec4 selection_color;

// Inputs:
in float pseudocolor_fs;
flat in float selected_face_fs;
in vec3 normal_fs;

void main()
{
    if(selected_face_fs == 0.0) {
        vec3 color = <texture2D>(color_map, vec2(pseudocolor_fs, 0.0)).xyz;
        outputShaded(vec4(color, opacity), normalize(normal_fs));
    }
    else {
        outputShaded(selection_color, normalize(normal_fs));
    }
}
