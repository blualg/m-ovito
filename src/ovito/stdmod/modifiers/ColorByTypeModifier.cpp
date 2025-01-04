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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/SyclFlatMap.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ColorByTypeModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorByTypeModifier);
#ifndef OVITO_BUILD_BASIC
    OVITO_CLASSINFO(ColorByTypeModifier, "DisplayName", "Color by type");
#else
    OVITO_CLASSINFO(ColorByTypeModifier, "DisplayName", "Color by type (Pro)");
#endif
OVITO_CLASSINFO(ColorByTypeModifier, "Description", "Color data elements according to a typed property.");
OVITO_CLASSINFO(ColorByTypeModifier, "ModifierCategory", "Coloring");
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, colorOnlySelected);
DEFINE_PROPERTY_FIELD(ColorByTypeModifier, clearSelection);
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, colorOnlySelected, "Color only selected elements");
SET_PROPERTY_FIELD_LABEL(ColorByTypeModifier, clearSelection, "Clear selection");

/******************************************************************************
* Constructor.
******************************************************************************/
void ColorByTypeModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

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

    if(!sourceProperty() && subject()) {

        // When the modifier is first inserted, automatically select the most recently added
        // typed property (in GUI mode) or the canonical type property (in script mode).
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                if(property->isTypedProperty()) {
                    if(this_task::isInteractive() || property->typeId() == Property::GenericTypeProperty) {
                        bestProperty = property;
                    }
                }
            }
            setSourceProperty(bestProperty);
        }
    }
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ColorByTypeModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
#ifdef OVITO_BUILD_BASIC
    throw Exception(tr("%1: This program feature is only available in OVITO Pro. Please visit our website www.ovito.org for more information.").arg(objectTitle()));
#else
    if(!subject())
        throw Exception(tr("No input element type selected."));
    if(!sourceProperty())
        throw Exception(tr("No input property selected."));

    DataObjectPath objectPath = state.expectMutableObject(subject());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());
    container->verifyIntegrity();

    // Get the input property.
    ConstPropertyPtr typeProperty = sourceProperty().findInContainer(container);
    if(!typeProperty)
        throw Exception(tr("The selected input property '%1' is not present.").arg(sourceProperty().name()));
    if(typeProperty->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typeProperty->name()));
    if(typeProperty->dataType() != Property::Int32)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be a 32-bit integer property.").arg(typeProperty->name()));

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selection;
    if(colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        if(const Property* selPropertyObj = container->getProperty(Property::GenericSelectionProperty)) {
            selection = selPropertyObj;

            // Clear selection if requested.
            if(clearSelection())
                container->removeProperty(selPropertyObj);
        }
    }

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            container,
            objectPath = std::move(objectPath),
            typeProperty = std::move(typeProperty),
            selection = std::move(selection)]() mutable
    {
        // Call implementation.
        colorByType(typeProperty, container, objectPath, selection);

        return std::move(state);
    });
#endif
}

/******************************************************************************
* Implementation of the color-by-type algorithm.
******************************************************************************/
void ColorByTypeModifier::colorByType(const Property* typeProperty, PropertyContainer* container, const ConstDataObjectPath& containerPath, const Property* selection)
{
#ifdef OVITO_USE_SYCL
    // Create the color output property.
    Property* colorProperty = container->createProperty(selection ? DataBuffer::Initialized : DataBuffer::Uninitialized, Property::GenericColorProperty, containerPath);

    if(colorProperty->size() != 0) {
        // Create type-color lookup table and convert it into a SYCL-compatible data structure.
        const SyclFlatMap colorMap = typeProperty->typeColorMap();
        if(!colorMap.empty()) {
            this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {

                // Access the input types.
                SyclBufferAccess<int32_t, access_mode::read> inputAcc(typeProperty, cgh);
                // Access selection flags array (optional).
                SyclBufferAccess<SelectionIntType, access_mode::read> selectionAcc(selection, cgh);
                // Access output color array.
                SyclBufferAccess<ColorG, access_mode::write> outputAcc(colorProperty, cgh, selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);
                // Access color lookup table.
                auto colorMapAcc = colorMap.get_access(cgh);

                if(selectionAcc) {
                    OVITO_SYCL_PARALLEL_FOR(cgh, ColorByTypeModifier_kernel_sel)(sycl::range(typeProperty->size()), [=](size_t i) {
                        if(selectionAcc[i]) {
                            outputAcc[i] = colorMapAcc.get(inputAcc[i], ColorG(1,1,1));
                        }
                    });
                }
                else {
                    OVITO_SYCL_PARALLEL_FOR(cgh, ColorByTypeModifier_kernel)(sycl::range(typeProperty->size()), [=](size_t i) {
                        outputAcc[i] = colorMapAcc.get(inputAcc[i], ColorG(1,1,1));
                    });
                }
            });
        }
        else {
            colorProperty->fillSelected<ColorG>(ColorG(1,1,1), selection);
        }
    }
#else
    // Access the type array.
    BufferReadAccess<int32_t> typeAcc = typeProperty;

    // Create the color output property.
    BufferWriteAccess<ColorG, access_mode::write> colorProperty(
        container->createProperty(selection ? DataBuffer::Initialized : DataBuffer::Uninitialized, Property::GenericColorProperty, containerPath),
        selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);

    // Access selection array.
    BufferReadAccess<SelectionIntType> selectionAcc(selection);

    // Create type-color lookup table.
    const std::map<int, ColorG> colorMap = typeProperty->typeColorMap();

    // Fill color property.
    size_t count = colorProperty.size();
    for(size_t i = 0; i < count; i++) {
        if(selectionAcc && !selectionAcc[i])
            continue;

        auto c = colorMap.find(typeAcc[i]);
        if(c == colorMap.end())
            colorProperty[i] = ColorG(1,1,1);
        else
            colorProperty[i] = c->second;
    }
#endif
}

}   // End of namespace
