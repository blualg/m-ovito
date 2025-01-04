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

// Uniforms:
uniform vec4 image_rect;

// Outputs:
out vec2 texcoord_fs;

void main()
{
    // Draw a textured quad:
    int corner = <VertexID>;
    if(corner == 0) {
        gl_Position = vec4(image_rect.x, image_rect.y, 0.0, 1.0);
        texcoord_fs = vec2(0.0, 1.0);
    }
    else if(corner == 1) {
        gl_Position = vec4(image_rect.z, image_rect.y, 0.0, 1.0);
        texcoord_fs = vec2(1.0, 1.0);
    }
    else if(corner == 2) {
        gl_Position = vec4(image_rect.x, image_rect.w, 0.0, 1.0);
        texcoord_fs = vec2(0.0, 0.0);
    }
    else if(corner == 3) {
        gl_Position = vec4(image_rect.z, image_rect.w, 0.0, 1.0);
        texcoord_fs = vec2(1.0, 0.0);
    }
}
