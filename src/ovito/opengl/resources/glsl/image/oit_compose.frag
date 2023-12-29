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

// Texture samplers:
uniform sampler2D accumulationTex;
uniform sampler2D revealageTex;

// Inputs:
in vec2 texcoord_fs;

void main()
{
	// Post processing (compositing) fragment shader:

	vec4 accum = <texture2D>(accumulationTex, texcoord_fs);
	float r = accum.a;
	if(r >= 1.0)
        discard;
	accum.a = <texture2D>(revealageTex, texcoord_fs).r;
//	<fragColor> = vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), r);
	<fragColor> = vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), 1.0-r);
}
