////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesVis.h>

namespace Ovito::Particles {

/**
 * \brief A visualization element for rendering DNA nucleotides.
 */
class OVITO_OXDNA_EXPORT NucleotidesVis : public ParticlesVis
{
	OVITO_CLASS(NucleotidesVis)
	Q_CLASSINFO("DisplayName", "Nucleotides");

public:

	/// Constructor.
	Q_INVOKABLE NucleotidesVis(ObjectCreationParams params);

	/// Renders the visual element.
	virtual PipelineStatus render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode) override;

	/// Computes the bounding box of the visual element.
	virtual Box3 boundingBox(AnimationTime time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval) override;

	/// Determines the effective rendering colors for the backbone sites of the nucleotides.
	ConstPropertyPtr backboneColors(const ParticlesObject* particles, bool highlightSelection) const;

	/// Determines the effective rendering colors for the base sites of the nucleotides.
	ConstPropertyPtr nucleobaseColors(const ParticlesObject* particles, bool highlightSelection) const;

	/// Returns the typed particle property used to determine the rendering colors of particles (if no per-particle colors are defined).
	virtual const PropertyObject* getParticleTypeColorProperty(const ParticlesObject* particles) const override;

	/// Returns the typed particle property used to determine the rendering radii of particles (if no per-particle radii are defined).
	virtual const PropertyObject* getParticleTypeRadiusProperty(const ParticlesObject* particles) const override;

private:

	/// Controls the displa radius of cylinder elements.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cylinderRadius, setCylinderRadius, PROPERTY_FIELD_MEMORIZE);
};

}	// End of namespace
