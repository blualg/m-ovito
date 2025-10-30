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

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief Lets the user edit the list of element types associated with a property.
 */
class OVITO_STDMOD_EXPORT EditTypesModifier : public Modifier
{
    OVITO_CLASS(EditTypesModifier)

public:

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

    /// Records that the given element type has been edited.
    void addEditedType(ElementType* type);

    /// Deletes the element type with the given numeric ID.
    void deleteType(int typeId);

    /// Restores the original element type with the given numeric ID.
    void restoreType(int typeId);

    /// Inserts an element type into the list of edited types.
    void insertEditedType(qsizetype index, OORef<ElementType> type) {
        OVITO_ASSERT(editedTypes().contains(type) == false);
        _editedTypes.insert(this, PROPERTY_FIELD(editedTypes), index, std::move(type));
    }

    /// Removes an element type from the list of edited types.
    void removeEditedType(qsizetype index) {
        _editedTypes.remove(this, PROPERTY_FIELD(editedTypes), index);
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Selects the typed property to be edited.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyDataObjectReference{}, sourceProperty, setSourceProperty);

    /// The element types that have been edited.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ElementType>, editedTypes, setEditedTypes);

    /// The numeric IDs of the element types to delete.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QSet<int32_t>{}, deletedTypeIDs, setDeletedTypeIDs);
};

}   // End of namespace
