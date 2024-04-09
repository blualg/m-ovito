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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "DislocationAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationAffineTransformationModifierDelegate);
OVITO_CLASSINFO(DislocationAffineTransformationModifierDelegate, "DisplayName", "Dislocations");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> DislocationAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<DislocationNetworkObject>())
        return { DataObjectReference(&DislocationNetworkObject::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> DislocationAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    if(modifier->selectionOnly())
        return std::move(state);

    // The actual work can be performed in a separate thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState)]() mutable {

        for(const DataObject* obj : state.data()->objects()) {
            if(const DislocationNetworkObject* inputDislocations = dynamic_object_cast<DislocationNetworkObject>(obj)) {
                DislocationNetworkObject* outputDislocations = state.makeMutable(inputDislocations);

                // Apply transformation to the vertices of the dislocation lines.
                for(DislocationSegment* segment : outputDislocations->modifiableSegments()) {
                    for(Point3& vertex : segment->line) {
                        vertex = tm * vertex;
                    }
                }

                // Apply transformation to the cutting planes attached to the dislocation network.
                QVector<Plane3> cuttingPlanes = outputDislocations->cuttingPlanes();
                for(Plane3& plane : cuttingPlanes)
                    plane = tm * plane;
                outputDislocations->setCuttingPlanes(std::move(cuttingPlanes));
            }
        }

        return std::move(state);
    });
}

}   // End of namespace
