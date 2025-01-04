////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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

#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include "VectorsModifierDelegates.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(VectorsExpressionSelectionModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(
    const DataCollection& input) const
{
    // Gather list of all vector objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsDeleteSelectedModifierDelegate);
OVITO_CLASSINFO(VectorsDeleteSelectedModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Vectors>()) return {DataObjectReference(&Vectors::OOClass())};
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> VectorsDeleteSelectedModifierDelegate::apply(
    const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState,
    const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([state = std::move(state)]() mutable {
        size_t numVectors = 0;
        size_t numSelected = 0;

        // Get the vectors (base points) selection.
        if(const Vectors* inputVectors = state.getObject<Vectors>()) {
            inputVectors->verifyIntegrity();
            numVectors += inputVectors->elementCount();
            if(ConstPropertyPtr selProperty = inputVectors->getProperty(Vectors::SelectionProperty)) {
                // Make sure we can safely modify the vectors object.
                Vectors* outputVectors = state.makeMutable(inputVectors);

                // Remove selection property.
                outputVectors->removeProperty(selProperty);

                // Delete the selected vector base points / positions.
                numSelected += outputVectors->deleteElements(std::move(selProperty));
            }
        }

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 vectors deleted (%3%)")
                                    .arg(numSelected)
                                    .arg(numVectors)
                                    .arg((FloatType)numSelected * 100 / (FloatType)std::max(numVectors, (size_t)1), 0, 'f', 1);
        state.combineStatus(statusMessage);

        return std::move(state);
    });
}

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsComputePropertyModifierDelegate);
OVITO_CLASSINFO(VectorsComputePropertyModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Vectors objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}  // namespace Ovito
