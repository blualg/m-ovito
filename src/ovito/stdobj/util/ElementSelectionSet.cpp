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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include "ElementSelectionSet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ElementSelectionSet);
DEFINE_PROPERTY_FIELD(ElementSelectionSet, useIdentifiers);

/* Undo record that can restore an old selection state. */
class ReplaceSelectionOperation : public UndoableOperation
{
public:
    ReplaceSelectionOperation(ElementSelectionSet* owner) :
        _owner(owner), _selection(owner->_selection), _selectedIdentifiers(owner->_selectedIdentifiers) {}

    virtual void undo() override {
        _selection.swap(_owner->_selection);
        _selectedIdentifiers.swap(_owner->_selectedIdentifiers);
        _owner->notifyTargetChanged();
    }

    virtual QString displayName() const override {
        return QStringLiteral("Replace selection set");
    }

private:

    OORef<ElementSelectionSet> _owner;
    ConstPropertyPtr _selection;
    QSet<qlonglong> _selectedIdentifiers; // Note: using qlonglong instead of IdentifierIntType for compatibility with corresponding field of ElementSelectionSet class
};

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
* Saves the class' contents to the given stream.
******************************************************************************/
void ElementSelectionSet::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    RefTarget::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x03);
    stream.saveObject(_selection);
    stream << _selectedIdentifiers;
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void ElementSelectionSet::loadFromStream(ObjectLoadStream& stream)
{
    RefTarget::loadFromStream(stream);
    int version = stream.expectChunkRange(0x02, 1);
    if(version >= 1) {
        _selection = stream.loadObject<Property>();
    }
    else {
        // For backward compatibility with OVITO 3.10:
        boost::dynamic_bitset<> bs;
        stream >> bs;
        PropertyPtr selection = PropertyPtr::create(DataBuffer::Uninitialized, bs.size(), DataBuffer::IntSelection, 1, QStringLiteral("Selection"), Property::GenericSelectionProperty);
        size_t index = 0;
        for(auto& s : BufferWriteAccess<SelectionIntType, access_mode::discard_write>{selection})
            s = bs[index++];
        _selection = std::move(selection);
    }
    stream >> _selectedIdentifiers;
    stream.closeChunk();
}

/******************************************************************************
* Creates a copy of this object.
******************************************************************************/
OORef<RefTarget> ElementSelectionSet::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<ElementSelectionSet> clone = static_object_cast<ElementSelectionSet>(RefTarget::clone(deepCopy, cloneHelper));
    clone->_selection = this->_selection;
    clone->_selectedIdentifiers = this->_selectedIdentifiers;
    return clone;
}

/******************************************************************************
* Adopts the selection set from the given input property container.
******************************************************************************/
void ElementSelectionSet::resetSelection(const PropertyContainer* container)
{
    OVITO_ASSERT(container != nullptr);

    // Take a snapshot of the current selection state.
    if(const Property* selectionProperty = container->getProperty(Property::GenericSelectionProperty)) {

        // Make a backup of the old snapshot so it may be restored.
        pushIfUndoRecording<ReplaceSelectionOperation>(this);

        // Obtain access to the unique identifiers of the data elements (if present).
        BufferReadAccess<IdentifierIntType> identifierProperty;
        if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
            identifierProperty = container->getProperty(Property::GenericIdentifierProperty);
        OVITO_ASSERT(!identifierProperty || selectionProperty->size() == identifierProperty.size());

        if(identifierProperty && selectionProperty->size() == identifierProperty.size()) {
            _selectedIdentifiers.clear();
            _selection.reset();
            BufferReadAccess<SelectionIntType> selectionAcc{selectionProperty};
            auto s = selectionAcc.begin();
            for(auto id : identifierProperty) {
                if(*s++)
                    _selectedIdentifiers.insert(id);
            }
        }
        else {
            // Take a snapshot of the selection state.
            _selectedIdentifiers.clear();
            _selection = selectionProperty;
        }

        notifyTargetChanged();
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

    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ReplaceSelectionOperation>(this);

    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty) && container->getProperty(Property::GenericIdentifierProperty)) {
        _selection.reset();
        _selectedIdentifiers.clear();
    }
    else {
        PropertyPtr selection = container->getOOMetaClass().createStandardProperty(DataBuffer::Uninitialized, container->elementCount(), Property::GenericSelectionProperty);
        selection->fill<SelectionIntType>(0);
        _selection = std::move(selection);
        _selectedIdentifiers.clear();
    }
    notifyTargetChanged();
}

/******************************************************************************
* Replaces the selection set.
******************************************************************************/
void ElementSelectionSet::setSelection(const PropertyContainer* container, ConstPropertyPtr selection, SelectionMode mode)
{
    OVITO_ASSERT(container);
    OVITO_ASSERT(selection);
    OVITO_ASSERT(selection->size() == container->elementCount());

    // Make a backup of the old snapshot so it may be restored.
    pushIfUndoRecording<ReplaceSelectionOperation>(this);

    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);
    OVITO_ASSERT(!identifierProperty || selection->size() == identifierProperty.size());

    if(identifierProperty) {
        _selection.reset();
        BufferReadAccess<SelectionIntType> selectionAcc{selection};
        auto s = selectionAcc.begin();
        if(mode == SelectionReplace) {
            _selectedIdentifiers.clear();
            for(auto id : identifierProperty) {
                if(*s++)
                    _selectedIdentifiers.insert(id);
            }
        }
        else if(mode == SelectionAdd) {
            for(auto id : identifierProperty) {
                if(*s++)
                    _selectedIdentifiers.insert(id);
            }
        }
        else if(mode == SelectionSubtract) {
            for(auto id : identifierProperty) {
                if(*s++)
                    _selectedIdentifiers.remove(id);
            }
        }
    }
    else {
        _selectedIdentifiers.clear();
        if(mode == SelectionReplace) {
            _selection = std::move(selection);
        }
        else if(mode == SelectionAdd) {
            if(!_selection) {
                _selection = std::move(selection);
            }
            else {
                OVITO_ASSERT(_selection->size() == selection->size());
                if(_selection->size() == selection->size()) {
                    BufferWriteAccess<SelectionIntType, access_mode::read_write> outAcc{_selection.makeMutableInplace()};
                    auto sout = outAcc.begin();
                    for(auto sin : BufferReadAccess<SelectionIntType>{selection})
                        *sout++ |= sin;
                }
            }
        }
        else if(mode == SelectionSubtract) {
            OVITO_ASSERT(_selection && _selection->size() == selection->size());
            if(_selection && _selection->size() == selection->size()) {
                BufferWriteAccess<SelectionIntType, access_mode::read_write> outAcc{_selection.makeMutableInplace()};
                auto sout = outAcc.begin();
                for(auto sin : BufferReadAccess<SelectionIntType>{selection})
                    *sout++ &= ~sin;
            }
        }
    }

    notifyTargetChanged();
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
        _selection.reset();
        toggleElementById(identifierProperty[elementIndex]);
    }
    else {
        OVITO_ASSERT(_selection);
        if(_selection && elementIndex < _selection->size()) {
            _selectedIdentifiers.clear();
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
        if(_selectedIdentifiers.contains(elementId))
            _selectedIdentifiers.remove(elementId);
        else
            _selectedIdentifiers.insert(elementId);
    }
    notifyTargetChanged();
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ElementSelectionSet::toggleElementByIndex(size_t elementIndex)
{
    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ToggleSelectionOperation>(this, -1, elementIndex);

    if(_selection && elementIndex < _selection->size()) {
        BufferWriteAccess<SelectionIntType, access_mode::read_write> acc{_selection.makeMutableInplace()};
        acc[elementIndex] = !acc[elementIndex];
    }
    notifyTargetChanged();
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ElementSelectionSet::selectAll(const PropertyContainer* container)
{
    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ReplaceSelectionOperation>(this);

    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);

    if(identifierProperty) {
        _selection.reset();
        _selectedIdentifiers.clear();
        for(auto id : identifierProperty)
            _selectedIdentifiers.insert(id);
    }
    else {
        PropertyPtr selection = container->getOOMetaClass().createStandardProperty(DataBuffer::Uninitialized, container->elementCount(), Property::GenericSelectionProperty);
        selection->fill<SelectionIntType>(1);
        _selection = std::move(selection);
        _selectedIdentifiers.clear();
    }
    notifyTargetChanged();
}

/******************************************************************************
* Inverts the selection state of all elements.
******************************************************************************/
void ElementSelectionSet::invertSelection(const PropertyContainer* container)
{
    // Make a backup of the old selection state so it may be restored.
    pushIfUndoRecording<ReplaceSelectionOperation>(this);

    // Obtain access to the unique identifiers of the data elements (if present).
    BufferReadAccess<IdentifierIntType> identifierProperty;
    if(useIdentifiers() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        identifierProperty = container->getProperty(Property::GenericIdentifierProperty);

    if(identifierProperty) {
        _selection.reset();
        for(auto id : identifierProperty) {
            if(!_selectedIdentifiers.remove(id))
                _selectedIdentifiers.insert(id);
        }
    }
    else {
        OVITO_ASSERT(_selection && _selection->size() == container->elementCount());
        if(_selection) {
            for(auto& s : BufferWriteAccess<SelectionIntType, access_mode::read_write>{_selection.makeMutableInplace()})
                s = !s;
        }
        _selectedIdentifiers.clear();
    }
    notifyTargetChanged();
}

/******************************************************************************
* Copies the stored selection set into the given output selection property.
******************************************************************************/
PipelineStatus ElementSelectionSet::applySelection(PropertyContainer* container, BufferReadAccess<IdentifierIntType> identifierProperty)
{
    size_t nselected = 0;
    if(!identifierProperty || !useIdentifiers()) {

        // When not using identifiers, the number of input elements must match.
        if(!_selection || container->elementCount() != _selection->size())
            throw Exception(tr("Stored selection state became invalid, because the number of input elements has changed."));

        // Restore selection simply by placing the snapshot into the pipeline.
        nselected = _selection->nonzeroCount();
        container->createProperty(_selection);
    }
    else if(identifierProperty) {
        Property* selectionProperty = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);
        auto id = identifierProperty.begin();
        for(auto& s : BufferWriteAccess<SelectionIntType, access_mode::discard_write>{selectionProperty}) {
            if((s = _selectedIdentifiers.contains(*id++)))
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
