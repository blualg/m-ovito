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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetworkObject.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "DislocationSliceModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationSliceModifierDelegate);
OVITO_CLASSINFO(DislocationSliceModifierDelegate, "DisplayName", "Dislocation lines");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> DislocationSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<DislocationNetworkObject>())
        return { DataObjectReference(&DislocationNetworkObject::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> DislocationSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());

    if(modifier->createSelection())
        return std::move(state);

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);

    for(const DataObject* obj : state.data()->objects()) {
        if(const DislocationNetworkObject* inputDislocations = dynamic_object_cast<DislocationNetworkObject>(obj)) {
            QVector<Plane3> planes = inputDislocations->cuttingPlanes();
            if(sliceWidth <= 0) {
                planes.push_back(plane);
            }
            else {
                planes.push_back(Plane3( plane.normal,  plane.dist + sliceWidth/2));
                planes.push_back(Plane3(-plane.normal, -plane.dist + sliceWidth/2));
            }
            DislocationNetworkObject* outputDislocations = state.makeMutable(inputDislocations);
            outputDislocations->setCuttingPlanes(std::move(planes));
        }
    }

    return std::move(state);
}

}   // End of namespace
