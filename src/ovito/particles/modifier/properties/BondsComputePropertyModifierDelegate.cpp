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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/dataset/DataSet.h>
#include "BondsComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondsComputePropertyModifierDelegate);
OVITO_CLASSINFO(BondsComputePropertyModifierDelegate, "DisplayName", "Bonds");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds())
            return { DataObjectReference(&Particles::OOClass()) };
    }
    return {};
}

/******************************************************************************
 * Launches the actual computations.
 ******************************************************************************/
Future<PipelineFlowState> BondsComputePropertyModifierDelegate::performComputation(
    const ComputePropertyModifier* modifier,
    ComputePropertyModificationNode* modNode,
    PipelineFlowState state,
    const PipelineFlowState& originalState,
    PropertyPtr outputProperty,
    ConstPropertyPtr selectionProperty,
    int frame) const
{
    // Initialize expression evaluator.
    auto evaluator = std::make_unique<BondExpressionEvaluator>();
    evaluator->initialize(modifier->expressions(), originalState, originalState.expectObject(inputContainerRef()), frame);

    // Store the list of input variables in the ModificationNode so that the UI component can display it to the user.
    modNode->setInputVariableNames(evaluator->inputVariableNames());
    modNode->setInputVariableTable(evaluator->inputVariableTable());

    // Notify the UI component that the list of variables should be refreshed.
    modifier->notifyDependents(ReferenceEvent::ObjectStatusChanged);
    modNode->notifyDependents(ReferenceEvent::ObjectStatusChanged);

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            outputProperty = std::move(outputProperty),
            selectionProperty = std::move(selectionProperty),
            evaluator = std::move(evaluator)]() mutable
    {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Computing property '%1'").arg(outputProperty->name()));

        RawBufferAccess<access_mode::write> outputAccessor(outputProperty, selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        BufferReadAccess<SelectionIntType> selectionAccessor(selectionProperty);

        EnumerableThreadSpecific<BondExpressionEvaluator::Worker> expressionWorkers;
        size_t componentCount = outputAccessor.componentCount();

        parallelForInnerOuter(outputProperty->size(), 10000, progress, [&](auto&& iterate) {
            BondExpressionEvaluator::Worker& worker = expressionWorkers.create(*evaluator);
            iterate([&](size_t i) {
                // Skip unselected particles if requested.
                if(selectionAccessor && !selectionAccessor[i])
                    return;

                for(size_t component = 0; component < componentCount; component++) {
                    // Compute expression value.
                    FloatType value = worker.evaluate(i, component);

                    // Store results in output property.
                    outputAccessor.set(i, component, value);
                }
            });
        });
        return std::move(state);
    });
}

}   // End of namespace
