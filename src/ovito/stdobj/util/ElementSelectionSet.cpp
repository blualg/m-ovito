////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include "ElementSelectionSet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ElementSelectionSet);
DEFINE_REFERENCE_FIELD(ElementSelectionSet, selection);
DEFINE_PROPERTY_FIELD(ElementSelectionSet, selectedIdentifiers);
DEFINE_PROPERTY_FIELD(ElementSelectionSet, useIdentifiers);

/* Undo record that can restore selection state of a single element. */
class ToggleSelectionOperation : public UndoableOperation
{
public:
    ToggleSelectionOperation(ElementSelectionSet* owner, IdentifierIntType id, size_t elementIndex = std::numeric_limits<size_t>::max()) :
        _owner(owner), _index(elementIndex), _id(id) {}

    virtual void undo() override {
        if(_index != std::numeric_limits<size_t>::max())
            _owner->toggleElementByIndex(_index);
        else
            _owner->toggleElementById(_id);
    }

    virtual QString displayName() const override {
        return QStringLiteral("Toggle element selection");
    }

private:

    OORef<ElementSelectionSet> _owner;
    IdentifierIntType _id;
    size_t _index;
};

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void ElementSelectionSet::loadFromStream(ObjectLoadStream& stream)
{
    RefTarget::loadFromStream(stream);

    if(stream.formatVersion() < 30016) {
        int version = stream.expectChunkRange(0x02, 1);
        if(version >= 1) {
            // For backward compatibility with OVITO 3.14:
            _selection.set(this, PROPERTY_FIELD(selection), stream.loadObject<Property>());
        }
        else {
            // For backward compatibility with OVITO 3.10:
            boost::dynamic_bitset<> bs;
            stream >> bs;
            PropertyPtr selection = PropertyPtr::create(DataBuffer::Uninitialized, bs.size(), DataBuffer::IntSelection, 1, QStringLiteral("Selection"), Property::GenericSelectionProperty);
            size_t index = 0;
            for(auto& s : BufferWriteAccess<SelectionIntType, access_mode::discard_write>{selection})
                s = bs[index++];
            _selection.set(this, PROPERTY_FIELD(selection), std::move(selection));
        }
        stream >> _selectedIdentifiers.mutableValue();
        stream.closeChunk();
    }
}

/******************************************************************************
* Adopts the selection set from the given input property container.
******************************************************************************/
void ElementSelectionSet::resetSelection(const PropertyContainer* container)
{
    OVITO_ASSERT(container != nullptr);

    // Take a snapshot of the current selection state.
    if(const Property* selectionProperty = container->getProperty(Property::GenericSelectionProperty)) {

        // Obtain access to the unique identifiers of the data elements (if present).
        BufferReadAccess<IdentifierIntType> identifierProperty;
        if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
            identifierProperty = container->getProperty(Property::GenericIdentifierProperty);
        OVITO_ASSERT(!identifierProperty || selectionProperty->size() == identifierProperty.size());

        if(identifierProperty && selectionProperty->size() == identifierProperty.size()) {
            QSet<qlonglong> selectedIds;
            BufferReadAccess<SelectionIntType> selectionAcc{selectionProperty};
            auto s = selectionAcc.begin();
            for(auto id : identifierProperty) {
                if(*s++)
                    selectedIds.insert(id);
            }
            _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), std::move(selectedIds));
            _selection.set(this, PROPERTY_FIELD(selection), nullptr);
        }
        else {
            // Take a snapshot of the selection state.
            _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
            _selection.set(this, PROPERTY_FIELD(selection), selectionProperty);
        }
    }
    else {
        // Reset selection snapshot if input doesn't contain a selection state.
        clearSelection(container);
    }
}

/******************************************************************************
* Clears the selection set.
******************************************************************************/
void ElementSelectionSet::clearSelection(const PropertyContainer* container)
{
    OVITO_ASSERT(container != nullptr);

    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty) && container->getProperty(Property::GenericIdentifierProperty)) {
        _selection.set(this, PROPERTY_FIELD(selection), nullptr);
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
    }
    else {
        PropertyPtr selection = container->getOOMetaClass().createStandardProperty(DataBuffer::Uninitialized, container->elementCount(), Property::GenericSelectionProperty);
        selection->fill<SelectionIntType>(0);
        _selection.set(this, PROPERTY_FIELD(selection), std::move(selection));
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
    }
}

/******************************************************************************
* Replaces the selection set.
******************************************************************************/
void ElementSelectionSet::setSelection(const PropertyContainer* container, ConstPropertyPtr selection, SelectionMode mode)
{
    OVITO_ASSERT(container);
    OVITO_ASSERT(selection);
    OVITO_ASSERT(selection->size() == container->elementCount());

    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);
    OVITO_ASSERT(!identifierProperty || selection->size() == identifierProperty.size());

    if(identifierProperty) {
        QSet<qlonglong> selectedIds;
        BufferReadAccess<SelectionIntType> selectionAcc{selection};
        auto s = selectionAcc.begin();
        if(mode == SelectionReplace) {
            for(auto id : identifierProperty) {
                if(*s++)
                    selectedIds.insert(id);
            }
        }
        else if(mode == SelectionAdd) {
            selectedIds = _selectedIdentifiers;
            for(auto id : identifierProperty) {
                if(*s++)
                    selectedIds.insert(id);
            }
        }
        else if(mode == SelectionSubtract) {
            selectedIds = _selectedIdentifiers;
            for(auto id : identifierProperty) {
                if(*s++)
                    selectedIds.remove(id);
            }
        }
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), std::move(selectedIds));
        _selection.set(this, PROPERTY_FIELD(selection), nullptr);
    }
    else {
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
        if(mode == SelectionReplace) {
            _selection.set(this, PROPERTY_FIELD(selection), std::move(selection));
        }
        else if(mode == SelectionAdd) {
            if(!this->selection()) {
                _selection.set(this, PROPERTY_FIELD(selection), std::move(selection));
            }
            else {
                OVITO_ASSERT(this->selection()->size() == selection->size());
                if(this->selection()->size() == selection->size()) {
                    BufferWriteAccessAndRef<SelectionIntType, access_mode::read_write> outAcc{ConstPropertyPtr::makeCopy(this->selection())};
                    auto sout = outAcc.begin();
                    for(auto sin : BufferReadAccess<SelectionIntType>{selection})
                        *sout++ |= sin;
                    _selection.set(this, PROPERTY_FIELD(selection), static_object_cast<Property>(outAcc.take()));
                }
            }
        }
        else if(mode == SelectionSubtract) {
            OVITO_ASSERT(this->selection() && this->selection()->size() == selection->size());
            if(this->selection() && this->selection()->size() == selection->size()) {
                BufferWriteAccessAndRef<SelectionIntType, access_mode::read_write> outAcc{ConstPropertyPtr::makeCopy(this->selection())};
                auto sout = outAcc.begin();
                for(auto sin : BufferReadAccess<SelectionIntType>{selection})
                    *sout++ &= ~sin;
                _selection.set(this, PROPERTY_FIELD(selection), static_object_cast<Property>(outAcc.take()));
            }
        }
    }
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ElementSelectionSet::toggleElement(const PropertyContainer* container, size_t elementIndex)
{
    if(elementIndex >= container->elementCount())
        return;

    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);

    if(identifierProperty) {
        _selection.set(this, PROPERTY_FIELD(selection), nullptr);
        toggleElementById(identifierProperty[elementIndex]);
    }
    else {
        OVITO_ASSERT(selection());
        if(selection() && elementIndex < selection()->size()) {
            _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
            toggleElementByIndex(elementIndex);
        }
    }
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ElementSelectionSet::toggleElementById(IdentifierIntType elementId)
{
    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ToggleSelectionOperation>(this, elementId);

    if(useIdentifiers()) {
        UndoSuspender suspendUndoRecording;
        QSet<qlonglong> selectedIds = selectedIdentifiers();
        if(selectedIds.contains(elementId))
            selectedIds.remove(elementId);
        else
            selectedIds.insert(elementId);
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), std::move(selectedIds));
    }
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ElementSelectionSet::toggleElementByIndex(size_t elementIndex)
{
    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ToggleSelectionOperation>(this, -1, elementIndex);

    if(selection() && elementIndex < selection()->size()) {
        UndoSuspender suspendUndoRecording;
        BufferWriteAccessAndRef<SelectionIntType, access_mode::read_write> acc{ConstPropertyPtr::makeCopy(this->selection())};
        acc[elementIndex] = !acc[elementIndex];
        _selection.set(this, PROPERTY_FIELD(selection), static_object_cast<Property>(acc.take()));
    }
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ElementSelectionSet::selectAll(const PropertyContainer* container)
{
    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);

    if(identifierProperty) {
        QSet<qlonglong> selectedIds;
        for(auto id : identifierProperty)
            selectedIds.insert(id);
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), std::move(selectedIds));
        _selection.set(this, PROPERTY_FIELD(selection), nullptr);
    }
    else {
        PropertyPtr selection = container->getOOMetaClass().createStandardProperty(DataBuffer::Uninitialized, container->elementCount(), Property::GenericSelectionProperty);
        selection->fill<SelectionIntType>(1);
        _selection.set(this, PROPERTY_FIELD(selection), std::move(selection));
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
    }
}

/******************************************************************************
* Inverts the selection state of all elements.
******************************************************************************/
void ElementSelectionSet::invertSelection(const PropertyContainer* container)
{
    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);

    if(identifierProperty) {
        QSet<qlonglong> newSelectedIds = selectedIdentifiers();
        for(auto id : identifierProperty) {
            if(!newSelectedIds.remove(id))
                newSelectedIds.insert(id);
        }
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), std::move(newSelectedIds));
        _selection.set(this, PROPERTY_FIELD(selection), nullptr);
    }
    else {
        OVITO_ASSERT(selection() && selection()->size() == container->elementCount());
        if(selection()) {
            BufferWriteAccessAndRef<SelectionIntType, access_mode::read_write> acc{ConstPropertyPtr::makeCopy(this->selection())};
            for(auto& s : acc)
                s = !s;
            _selection.set(this, PROPERTY_FIELD(selection), static_object_cast<Property>(acc.take()));
        }
        _selectedIdentifiers.set(this, PROPERTY_FIELD(selectedIdentifiers), QSet<qlonglong>{});
    }
}

/******************************************************************************
* Copies the stored selection set into the given output selection property.
******************************************************************************/
PipelineStatus ElementSelectionSet::applySelection(PropertyContainer* container, BufferReadAccess<IdentifierIntType> identifierProperty)
{
    size_t nselected = 0;
    if(!identifierProperty || !useIdentifiers()) {

        // When not using identifiers, the number of input elements must match.
        if(!selection() || container->elementCount() != selection()->size())
            throw Exception(tr("Stored selection state became invalid, because the number of input elements has changed."));

        // Restore selection simply by placing the snapshot into the pipeline.
        nselected = selection()->nonzeroCount();
        container->createProperty(selection());
    }
    else if(identifierProperty) {
        Property* selectionProperty = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);
        auto id = identifierProperty.begin();
        for(auto& s : BufferWriteAccess<SelectionIntType, access_mode::discard_write>{selectionProperty}) {
            if((s = selectedIdentifiers().contains(*id++)))
                nselected++;
        }
        selectionProperty->setNonzeroCount(nselected);
    }
    else {
        container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty)->fillZero();
    }

    return PipelineStatus(PipelineStatus::Success, tr("%1 elements selected").arg(nselected));
}

}   // End of namespace
