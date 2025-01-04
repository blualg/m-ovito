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

#pragma once


#include <ovito/core/Core.h>
#include "RendererResourceCache.h"

namespace Ovito {

/**
 * Abstract base class for all rendering primitives in OVITO:
 *
 *   - ParticlePrimitive
 *   - CylinderPrimitive
 *   - LinePrimitive
 *   - MeshPrimitive
 *   - TextPrimitive
 *   - ImagePrimitive
 *   - MarkerPrimitive
 */
class OVITO_CORE_EXPORT RenderingPrimitive
{
public:

	/// Virtual destructor.
	virtual ~RenderingPrimitive() = default;

	/// Computes the 3d bounding box of the primitive in local coordinate space.
	virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const { return Box3(); }

protected:

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function();
#endif
};

}   // End of namespace
