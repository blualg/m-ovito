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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/app/Application.h>
#include "ExpressionSelectionModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ExpressionSelectionModifier);
DEFINE_PROPERTY_FIELD(ExpressionSelectionModifier, expression);
SET_PROPERTY_FIELD_LABEL(ExpressionSelectionModifier, expression, "Boolean expression");

IMPLEMENT_OVITO_CLASS(ExpressionSelectionModifierDelegate);

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
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus ExpressionSelectionModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ExpressionSelectionModifier* expressionMod = static_object_cast<ExpressionSelectionModifier>(request.modifier());

    // The current animation frame number.
    int currentFrame = request.time().frame(); // Note: Using global animation frame here, because that's what the user expects.

    // Look up the input property container.
    DataObjectPath objectPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());

    // Initialize the evaluator class.
    std::unique_ptr<PropertyExpressionEvaluator> evaluator = initializeExpressionEvaluator(QStringList(expressionMod->expression()), state, objectPath, currentFrame);

    // Save list of available input variables, which will be displayed in the modifier's UI.
    expressionMod->setVariablesInfo(evaluator->inputVariableNames(), evaluator->inputVariableTable());

    // If the user has not entered an expression yet, let them know.
    if(expressionMod->expression().trimmed().isEmpty()) {
        if(ExecutionContext::isInteractive())
            return PipelineStatus(PipelineStatus::Warning, tr("Please enter a Boolean expression."));
        else
            throw Exception(tr("Modifier has no expression set. Did you forget to specify the selection expression?"));
    }

    // Check if expression contains an assignment ('=' operator).
    // This should be considered a user's mistake, because the user is probably referring the comparison operator '=='.
    if(expressionMod->expression().contains(QRegularExpression(QStringLiteral("[^=!><]=(?!=)"))))
        throw Exception(tr("The expression contains the assignment operator '='. Please use the comparison operator '==' instead."));

    // The number of selected elements.
    std::atomic_size_t nselected(0);

    // Generate the output selection property.
    PropertyAccess<int> selProperty = container->createProperty(PropertyObject::GenericSelectionProperty);

    // Evaluate Boolean expression for every input data element.
    evaluator->evaluate([&selProperty, &nselected](size_t elementIndex, size_t componentIndex, double value) {
        if(value) {
            selProperty[elementIndex] = 1;
            ++nselected;
        }
        else {
            selProperty[elementIndex] = 0;
        }
    });

    // If the expression contains a time-dependent term, then we have to restrict the validity interval
    // of the generated selection to the current animation time.
    if(evaluator->isTimeDependent())
        state.intersectStateValidity(request.time());

    // Report the total number of selected elements as a pipeline attribute.
    state.addAttribute(QStringLiteral("ExpressionSelection.count"), QVariant::fromValue(nselected.load()), request.modApp());
    // For backward compatibility with OVITO 2.9.0.
    state.addAttribute(QStringLiteral("SelectExpression.num_selected"), QVariant::fromValue(nselected.load()), request.modApp());

    // Update status display in the UI.
    QString statusMessage = tr("%1 out of %2 elements selected (%3%)").arg(nselected.load()).arg(selProperty.size()).arg((FloatType)nselected.load() * 100 / std::max((size_t)1,selProperty.size()), 0, 'f', 1);
    return PipelineStatus(std::move(statusMessage));
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
