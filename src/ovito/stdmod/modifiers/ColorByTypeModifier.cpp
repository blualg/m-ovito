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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/app/Application.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ColorByTypeModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ColorByTypeModifier);
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
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("ParticlesObject"));
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
        const PipelineFlowState& input = request.modApp()->evaluateInputSynchronous(request);
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const PropertyObject* property : container->properties()) {
                if(property->isTypedProperty()) {
                    if(ExecutionContext::isInteractive() || property->type() == PropertyObject::GenericTypeProperty) {
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
    const PropertyObject* typePropertyObject = sourceProperty().findInContainer(container);
    if(!typePropertyObject)
        throw Exception(tr("The selected input property '%1' is not present.").arg(sourceProperty().name()));
    if(typePropertyObject->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typePropertyObject->name()));
    if(typePropertyObject->dataType() != PropertyObject::Int)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be an integer property.").arg(typePropertyObject->name()));
    ConstPropertyAccess<int> typeProperty = typePropertyObject;

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selectionProperty;
    if(colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
        if(const PropertyObject* selPropertyObj = container->getProperty(PropertyObject::GenericSelectionProperty)) {
            selectionProperty = selPropertyObj;

            // Clear selection if requested.
            if(clearSelection())
                container->removeProperty(selPropertyObj);
        }
    }

    // Create the color output property.
    PropertyAccess<Color> colorProperty = container->createProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, PropertyObject::GenericColorProperty, objectPath);

    // Access selection array.
    ConstPropertyAccessAndRef<int> selection(std::move(selectionProperty));

    // Create color lookup table.
    const std::map<int,Color> colorMap = typePropertyObject->typeColorMap();

    // Fill color property.
    size_t count = colorProperty.size();
    for(size_t i = 0; i < count; i++) {
        if(selection && !selection[i])
            continue;

        auto c = colorMap.find(typeProperty[i]);
        if(c == colorMap.end())
            colorProperty[i] = Color(1,1,1);
        else
            colorProperty[i] = c->second;
    }
#endif
}


#ifdef OVITO_QML_GUI
/******************************************************************************
* This helper method is called by the QML GUI (ColorByTypeModifier.qml) to extract
* the list of element types from the input pipeline output state.
******************************************************************************/
QVariantList ColorByTypeModifier::getElementTypesFromInputState(ModifierApplication* modApp) const
{
    QVariantList list;
    if(modApp && subject() && !sourceProperty().isNull() && sourceProperty().containerClass() == subject().dataClass()) {
        // Populate types list based on the selected input property.
        const PipelineFlowState& state = modApp->evaluateInputSynchronous(dataset()->animationSettings()->time());
        if(const PropertyContainer* container = state.getLeafObject(subject())) {
            if(const PropertyObject* inputProperty = sourceProperty().findInContainer(container)) {
                for(const ElementType* type : inputProperty->elementTypes()) {
                    if(!type) continue;
                    list.push_back(QVariantMap({
                        {"id", type->numericId()},
                        {"name", type->nameOrNumericId()},
                        {"color", (QColor)type->color()}}));
                }
            }
        }
    }
    return list;
}
#endif

}   // End of namespace
