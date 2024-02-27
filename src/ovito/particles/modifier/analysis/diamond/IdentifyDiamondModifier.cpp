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

#include <ovito/particles/Particles.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/particles/modifier/analysis/cna/CommonNeighborAnalysisModifier.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "IdentifyDiamondModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(IdentifyDiamondModifier);

/******************************************************************************
 * Constructs the modifier object.
 ******************************************************************************/
IdentifyDiamondModifier::IdentifyDiamondModifier(ObjectInitializationFlags flags) : StructureIdentificationModifier(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the structure types.
        createStructureType(OTHER, ParticleType::PredefinedStructureType::OTHER);
        createStructureType(CUBIC_DIAMOND, ParticleType::PredefinedStructureType::CUBIC_DIAMOND);
        createStructureType(CUBIC_DIAMOND_FIRST_NEIGH, ParticleType::PredefinedStructureType::CUBIC_DIAMOND_FIRST_NEIGH);
        createStructureType(CUBIC_DIAMOND_SECOND_NEIGH, ParticleType::PredefinedStructureType::CUBIC_DIAMOND_SECOND_NEIGH);
        createStructureType(HEX_DIAMOND, ParticleType::PredefinedStructureType::HEX_DIAMOND);
        createStructureType(HEX_DIAMOND_FIRST_NEIGH, ParticleType::PredefinedStructureType::HEX_DIAMOND_FIRST_NEIGH);
        createStructureType(HEX_DIAMOND_SECOND_NEIGH, ParticleType::PredefinedStructureType::HEX_DIAMOND_SECOND_NEIGH);
    }
}

/******************************************************************************
 * Creates and initializes a computation engine that will compute the
 * modifier's results.
 ******************************************************************************/
Future<ModifierEnginePtr> IdentifyDiamondModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // If pipeline is in interactive mode, skip the long-running computation step.
    if(request.interactiveMode())
        return {};

    // Get modifier input.
    const Particles* particles = input.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);
    const SimulationCell* simCell = input.expectObject<SimulationCell>();
    if(simCell->is2D())
        throw Exception(tr("The modifier does not support 2d simulation cells."));

    // Get particle selection.
    const Property* selectionProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    return std::make_shared<DiamondIdentificationEngine>(request, particles, posProperty, simCell, structureTypes(), selectionProperty);
}

/******************************************************************************
 * Performs the actual analysis. This method is executed in a worker thread.
 ******************************************************************************/
void IdentifyDiamondModifier::DiamondIdentificationEngine::perform()
{
    setProgressText(tr("Finding nearest neighbors"));

    // Prepare the neighbor list builder.
    NearestNeighborFinder neighborFinder(4);
    if(!neighborFinder.prepare(positions(), cell(), selection())) return;

    // This data structure stores information about a single neighbor.
    struct NeighborInfo {
        Vector3 vec;
        int index;
    };
    // This array will be filled with the four nearest neighbors of each atom.
    std::vector<std::array<NeighborInfo, 4>> neighLists(positions()->size());

    // Determine four nearest neighbors of each atom and store vectors in the working array.
    BufferReadAccess<SelectionIntType> selectionData(selection());
    parallelForWithProgress(positions()->size(), [&](size_t index) {
        // Skip particles that are not included in the analysis.
        if(selectionData && selectionData[index] == 0) return;
        NearestNeighborFinder::Query<4> neighQuery(neighborFinder);
        neighQuery.findNeighbors(index);
        for(int i = 0; i < neighQuery.results().size(); i++) {
            neighLists[index][i].vec = neighQuery.results()[i].delta;
            neighLists[index][i].index = neighQuery.results()[i].index;
            OVITO_ASSERT(!selectionData || selectionData[neighLists[index][i].index]);
        }
        for(int i = neighQuery.results().size(); i < 4; i++) {
            neighLists[index][i].vec.setZero();
            neighLists[index][i].index = -1;
        }
    });
    if(isCanceled()) return;

    // Create output storage.
    BufferWriteAccess<int32_t, access_mode::discard_read_write> output(structures());

    // Perform structure identification.
    setProgressText(tr("Identifying diamond structures"));
    parallelForWithProgress(positions()->size(), [&](size_t index) {
        // Mark atom as 'other' by default.
        output[index] = OTHER;

        // Skip particles that are not included in the analysis.
        if(selectionData && selectionData[index] == 0) return;

        const std::array<NeighborInfo, 4>& nlist = neighLists[index];

        // Generate list of second nearest neighbors.
        std::array<Vector3, 12> secondNeighbors;
        auto vout = secondNeighbors.begin();
        for(size_t i = 0; i < 4; i++) {
            if(nlist[i].index == -1) return;
            const Vector3& v0 = nlist[i].vec;
            const std::array<NeighborInfo, 4>& nlist2 = neighLists[nlist[i].index];
            for(size_t j = 0; j < 4; j++) {
                Vector3 v = v0 + nlist2[j].vec;
                if(v.isZero(1e-2f)) continue;
                if(vout == secondNeighbors.end()) return;
                *vout++ = v;
            }
            if(vout != secondNeighbors.begin() + i * 3 + 3) return;
        }

        // Compute a local CNA cutoff radius from the average distance of the 12 second nearest neighbors.
        FloatType sum = 0;
        for(const Vector3& v : secondNeighbors) sum += v.length();
        sum /= 12;
        const FloatType factor = FloatType(1.2071068);  // = sqrt(2.0) * ((1.0 + sqrt(0.5)) / 2)
        FloatType localCutoff = sum * factor;
        FloatType localCutoffSquared = localCutoff * localCutoff;

        // Determine bonds between common neighbors using local cutoff.
        CommonNeighborAnalysisModifier::NeighborBondArray neighborArray;
        for(int ni1 = 0; ni1 < 12; ni1++) {
            neighborArray.setNeighborBond(ni1, ni1, false);
            for(int ni2 = ni1 + 1; ni2 < 12; ni2++)
                neighborArray.setNeighborBond(ni1, ni2,
                                              (secondNeighbors[ni1] - secondNeighbors[ni2]).squaredLength() <= localCutoffSquared);
        }

        // Determine whether second nearest neighbors form FCC or HCP using common neighbor analysis.
        int n421 = 0;
        int n422 = 0;
        for(int ni = 0; ni < 12; ni++) {
            // Determine number of neighbors the two atoms have in common.
            unsigned int commonNeighbors;
            int numCommonNeighbors = CommonNeighborAnalysisModifier::findCommonNeighbors(neighborArray, ni, commonNeighbors);
            if(numCommonNeighbors != 4) return;

            // Determine the number of bonds among the common neighbors.
            CommonNeighborAnalysisModifier::CNAPairBond neighborBonds[12 * 12];
            int numNeighborBonds = CommonNeighborAnalysisModifier::findNeighborBonds(neighborArray, commonNeighbors, 12, neighborBonds);
            if(numNeighborBonds != 2) return;

            // Determine the number of bonds in the longest continuous chain.
            int maxChainLength = CommonNeighborAnalysisModifier::calcMaxChainLength(neighborBonds, numNeighborBonds);
            if(maxChainLength == 1)
                n421++;
            else if(maxChainLength == 2)
                n422++;
            else
                return;
        }
        if(n421 == 12 && typeIdentificationEnabled(CUBIC_DIAMOND))
            output[index] = CUBIC_DIAMOND;
        else if(n421 == 6 && n422 == 6 && typeIdentificationEnabled(HEX_DIAMOND))
            output[index] = HEX_DIAMOND;
    });
    if(isCanceled()) return;

    // Mark first neighbors of crystalline atoms.
    for(size_t index = 0; index < output.size(); index++) {
        int ctype = output[index];
        if(ctype != CUBIC_DIAMOND && ctype != HEX_DIAMOND) continue;
        if(selectionData && selectionData[index] == 0) continue;

        const std::array<NeighborInfo, 4>& nlist = neighLists[index];
        for(size_t i = 0; i < 4; i++) {
            OVITO_ASSERT(nlist[i].index != -1);
            if(output[nlist[i].index] == OTHER) {
                if(ctype == CUBIC_DIAMOND)
                    output[nlist[i].index] = CUBIC_DIAMOND_FIRST_NEIGH;
                else
                    output[nlist[i].index] = HEX_DIAMOND_FIRST_NEIGH;
            }
        }
    }

    // Mark second neighbors of crystalline atoms.
    for(size_t index = 0; index < output.size(); index++) {
        int ctype = output[index];
        if(ctype != CUBIC_DIAMOND_FIRST_NEIGH && ctype != HEX_DIAMOND_FIRST_NEIGH) continue;
        if(selectionData && selectionData[index] == 0) continue;

        const std::array<NeighborInfo, 4>& nlist = neighLists[index];
        for(size_t i = 0; i < 4; i++) {
            if(nlist[i].index != -1 && output[nlist[i].index] == OTHER) {
                if(ctype == CUBIC_DIAMOND_FIRST_NEIGH)
                    output[nlist[i].index] = CUBIC_DIAMOND_SECOND_NEIGH;
                else
                    output[nlist[i].index] = HEX_DIAMOND_SECOND_NEIGH;
            }
        }
    }

    // Release data that is no longer needed.
    releaseWorkingData();
}

/******************************************************************************
 * Injects the computed results of the engine into the data pipeline.
 ******************************************************************************/
void IdentifyDiamondModifier::DiamondIdentificationEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    StructureIdentificationEngine::applyResults(request, state);

    // Also output structure type counts, which have been computed by the base class.
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.OTHER"), QVariant::fromValue(getTypeCount(OTHER)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.CUBIC_DIAMOND"), QVariant::fromValue(getTypeCount(CUBIC_DIAMOND)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.CUBIC_DIAMOND_FIRST_NEIGHBOR"), QVariant::fromValue(getTypeCount(CUBIC_DIAMOND_FIRST_NEIGH)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.CUBIC_DIAMOND_SECOND_NEIGHBOR"), QVariant::fromValue(getTypeCount(CUBIC_DIAMOND_SECOND_NEIGH)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.HEX_DIAMOND"), QVariant::fromValue(getTypeCount(HEX_DIAMOND)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.HEX_DIAMOND_FIRST_NEIGHBOR"), QVariant::fromValue(getTypeCount(HEX_DIAMOND_FIRST_NEIGH)), request.modificationNode());
    state.addAttribute(QStringLiteral("IdentifyDiamond.counts.HEX_DIAMOND_SECOND_NEIGHBOR"), QVariant::fromValue(getTypeCount(HEX_DIAMOND_SECOND_NEIGH)), request.modificationNode());
}

}  // namespace Ovito::Particles
