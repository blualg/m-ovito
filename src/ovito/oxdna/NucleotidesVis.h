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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesVis.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering DNA nucleotides.
 */
class OVITO_OXDNA_EXPORT NucleotidesVis : public ParticlesVis
{
    OVITO_CLASS(NucleotidesVis)
    OVITO_CLASSINFO("DisplayName", "Nucleotides");

public:

    /// Constructor.
    explicit NucleotidesVis(ObjectInitializationFlags flags);

    /// Renders the visual element.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the visual element.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Determines the effective rendering colors for the backbone sites of the nucleotides.
    ConstPropertyPtr backboneColors(const Particles* particles, bool highlightSelection) const;

    /// Determines the effective rendering colors for the base sites of the nucleotides.
    ConstPropertyPtr nucleobaseColors(const Particles* particles, bool highlightSelection) const;

    /// Returns the typed particle property used to determine the rendering colors of particles (if no per-particle colors are defined).
    virtual const Property* getParticleTypeColorProperty(const Particles* particles) const override;

    /// Returns the typed particle property used to determine the rendering radii of particles (if no per-particle radii are defined).
    virtual const Property* getParticleTypeRadiusProperty(const Particles* particles) const override;

private:

    /// Controls the displa radius of cylinder elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cylinderRadius, setCylinderRadius, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
