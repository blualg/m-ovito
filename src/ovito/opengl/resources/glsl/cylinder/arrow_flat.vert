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

#include "../global_uniforms.glsl"

// Uniforms:
uniform vec3 view_dir_eye_pos; // Either camera viewing direction (parallel) or camera position (perspective) in object space coordinates.

// Inputs:
in vec3 base;
in vec3 head;
in float diameter;
in vec3 color1;
in float transparency1;

// Outputs:
flat out vec4 color_fs;

void main()
{
    // The radius of the current arrow (in object coordinates).
    float radius = 0.5 * diameter;

    // Vector pointing from camera to cylinder base in object space:
	vec3 view_dir;
	if(!is_perspective())
		view_dir = view_dir_eye_pos;
	else
		view_dir = view_dir_eye_pos - base;

	// Build local coordinate system in object space.
    mat3 uv_tm;
	uv_tm[0] = head - base;
    uv_tm[1] = normalize(cross(view_dir, uv_tm[0])) * radius;
    uv_tm[2] = vec3(0);

    vec2 vpos;
    float arrowHeadRadius = 2.5;
    float arrowHeadLength = (radius * arrowHeadRadius * 1.8) / length(uv_tm[0]);
    int vidx = <VertexID>;
    if(arrowHeadLength < 1.0) {
        if(vidx == 0) vpos = vec2(1.0, 0.0);
        else if(vidx == 1) vpos = vec2(1.0 - arrowHeadLength, arrowHeadRadius);
        else if(vidx == 2) vpos = vec2(1.0 - arrowHeadLength, 1.0);
        else if(vidx == 3) vpos = vec2(0.0, 1.0);
        else if(vidx == 4) vpos = vec2(0.0,-1.0);
        else if(vidx == 5) vpos = vec2(1.0 - arrowHeadLength,-1.0);
        else if(vidx == 6) vpos = vec2(1.0 - arrowHeadLength,-arrowHeadRadius);
    }
    else {
        if(vidx == 0) vpos = vec2(1.0, 0.0);
        else if(vidx == 1) vpos = vec2(0.0, arrowHeadRadius / arrowHeadLength);
        else if(vidx == 6) vpos = vec2(0.0,-arrowHeadRadius / arrowHeadLength);
        else vpos = vec2(0.0, 0.0);
    }

	// Project corner vertex.
    gl_Position = modelview_projection_matrix * vec4(base + uv_tm * vec3(vpos, 0.0), 1.0);

    // Forward primitive color to fragment shader.
    color_fs = vec4(color1, clamp(1.0 - transparency1, 0.0, 1.0));
}
