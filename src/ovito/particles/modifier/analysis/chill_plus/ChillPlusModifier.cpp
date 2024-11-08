////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
//  Copyright 2019 Henrik Andersen Sveinsson
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

#include <ovito/particles/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "ChillPlusModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ChillPlusModifier);
OVITO_CLASSINFO(ChillPlusModifier, "DisplayName", "Chill+");
OVITO_CLASSINFO(ChillPlusModifier, "Description", "Identify hexagonal ice, cubic ice, hydrate and other arrangements of water molecules.");
OVITO_CLASSINFO(ChillPlusModifier, "ModifierCategory", "Structure identification");
DEFINE_PROPERTY_FIELD(ChillPlusModifier, cutoff);
SET_PROPERTY_FIELD_LABEL(ChillPlusModifier, cutoff, "Cutoff radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ChillPlusModifier, cutoff, WorldParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
void ChillPlusModifier::initializeObject(ObjectInitializationFlags flags)
{
    StructureIdentificationModifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the structure types.
        createStructureType(OTHER, ParticleType::PredefinedStructureType::OTHER);
        createStructureType(HEXAGONAL_ICE, ParticleType::PredefinedStructureType::HEXAGONAL_ICE);
        createStructureType(CUBIC_ICE, ParticleType::PredefinedStructureType::CUBIC_ICE);
        createStructureType(INTERFACIAL_ICE, ParticleType::PredefinedStructureType::INTERFACIAL_ICE);
        createStructureType(HYDRATE, ParticleType::PredefinedStructureType::HYDRATE);
        createStructureType(INTERFACIAL_HYDRATE, ParticleType::PredefinedStructureType::INTERFACIAL_HYDRATE);
    }
}

/******************************************************************************
* Performs the actual analysis.
******************************************************************************/
void ChillPlusModifier::ChillPlusAlgorithm::identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection)
{
    if(simulationCell && simulationCell->is2D())
        throw Exception(tr("The Chill+ algorithm does not support 2d simulation cells."));

    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Computing q_lm values in Chill+ analysis"));

    // Prepare the neighbor list.
    CutoffNeighborFinder neighborFinder;
    neighborFinder.prepare(cutoff(), particles->expectProperty(Particles::PositionProperty), simulationCell, selection);

    BufferReadAccess<SelectionIntType> selectionAcc(selection);
    BufferWriteAccess<int32_t, access_mode::discard_write> structureAcc(structures());

    // Find all relevant q_lm
    // create matrix of q_lm
    size_t particleCount = particles->elementCount();
    boost::numeric::ublas::matrix<std::complex<float>> q_values(particleCount, 7);

    auto compute_q_lm = [&](size_t particleIndex, int l, int m) {
        std::complex<float> q = 0;
        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, particleIndex); !neighQuery.atEnd(); neighQuery.next()) {
            const Vector3& delta = neighQuery.delta();
            float asimuthal = std::atan2(delta.y(), delta.x());
            float xy_distance = std::sqrt(delta.x()*delta.x()+delta.y()*delta.y());
            float polar = std::atan2(xy_distance, delta.z());
            q += boost::math::spherical_harmonic(l, m, polar, asimuthal);
        }
        return q;
    };

    // Parallel calculation loop:
    parallelFor(particleCount, 1024, progress, [&](size_t index) {
        for(int m = -3; m <= 3; m++) {
            q_values(index, m+3) = compute_q_lm(index, 3, m);
        }
    });

    // For each particle, count the bonds and determine structure
    progress.setProgressText(tr("Computing c_ij values of Chill+"));
    parallelFor(particleCount, 4096, progress, [&](size_t index) {
        structureAcc[index] =
            (!selectionAcc || selectionAcc[index]) // Skip particles that are not included in the analysis.
                ? determineStructure(neighborFinder, index, q_values)
                : OTHER;
    });
}

/******************************************************************************
* Determines the structure of an atom based on the number of eclipsed and staggered bonds.
******************************************************************************/
ChillPlusModifier::StructureType ChillPlusModifier::ChillPlusAlgorithm::determineStructure(const CutoffNeighborFinder& neighFinder, size_t particleIndex, const boost::numeric::ublas::matrix<std::complex<float>>& q_values)
{
    int num_eclipsed = 0;
    int num_staggered = 0;
    int coordination = 0;
    for(CutoffNeighborFinder::Query neighQuery(neighFinder, particleIndex); !neighQuery.atEnd(); neighQuery.next()) {
        // Compute c(i,j)
        std::complex<float> c1 = 0;
        std::complex<float> c2 = 0;
        std::complex<float> c3 = 0;
        std::complex<float> q_i = 0;
        std::complex<float> q_j = 0;
        for(int m = -3; m <= 3; m++) {
            q_i = q_values(particleIndex, m+3);
            q_j = q_values(neighQuery.current(), m+3);
            c1 += q_i*std::conj(q_j);
            c2 += q_i*std::conj(q_i);
            c3 += q_j*std::conj(q_j);
        }
        std::complex<float> c_ij = c1/(std::sqrt(c2)*std::sqrt(c3));
        if(std::real(c_ij) > -0.35 && std::real(c_ij) < 0.25) {
            num_eclipsed ++;
        }
        if(std::real(c_ij) < -0.8) {
            num_staggered ++;
        }
        coordination++;
    }

    if(coordination == 4) {
        if(num_eclipsed == 4) {
            return HYDRATE;
        }
        else if(num_eclipsed == 3) {
            return INTERFACIAL_HYDRATE;
        }
        else if(num_staggered == 4) {
            return CUBIC_ICE;
        }
        else if(num_staggered == 3 && num_eclipsed == 1) {
            return HEXAGONAL_ICE;
        }
        else if(num_staggered == 3 && num_eclipsed == 0) {
            return INTERFACIAL_ICE;
        }
        else if(num_staggered == 2) {
            return INTERFACIAL_ICE;
        }
    }
    return OTHER;
}

/******************************************************************************
* Computes the structure identification statistics.
******************************************************************************/
std::vector<int64_t> ChillPlusModifier::ChillPlusAlgorithm::computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const
{
    std::vector<int64_t> typeCounts = StructureIdentificationModifier::Algorithm::computeStructureStatistics(structures, state, createdByNode, modifierParameters);

    // Also output structure type counts, which have been computed by the base class.
    state.addAttribute(QStringLiteral("ChillPlus.counts.OTHER"), QVariant::fromValue(typeCounts.at(OTHER)), createdByNode);
    state.addAttribute(QStringLiteral("ChillPlus.counts.CUBIC_ICE"), QVariant::fromValue(typeCounts.at(CUBIC_ICE)), createdByNode);
    state.addAttribute(QStringLiteral("ChillPlus.counts.HEXAGONAL_ICE"), QVariant::fromValue(typeCounts.at(HEXAGONAL_ICE)), createdByNode);
    state.addAttribute(QStringLiteral("ChillPlus.counts.INTERFACIAL_ICE"), QVariant::fromValue(typeCounts.at(INTERFACIAL_ICE)), createdByNode);
    state.addAttribute(QStringLiteral("ChillPlus.counts.HYDRATE"), QVariant::fromValue(typeCounts.at(HYDRATE)), createdByNode);
    state.addAttribute(QStringLiteral("ChillPlus.counts.INTERFACIAL_HYDRATE"), QVariant::fromValue(typeCounts.at(INTERFACIAL_HYDRATE)), createdByNode);

    return typeCounts;
}

}   // End of namespace
