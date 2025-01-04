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

const float ambient = 0.4;
const float diffuse_strength = 1.0 - ambient;
const float shininess = 6.0;
const vec3 specular_lightdir = normalize(vec3(-1.8, 1.5, -0.2));

void outputFragment(in vec4 v_color, in float z)
{
    float alpha = v_color.a;
    float weight = pow(alpha + 0.01f, 4.0f) +
                   max(0.01f, min(3000.0f, 0.3f / (0.00001f + pow(abs(z) / 200.0f, 4.0f))));

    // gl_FragData[0]:
    <fragAccumulation> = vec4(v_color.rgb * alpha * weight, alpha);

    // gl_FragData[1].r:
    <fragRevealage> = alpha * weight;
}

void outputShadedRayAndDepth(in vec4 color, in vec3 surface_normal, in vec3 ray_dir, in float zdepth)
{
    float specular = pow(max(0.0, dot(reflect(specular_lightdir, surface_normal), ray_dir)), shininess) * 0.25;
    float diffuse = abs(surface_normal.z) * diffuse_strength;
    <fragDepth> = zdepth;
    outputFragment(vec4(color.rgb * (diffuse + ambient) + vec3(specular), color.a), zdepth);
}

void outputShadedRay(in vec4 color, in vec3 surface_normal, in vec3 ray_dir)
{
    float specular = pow(max(0.0, dot(reflect(specular_lightdir, surface_normal), ray_dir)), shininess) * 0.25;
    float diffuse = abs(surface_normal.z) * diffuse_strength;
    outputFragment(vec4(color.rgb * (diffuse + ambient) + vec3(specular), color.a), gl_FragCoord.z);
}

void outputShaded(in vec4 color, in vec3 surface_normal)
{
    vec3 ray_dir;
    if(is_perspective()) {
        vec2 view_c = ((gl_FragCoord.xy - viewport_origin) * inverse_viewport_size) - 1.0;
        ray_dir = normalize(vec3(inverse_projection_matrix * vec4(view_c.x, view_c.y, 1.0, 1.0)));
    }
    else {
        ray_dir = vec3(0.0, 0.0, -1.0);
    }
    outputShadedRay(color, surface_normal, ray_dir);
}

void outputFlat(in vec4 color)
{
    outputFragment(color, gl_FragCoord.z);
}
