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
DEFINE_VECTOR_REFERENCE_FIELD(EditTypesModifier, editedTypes);
SET_PROPERTY_FIELD_LABEL(EditTypesModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(EditTypesModifier, deletedTypeIDs, "Deleted type IDs");
SET_PROPERTY_FIELD_LABEL(EditTypesModifier, editedTypes, "Edited types");

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
        // Reset the lists of edited and deleted types when the source property changes.
        if(!isUndoingOrRedoing()) {
            setEditedTypes({});
            setDeletedTypeIDs({});
        }
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

    if(editedTypes().empty() && deletedTypeIDs().empty())
        return std::move(state); // No changes to apply.

    // Get the input property.
    Property* typeProperty = state.expectMutableLeafObject(sourceProperty());
    if(typeProperty->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typeProperty->name()));
    if(typeProperty->dataType() != Property::Int32)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be a 32-bit integer property.").arg(typeProperty->name()));

    // Detach the element types so that we can modify the list.
    auto typeList = typeProperty->elementTypes();

    // Remove deleted types.
    typeList.erase(std::remove_if(typeList.begin(), typeList.end(), [&](const OORef<const ElementType>& type) {
        return deletedTypeIDs().contains(type->numericId());
    }), typeList.end());

    // Apply edited types.
    CloneHelper cloneHelper;
    for(const OORef<ElementType>& editedType : editedTypes()) {
        for(size_t i = 0; i < typeList.size(); ++i) {
            if(typeList[i]->numericId() == editedType->numericId() && &typeList[i]->getOOClass() == &editedType->getOOClass()) {
                typeList[i] = cloneHelper.cloneObject(editedType, false);
                break;
            }
        }
    }

    // Append new types that are not already in the list.
    for(const OORef<ElementType>& editedType : editedTypes()) {
        bool found = false;
        for(const OORef<const ElementType>& existingType : typeList) {
            if(existingType->numericId() == editedType->numericId() && &existingType->getOOClass() == &editedType->getOOClass()) {
                found = true;
                break;
            }
        }
        if(!found) {
            typeList.push_back(cloneHelper.cloneObject(editedType, false));
        }
    }

    // Update the property with the modified type list.
    typeProperty->setElementTypes(std::move(typeList));

    return std::move(state);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant EditTypesModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    if(node && sourceProperty()) {
        QString title = sourceProperty().dataTitleOrPath();
        // Shorten the full path to show only the property name.
        int lastSepPos = title.lastIndexOf(QStringLiteral(u" \u2192 "));  // Unicode arrow path separator
        if(lastSepPos >= 0)
            title = title.mid(lastSepPos + 3);
        return title;
    }
    return {};
}

/******************************************************************************
* Records that the given element type has been edited.
******************************************************************************/
void EditTypesModifier::addEditedType(ElementType* type)
{
    OVITO_ASSERT(type);

    if(editedTypes().contains(type))
        return;

    if(deletedTypeIDs().contains(type->numericId())) {
        QSet<int32_t> deletedIDs = deletedTypeIDs();
        deletedIDs.remove(type->numericId());
        setDeletedTypeIDs(deletedIDs);
    }

    _editedTypes.push_back(this, PROPERTY_FIELD(editedTypes), type);
}

/******************************************************************************
* Deletes the element type with the given numeric ID.
******************************************************************************/
void EditTypesModifier::deleteType(int typeId)
{
    QSet<int32_t> deletedIDs = deletedTypeIDs();
    deletedIDs.insert(typeId);
    setDeletedTypeIDs(deletedIDs);
}

/******************************************************************************
* Restores the original element type with the given numeric ID.
******************************************************************************/
void EditTypesModifier::restoreType(int typeId)
{
    if(deletedTypeIDs().contains(typeId)) {
        QSet<int32_t> deletedIDs = deletedTypeIDs();
        deletedIDs.remove(typeId);
        setDeletedTypeIDs(deletedIDs);
    }
    else {
        for(int i = 0; i < editedTypes().size(); ++i) {
            if(editedTypes()[i]->numericId() == typeId) {
                _editedTypes.remove(this, PROPERTY_FIELD(editedTypes), i);
                break;
            }
        }
    }
}

}   // End of namespace
