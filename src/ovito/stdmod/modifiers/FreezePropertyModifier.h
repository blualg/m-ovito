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
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief Injects the values of a property taken from a different animation time.
 */
class OVITO_STDMOD_EXPORT FreezePropertyModifier : public GenericPropertyModifier
{
    OVITO_CLASS(FreezePropertyModifier)
    OVITO_CLASSINFO("DisplayName", "Freeze property");
    OVITO_CLASSINFO("Description", "Copy the values of a varying property from one trajectory frame to all others.");
    OVITO_CLASSINFO("ModifierCategory", "Modification");

public:

    /// Constructor.
    explicit FreezePropertyModifier(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState input) override;

    /// Modifies the input data synchronously.
    virtual void evaluateModifierSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return sourceProperty().name(); }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// The particle property that is preserved by this modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference, sourceProperty, setSourceProperty);

    /// The particle property to which the stored values should be written
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference, destinationProperty, setDestinationProperty);

    /// Animation frame at which the frozen property is taken.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, freezeTime, setFreezeTime);
};

/**
 * Used by the FreezePropertyModifier to store the values of the selected property.
 */
class OVITO_STDMOD_EXPORT FreezePropertyModificationNode : public ModificationNode
{
    OVITO_CLASS(FreezePropertyModificationNode)
    OVITO_CLASSINFO("ClassNameAlias", "FreezePropertyModifierApplication");  // For backward compatibility with OVITO 3.9.2

public:

    /// Constructor.
    explicit FreezePropertyModificationNode(ObjectInitializationFlags flags) : ModificationNode(flags) {}

    /// Makes a copy of the given source property and, optionally, of the provided
    /// element identifier list, which will allow to restore the saved property
    /// values even if the order of particles changes.
    void updateStoredData(const Property* property, const Property* identifiers, TimeInterval validityInterval);

    /// Returns true if the frozen state for given animation time is already stored.
    bool hasFrozenState(AnimationTime time) const { return _validityInterval.contains(time); }

    /// Clears the stored state.
    void invalidateFrozenState() {
        setProperty(nullptr);
        setIdentifiers(nullptr);
        _validityInterval.setEmpty();
        _idmap.clear();
    }

    /// Provides access to the cached ID-to-index map.
    std::unordered_map<IdentifierIntType, size_t>& idmap() { return _idmap; }

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The stored copy of the property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const Property>, property, setProperty, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// A copy of the element identifiers, taken at the time when the property values were saved.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const Property>, identifiers, setIdentifiers, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// The cached visalization elements that are attached to the output property.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<DataVis>, cachedVisElements, setCachedVisElements, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// The validity interval of the frozen property.
    TimeInterval _validityInterval;

    /// Cached mapping of unique IDs to indices.
    std::unordered_map<IdentifierIntType, size_t> _idmap;
};

}   // End of namespace
