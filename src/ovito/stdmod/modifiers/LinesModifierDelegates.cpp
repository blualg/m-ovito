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

#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include "LinesModifierDelegates.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesExpressionSelectionModifierDelegate);
OVITO_CLASSINFO(LinesExpressionSelectionModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesExpressionSelectionModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesDeleteSelectedModifierDelegate);
OVITO_CLASSINFO(LinesDeleteSelectedModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Lines>()) return {DataObjectReference(&Lines::OOClass())};
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> LinesDeleteSelectedModifierDelegate::apply(
    const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState,
    const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([state = std::move(state)]() mutable {
        size_t numLines = 0;
        size_t numSelected = 0;

        // Get the lines (vertex) selection.
        if(const Lines* inputLines = state.getObject<Lines>()) {
            inputLines->verifyIntegrity();
            numLines += inputLines->elementCount();
            if(ConstPropertyPtr selProperty = inputLines->getProperty(Lines::SelectionProperty)) {
                // Make sure we can safely modify the lines object.
                Lines* outputLines = state.makeMutable(inputLines);

                // Remove selection property.
                outputLines->removeProperty(selProperty);

                // Delete the selected line vertices.
                numSelected += outputLines->deleteElements(std::move(selProperty));
            }
        }

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 lines deleted (%3%)")
                                    .arg(numSelected)
                                    .arg(numLines)
                                    .arg((FloatType)numSelected * 100 / (FloatType)std::max(numLines, (size_t)1), 0, 'f', 1);
        state.combineStatus(statusMessage);

        return std::move(state);
    });
}

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesComputePropertyModifierDelegate);
OVITO_CLASSINFO(LinesComputePropertyModifierDelegate, "DisplayName", "Lines");

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all Lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}  // namespace Ovito
