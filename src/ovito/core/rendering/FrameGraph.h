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

/**
 * \file SceneRenderer.h
 * \brief Contains the definition of the Ovito::SceneRenderer class.
 */

#pragma once


#include <ovito/core/Core.h>
#
namespace Ovito {

class OVITO_CORE_EXPORT FrameGraph
{
public:

	/// Changes the current local-to-world transformation matrix.
	void setWorldTransform(const AffineTransformation& tm) {
		_modelWorldTM = tm;
		_modelViewTM = projParams().viewMatrix * tm;
	}

	/// Returns the current local-to-world transformation matrix.
	const AffineTransformation& worldTransform() const { return _modelWorldTM; }

	/// Returns the current model-to-view transformation matrix.
	const AffineTransformation& modelViewTM() const { return _modelViewTM; }

	/// Renders the line geometry stored in the given buffer.
	virtual void renderLines(const LinePrimitive& primitive) {}

	/// Renders the particles stored in the given primitive buffer.
	virtual void renderParticles(const ParticlePrimitive& primitive) {}

	/// Renders the marker geometry stored in the given buffer.
	virtual void renderMarkers(const MarkerPrimitive& primitive) {}

	/// Renders the text stored in the given primitive buffer.
	virtual void renderText(const TextPrimitive& primitive) {}

	/// Renders the image stored in the given primitive buffer.
	virtual void renderImage(const ImagePrimitive& primitive) {}

	/// Renders the cylinder or arrow elements stored in the given buffer.
	virtual void renderCylinders(const CylinderPrimitive& primitive) {}

	/// Renders the triangle mesh stored in the given buffer.
	virtual void renderMesh(const MeshPrimitive& primitive) {}

	/// Renders a 2d polyline or polygon into an interactive viewport.
	void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed);
};

}	// End of namespace
