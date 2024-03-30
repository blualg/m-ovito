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
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/Application.h>
#include "AssignColorModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(AssignColorModifierDelegate);

IMPLEMENT_CREATABLE_OVITO_CLASS(AssignColorModifier);
DEFINE_REFERENCE_FIELD(AssignColorModifier, colorController);
DEFINE_PROPERTY_FIELD(AssignColorModifier, keepSelection);
SET_PROPERTY_FIELD_LABEL(AssignColorModifier, colorController, "Color");
SET_PROPERTY_FIELD_LABEL(AssignColorModifier, keepSelection, "Keep selection");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
AssignColorModifier::AssignColorModifier(ObjectInitializationFlags flags) : DelegatingModifier(flags),
    // In the graphical environment, we clear the selection by default to make the assigned colors visible.
    _keepSelection(ExecutionContext::isScripting())
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setColorController(ControllerManager::createColorController());
        colorController()->setColorValue(AnimationTime(0), Color(0.3f, 0.3f, 1.0f));

        // Let this modifier operate on particles by default.
        createDefaultModifierDelegate(AssignColorModifierDelegate::OOClass(), QStringLiteral("ParticlesAssignColorModifierDelegate"));
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool AssignColorModifier::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == colorController()) {
        // Changes to some the modifier's parameters affect the result of AssignColorModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return DelegatingModifier::referenceEvent(source, event);
}

/******************************************************************************
* This function is called by the pipeline system before a new modifier evaluation begins.
******************************************************************************/
void AssignColorModifierDelegate::preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    const AssignColorModifier* modifier = static_object_cast<AssignColorModifier>(request.modifier());

    // If the color is animated, restrict the validity interval of the modifier results.
    if(modifier->colorController())
        validityInterval.intersect(modifier->colorController()->validityInterval(request.time()));
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
Future<PipelineFlowState> AssignColorModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const AssignColorModifier* modifier = static_object_cast<AssignColorModifier>(request.modifier());
    if(!modifier->colorController())
        return state;

    // Look up the property container object and make sure we can safely modify it.
    DataObjectPath containerPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Get the input selection property.
    ConstPropertyPtr selection;
    if(container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        if(const Property* property = container->getProperty(Property::GenericSelectionProperty)) {
            selection = property;

            // Clear selection if requested.
            if(!modifier->keepSelection())
                container->removeProperty(selection);
        }
    }

    // Get modifier's color parameter value.
    Color color;
    modifier->colorController()->getColorValue(request.time(), color, state.mutableStateValidity());

    // Create the color output property.
    Property* colorProperty = container->createProperty(selection ? DataBuffer::Initialized : DataBuffer::Uninitialized, outputColorPropertyId(), containerPath);

    // Assign color to selected elements (or all elements if there is no selection).
    colorProperty->fillSelected<ColorG>(color.toDataType<GraphicsFloatType>(), selection.get());

    return state;
}

}   // End of namespace
