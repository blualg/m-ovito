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

const float ambient = 0.4;
const float diffuse_strength = 1.0 - ambient;
const float shininess = 6.0;
const vec3 specular_lightdir = normalize(vec3(-1.8, 1.5, -0.2));

vec4 shadeSurfaceColor(in vec3 surface_normal, in vec4 color)
{
    if(is_perspective()) {
        vec2 view_c = ((gl_FragCoord.xy - GlobalUniforms.viewport_origin) * GlobalUniforms.inverse_viewport_size) - 1.0;
        vec3 ray_dir = normalize(vec3(GlobalUniforms.inverse_projection_matrix * vec4(view_c.x, view_c.y, 1.0, 1.0)));
        float diffuse = abs(surface_normal.z) * diffuse_strength;
        float specular = pow(max(0.0, dot(reflect(specular_lightdir, surface_normal), ray_dir)), shininess) * 0.25;
        return vec4(color.rgb * (diffuse + ambient) + vec3(specular), color.a);
    }
    else {
        float reflz = max(0.0, 2.0 * dot(surface_normal, specular_lightdir) * surface_normal.z - specular_lightdir.z);

        // Compute pow(reflz, shininess):
        float specular = reflz * reflz;
        specular = 0.25 * specular * specular * specular;

        float diffuse = abs(surface_normal.z) * diffuse_strength;

        return vec4(color.rgb * (diffuse + ambient) + vec3(specular), color.a);
    }
}
