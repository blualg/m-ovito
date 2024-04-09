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


#include <ovito/vorotop/VoroTopPlugin.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include "Filter.h"

namespace voro {
    class voronoicell_neighbor; // Defined by Voro++
}

namespace Ovito::VoroTop {

/**
 * \brief This analysis modifier performs the Voronoi topology analysis developed by Emanuel A. Lazar.
 */
class OVITO_VOROTOP_EXPORT VoroTopModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(VoroTopModifier)

public:

    /// Constructor.
    explicit VoroTopModifier(ObjectInitializationFlags flags);

    /// Loads a new filter definition into the modifier.
    void loadFilterDefinition(const QString& filepath);

    /// Returns the VoroTop filter definition cached from the last analysis run.
    const std::shared_ptr<Filter>& filter() const { return _filter; }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) override {
        const Particles* particles = input.expectObject<Particles>();
        return std::make_shared<VoroTopAnalysisAlgorithm>(std::move(structures), useRadii() ? particles->inputParticleRadii() : nullptr, filterFile(), filter());
    }

private:

    /// Compute engine that performs the actual analysis in a background thread.
    class VoroTopAnalysisAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        VoroTopAnalysisAlgorithm(PropertyPtr structures, ConstPropertyPtr radii, const QString& filterFile, std::shared_ptr<Filter> filter) :
            Algorithm(std::move(structures)),
            _filterFile(filterFile),
            _filter(std::move(filter)),
            _radii(std::move(radii)) {}

        /// Performs the atomic structure classification.
        virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

        /// Processes a single Voronoi cell.
        int processCell(voro::voronoicell_neighbor& vcell);

        /// Returns the VoroTop filter definition.
        const std::shared_ptr<Filter>& filter() const { return _filter; }

    private:

        /// The path of the external file containing the filter definition.
        QString _filterFile;

        /// The VoroTop filter definition.
        std::shared_ptr<Filter> _filter;

        /// The per-particle radii.
        ConstPropertyPtr _radii;
    };

private:

    /// Controls whether the weighted Voronoi tessellation is computed, which takes into account particle radii.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, useRadii, setUseRadii);

    /// The external file path of the loaded filter file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, filterFile, setFilterFile);

    /// The VoroTop filter definition cached from the last analysis run.
    std::shared_ptr<Filter> _filter;
};

}   // End of namespace
