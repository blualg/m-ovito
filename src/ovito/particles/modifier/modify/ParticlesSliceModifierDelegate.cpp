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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include "ParticlesSliceModifierDelegate.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ParticlesSliceModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Performs the actual rejection of particles.
******************************************************************************/
PipelineStatus ParticlesSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const Particles* inputParticles = state.expectObject<Particles>();
    inputParticles->verifyIntegrity();
    QString statusMessage = tr("%n input particles", 0, inputParticles->elementCount());

    SliceModifier* mod = static_object_cast<SliceModifier>(request.modifier());
    PropertyFactory<SelectionIntType> mask(Particles::OOClass(), inputParticles->elementCount(), Particles::SelectionProperty);

    // Get the required input properties.
    BufferReadAccess<Point3> posProperty = inputParticles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<SelectionIntType> selProperty = mod->applyToSelection() ? inputParticles->expectProperty(Particles::SelectionProperty) : nullptr;
    OVITO_ASSERT(posProperty.size() == mask.size());
    OVITO_ASSERT(!selProperty || selProperty.size() == mask.size());

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = mod->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    // Number of marked/selected particles.
    size_t numMarked = 0;

    auto m = mask.begin();
    if(sliceWidth <= 0) {
        if(selProperty) {
            const auto* s = selProperty.cbegin();
            for(const Point3& p : posProperty) {
                if(*s++ && plane.pointDistance(p) > 0) {
                    *m = 1;
                    numMarked++;
                }
                else *m = 0;
                ++m;
            }
        }
        else {
            for(const Point3& p : posProperty) {
                if(plane.pointDistance(p) > 0) {
                    *m = 1;
                    numMarked++;
                }
                else *m = 0;
                ++m;
            }
        }
    }
    else {
        bool invert = mod->inverse();
        if(selProperty) {
            const auto* s = selProperty.cbegin();
            for(const Point3& p : posProperty) {
                if(*s++ && invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                    *m = 1;
                    numMarked++;
                }
                else *m = 0;
                ++m;
            }
        }
        else {
            for(const Point3& p : posProperty) {
                if(invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                    *m = 1;
                    numMarked++;
                }
                else *m = 0;
                ++m;
            }
        }
    }
    OVITO_ASSERT(m == mask.end());
    posProperty.reset();
    selProperty.reset();

    // Make sure we can safely modify the particles object.
    Particles* outputParticles = state.makeMutable(inputParticles);
    if(mod->createSelection() == false) {
        // Delete the marked particles.
        outputParticles->deleteElements(mask.take(), numMarked);
        statusMessage += tr("\n%n particles deleted", 0, numMarked);
        statusMessage += tr("\n%n particles remaining", 0, outputParticles->elementCount());
    }
    else {
        // Create or replace the selection particle property.
        outputParticles->createProperty(mask.take());
        statusMessage += tr("\n%n particles selected", 0, numMarked);
        statusMessage += tr("\n%n particles unselected", 0, outputParticles->elementCount() - numMarked);
    }
    outputParticles->verifyIntegrity();

    return PipelineStatus(PipelineStatus::Success, statusMessage);
}

}   // End of namespace
