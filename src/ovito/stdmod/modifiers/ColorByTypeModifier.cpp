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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ColorByTypeModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorByTypeModifier);
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, colorOnlySelected);
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, clearSelection);
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, colorOnlySelected, "Color only selected elements");
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, clearSelection, "Clear selection");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ColorByTypeModifier::ColorByTypeModifier(ObjectInitializationFlags flags) : GenericPropertyModifier(flags),
    _colorOnlySelected(false),
    _clearSelection(true)
{
    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ColorByTypeModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    if(sourceProperty().isNull() && subject()) {

        // When the modifier is first inserted, automatically select the most recently added
        // typed property (in GUI mode) or the canonical type property (in script mode).
        const PipelineFlowState& input = request.modificationNode()->evaluateInputSynchronous(request);
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                if(property->isTypedProperty()) {
                    if(ExecutionContext::isInteractive() || property->type() == Property::GenericTypeProperty) {
                        bestProperty = PropertyReference(subject().dataClass(), property);
                    }
                }
            }
            if(!bestProperty.isNull())
                setSourceProperty(bestProperty);
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ColorByTypeModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // Whenever the selected property class of this modifier is changed, update the source property reference accordingly.
    if(field == PROPERTY_FIELD(GenericPropertyModifier::subject) && !isBeingLoaded() && !isUndoingOrRedoing()) {
        setSourceProperty(sourceProperty().convertToContainerClass(subject().dataClass()));
    }
    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void ColorByTypeModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
#ifdef OVITO_BUILD_BASIC
    throw Exception(tr("%1: This program feature is only available in OVITO Pro. Please visit our website www.ovito.org for more information.").arg(objectTitle()));
#else
    if(!subject())
        throw Exception(tr("No input element type selected."));
    if(!sourceProperty())
        throw Exception(tr("No input property selected."));

    // Check if the source property is the right kind of property.
    if(sourceProperty().containerClass() != subject().dataClass())
        throw Exception(tr("Modifier was set to operate on '%1', but the selected input is a '%2' property.")
            .arg(subject().dataClass()->pythonName()).arg(sourceProperty().containerClass()->propertyClassDisplayName()));

    DataObjectPath objectPath = state.expectMutableObject(subject());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());
    container->verifyIntegrity();

    // Get the input property.
    const Property* typePropertyObject = sourceProperty().findInContainer(container);
    if(!typePropertyObject)
        throw Exception(tr("The selected input property '%1' is not present.").arg(sourceProperty().name()));
    if(typePropertyObject->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typePropertyObject->name()));
    if(typePropertyObject->dataType() != Property::Int32)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be a 32-bit integer property.").arg(typePropertyObject->name()));
    BufferReadAccess<int32_t> typeProperty = typePropertyObject;

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selectionProperty;
    if(colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        if(const Property* selPropertyObj = container->getProperty(Property::GenericSelectionProperty)) {
            selectionProperty = selPropertyObj;

            // Clear selection if requested.
            if(clearSelection())
                container->removeProperty(selPropertyObj);
        }
    }

    // Create the color output property.
    BufferWriteAccess<ColorG, access_mode::write> colorProperty(
        container->createProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, Property::GenericColorProperty, objectPath),
        selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);

    // Access selection array.
    BufferReadAccess<SelectionIntType> selection(selectionProperty.get());

    // Create color lookup table.
    const std::map<int,Color> colorMap = typePropertyObject->typeColorMap();

    // Fill color property.
    size_t count = colorProperty.size();
    for(size_t i = 0; i < count; i++) {
        if(selection && !selection[i])
            continue;

        auto c = colorMap.find(typeProperty[i]);
        if(c == colorMap.end())
            colorProperty[i] = ColorG(1,1,1);
        else
            colorProperty[i] = c->second.toDataType<GraphicsFloatType>();
    }
#endif
}

}   // End of namespace
