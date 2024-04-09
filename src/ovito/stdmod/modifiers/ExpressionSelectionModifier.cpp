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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include "ExpressionSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ExpressionSelectionModifier);
OVITO_CLASSINFO(ExpressionSelectionModifier, "DisplayName", "Expression selection");
OVITO_CLASSINFO(ExpressionSelectionModifier, "Description", "Select particles or other elements using a user-defined criterion.");
OVITO_CLASSINFO(ExpressionSelectionModifier, "ModifierCategory", "Selection");
DEFINE_PROPERTY_FIELD(ExpressionSelectionModifier, expression);
SET_PROPERTY_FIELD_LABEL(ExpressionSelectionModifier, expression, "Boolean expression");

IMPLEMENT_ABSTRACT_OVITO_CLASS(ExpressionSelectionModifierDelegate);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ExpressionSelectionModifier::ExpressionSelectionModifier(ObjectInitializationFlags flags) : DelegatingModifier(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Let this modifier operate on particles by default.
        createDefaultModifierDelegate(ExpressionSelectionModifierDelegate::OOClass(), QStringLiteral("ParticlesExpressionSelectionModifierDelegate"));
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ExpressionSelectionModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(ExpressionSelectionModifier::expression) && !isBeingLoaded()) {
        // Changes of some modifier parameters affect the result of ExpressionSelectionModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    DelegatingModifier::propertyChanged(field);
}

/******************************************************************************
 * Checks if math expressions are time-dependent, i.e. whether they involve the animation frame number.
 ******************************************************************************/
bool ExpressionSelectionModifierDelegate::isExpressionTimeDependent(ExpressionSelectionModifier* modifier) const
{
    // This is a very simple check for the presence of the word "Frame" in the expression.
    // It's not perfect, but should catch all relevant cases (maybe more).
    if(modifier->expression().contains(QLatin1String("Frame")))
        return true;
    return false;
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void ExpressionSelectionModifierDelegate::preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Determine whether math expressions are time-dependent, i.e. whether they involve the current animation
    // frame number. If so, then we have to restrict the validity interval of the computation results
    // to the current animation time.
    if(isExpressionTimeDependent(static_object_cast<ExpressionSelectionModifier>(request.modifier()))) {
        validityInterval.intersect(request.time());
    }
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ExpressionSelectionModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ExpressionSelectionModifier* modifier = static_object_cast<ExpressionSelectionModifier>(request.modifier());

    // The current animation frame number.
    int currentFrame = request.time().frame(); // Note: Using global animation frame here, because that's what the user expects.

    // Initialize the evaluator class.
    std::unique_ptr<PropertyExpressionEvaluator> evaluator = initializeExpressionEvaluator(QStringList(modifier->expression()), originalState, originalState.expectObject(inputContainerRef()), currentFrame);

    // Save list of available input variables, which will be displayed in the modifier's UI.
    modifier->setVariablesInfo(evaluator->inputVariableNames(), evaluator->inputVariableTable());

    // If the user has not entered an expression yet, let them know.
    if(modifier->expression().trimmed().isEmpty()) {
        if(ExecutionContext::isInteractive()) {
            state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("Please enter a Boolean expression.")));
            return std::move(state);
        }
        throw Exception(tr("Modifier has no expression set. Did you forget to specify the selection expression?"));
    }

    // Check if expression contains an assignment ('=' operator).
    // This should be considered a user's mistake, because the user is probably referring the comparison operator '=='.
    if(modifier->expression().contains(QRegularExpression(QStringLiteral("[^=!><]=(?!=)"))))
        throw Exception(tr("The expression contains the assignment operator '='. Please use the comparison operator '==' instead."));

    // Make the property container mutable.
    DataObjectPath objectPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());

    // Generate the output selection property.
    PropertyPtr selection = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);

    // The actual computation can be performed in a separate worker thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([
            state = std::move(state),
            selection = std::move(selection),
            evaluator = std::move(evaluator),
            createdByNode = request.modificationNode()]() mutable
    {
        // The number of selected elements.
        std::atomic_size_t nselected(0);

        // Write the output selection property.
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> selectionAcc{selection};

        // Evaluate Boolean expression for every input data element.
        EnumerableThreadSpecific<PropertyExpressionEvaluator::Worker> expressionWorkers;
        parallelForInnerOuter(selection->size(), 4096, [&](auto&& iterate) {
            PropertyExpressionEvaluator::Worker& worker = expressionWorkers.create(*evaluator);
            size_t nselectedLocal = 0;
            iterate([&](size_t i) {
                if(worker.evaluate(i, 0)) {
                    selectionAcc[i] = 1;
                    nselectedLocal++;
                }
                else {
                    selectionAcc[i] = 0;
                }
            });
            nselected.fetch_add(nselectedLocal, std::memory_order_relaxed);
        });

        // To speed up future queries, store the selection count in the selection property object.
        selection->setNonzeroCount(nselected.load());

        // Report the total number of selected elements as a pipeline attribute.
        state.addAttribute(QStringLiteral("ExpressionSelection.count"), QVariant::fromValue(nselected.load()), createdByNode);

        // Update status display in the UI.
        QString statusMessage = tr("%1 out of %2 elements selected (%3%)").arg(nselected.load()).arg(selection->size()).arg((FloatType)nselected.load() * 100 / std::max((size_t)1,selection->size()), 0, 'f', 1);
        state.setStatus(PipelineStatus(std::move(statusMessage)));

        return std::move(state);
    });
}

/******************************************************************************
* Creates and initializes the expression evaluator object.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> ExpressionSelectionModifierDelegate::initializeExpressionEvaluator(const QStringList& expressions, const PipelineFlowState& inputState, const ConstDataObjectPath& containerPath, int animationFrame)
{
    std::unique_ptr<PropertyExpressionEvaluator> evaluator = std::make_unique<PropertyExpressionEvaluator>();
    evaluator->initialize(expressions, inputState, containerPath, animationFrame);
    return evaluator;
}

}   // End of namespace
