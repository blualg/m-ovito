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

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/util/ElementSelectionSet.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * Modifiers that allows the user to select individual elements, e.g. particles or bonds, by hand.
 */
class OVITO_STDMOD_EXPORT ManualSelectionModifier : public GenericPropertyModifier
{
    OVITO_CLASS(ManualSelectionModifier)

public:

    /// Constructor.
    explicit ManualSelectionModifier(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Adopts the selection state from the modifier's input.
    void resetSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Selects all elements.
    void selectAll(ModificationNode* modApp, const PipelineFlowState& state);

    /// Deselects all elements.
    void clearSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Inverts the selection state of all elements.
    void invertSelection(ModificationNode* modApp, const PipelineFlowState& state);

    /// Toggles the selection state of a single element.
    void toggleElementSelection(ModificationNode* modApp, const PipelineFlowState& state, size_t elementIndex);

    /// Replaces the selection.
    void setSelection(ModificationNode* modApp, const PipelineFlowState& state, ConstPropertyPtr selection, ElementSelectionSet::SelectionMode mode);

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Returns the selection set object stored in the ModificationNode, or, if it does not exist, creates one when requested.
    ElementSelectionSet* getSelectionSet(ModificationNode* modApp, bool createIfNotExist);
};

/**
 * \brief The type of ModificationNode create for a ManualSelectionModifier
 *        when it is inserted into in a data pipeline.
 */
class OVITO_STDMOD_EXPORT ManualSelectionModificationNode : public ModificationNode
{
    OVITO_CLASS(ManualSelectionModificationNode)

public:

    /// Constructor.
    explicit ManualSelectionModificationNode(ObjectInitializationFlags flags) : ModificationNode(flags) {}

private:

    /// The per-application data of the modifier.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ElementSelectionSet>, selectionSet, setSelectionSet, PROPERTY_FIELD_ALWAYS_CLONE);
};

}   // End of namespace
