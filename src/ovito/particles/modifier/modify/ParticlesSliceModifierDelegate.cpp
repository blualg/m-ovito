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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/app/Application.h>
#include "ParticlesSliceModifierDelegate.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParticlesSliceModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<ParticlesObject>())
        return { DataObjectReference(&ParticlesObject::OOClass()) };
    return {};
}

/******************************************************************************
* Performs the actual rejection of particles.
******************************************************************************/
PipelineStatus ParticlesSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const ParticlesObject* inputParticles = state.expectObject<ParticlesObject>();
    inputParticles->verifyIntegrity();
    QString statusMessage = tr("%n input particles", 0, inputParticles->elementCount());

    SliceModifier* mod = static_object_cast<SliceModifier>(request.modifier());
    boost::dynamic_bitset<> mask(inputParticles->elementCount());

    // Get the required input properties.
    ConstDataBufferAccess<Point3> posProperty = inputParticles->expectProperty(ParticlesObject::PositionProperty);
    ConstDataBufferAccess<SelectionIntType> selProperty = mod->applyToSelection() ? inputParticles->expectProperty(ParticlesObject::SelectionProperty) : nullptr;
    OVITO_ASSERT(posProperty.size() == mask.size());
    OVITO_ASSERT(!selProperty || selProperty.size() == mask.size());

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = mod->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    if(sliceWidth <= 0) {
        if(selProperty) {
            const auto* s = selProperty.cbegin();
            boost::dynamic_bitset<>::size_type i = 0;
            for(const Point3& p : posProperty) {
                if(*s++ && plane.pointDistance(p) > 0)
                    mask.set(i);
                ++i;
            }
        }
        else {
            boost::dynamic_bitset<>::size_type i = 0;
            for(const Point3& p : posProperty) {
                if(plane.pointDistance(p) > 0)
                    mask.set(i);
                ++i;
            }
        }
    }
    else {
        bool invert = mod->inverse();
        if(selProperty) {
            const auto* s = selProperty.cbegin();
            boost::dynamic_bitset<>::size_type i = 0;
            for(const Point3& p : posProperty) {
                if(*s++ && invert == (plane.classifyPoint(p, sliceWidth) == 0))
                    mask.set(i);
                ++i;
            }
        }
        else {
            boost::dynamic_bitset<>::size_type i = 0;
            for(const Point3& p : posProperty) {
                if(invert == (plane.classifyPoint(p, sliceWidth) == 0))
                    mask.set(i);
                ++i;
            }
        }
    }
    posProperty.reset();
    selProperty.reset();

    // Make sure we can safely modify the particles object.
    ParticlesObject* outputParticles = state.makeMutable(inputParticles);
    if(mod->createSelection() == false) {

        // Delete the selected particles.
        size_t numDeleted = outputParticles->deleteElements(mask);
        statusMessage += tr("\n%n particles deleted", 0, numDeleted);
        statusMessage += tr("\n%n particles remaining", 0, outputParticles->elementCount());
    }
    else {
        size_t numSelected = 0;
        DataBufferAccess<SelectionIntType> newSelProperty = outputParticles->createProperty(ParticlesObject::SelectionProperty);
        OVITO_ASSERT(mask.size() == newSelProperty.size());
        boost::dynamic_bitset<>::size_type i = 0;
        for(auto& s : newSelProperty) {
            if(mask.test(i++)) {
                s = 1;
                numSelected++;
            }
            else s = 0;
        }

        statusMessage += tr("\n%n particles selected", 0, numSelected);
        statusMessage += tr("\n%n particles unselected", 0, outputParticles->elementCount() - numSelected);
    }
    outputParticles->verifyIntegrity();

    return PipelineStatus(PipelineStatus::Success, statusMessage);
}

}   // End of namespace
