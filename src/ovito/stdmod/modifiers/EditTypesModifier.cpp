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
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "EditTypesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(EditTypesModifier);
OVITO_CLASSINFO(EditTypesModifier, "DisplayName", "Edit types");
OVITO_CLASSINFO(EditTypesModifier, "Description", "Edit the list of particle or bond types.");
OVITO_CLASSINFO(EditTypesModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(EditTypesModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(EditTypesModifier, deletedTypeIDs);
SET_PROPERTY_FIELD_LABEL(EditTypesModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(EditTypesModifier, deletedTypeIDs, "Deleted type IDs");

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void EditTypesModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(!sourceProperty()) {
        // When the modifier is first inserted, automatically select a initial property with associated element types.
        // If present, select the "Particle Type" property.
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        PropertyDataObjectReference bestProperty;
        std::vector<ConstDataObjectPath> dataObjectPaths = input.getObjectsRecursive(Property::OOClass());
        for(const ConstDataObjectPath& path : dataObjectPaths) {
            const Property* property = path.lastAs<Property>();
            if(property->isTypedProperty()) {
                if(!bestProperty || property->name() == QStringLiteral("Particle Type")) {
                    bestProperty = path;
                }
            }
        }
        setSourceProperty(bestProperty);
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void EditTypesModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(EditTypesModifier::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of EditTypesModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> EditTypesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!sourceProperty())
        throw Exception(tr("No input property selected."));

    // Get the input property.
    Property* typePropertyObject = state.expectMutableLeafObject(sourceProperty());
    if(typePropertyObject->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typePropertyObject->name()));
    if(typePropertyObject->dataType() != Property::Int32)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be a 32-bit integer property.").arg(typePropertyObject->name()));

    return std::move(state);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant EditTypesModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    if(node && sourceProperty()) {
        return sourceProperty().dataTitleOrPath();
    }
    return {};
}

}   // End of namespace
