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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

// Explicit class template instantiations to be exported by the core module:
template class DataObjectPathTemplate<DataObject*>; // a.k.a. DataObjectPath
template class DataObjectPathTemplate<const DataObject*>; // a.k.a. ConstDataObjectPath
template class DataObjectPathTemplate<ConstDataObjectRef>; // a.k.a. ConstDataObjectRefPath

IMPLEMENT_ABSTRACT_OVITO_CLASS(DataObject);
DEFINE_PROPERTY_FIELD(DataObject, identifier);
DEFINE_RUNTIME_PROPERTY_FIELD(DataObject, createdByNode);
DEFINE_VECTOR_REFERENCE_FIELD(DataObject, visElements);
DEFINE_REFERENCE_FIELD(DataObject, editableProxy);
SET_PROPERTY_FIELD_LABEL(DataObject, visElements, "Visual elements");
SET_PROPERTY_FIELD_LABEL(DataObject, editableProxy, "Editable proxy");
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(DataObject, createdByNode, "dataSource"); // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Generates a human-readable string representation of the data object reference.
******************************************************************************/
QString DataObject::OOMetaClass::formatDataObjectPath(const ConstDataObjectPath& path) const
{
    QString str = path.back()->getOOMetaClass().displayName();
    bool first = true;
    for(const DataObject* obj : path) {
        if(first) {
            first = false;
            str += QStringLiteral(": ");
        }
        else str += QStringLiteral(u" \u2192 ");  // Unicode arrow
        str += obj->objectTitle();
    }
    return str;
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool DataObject::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && visElements().contains(source) && !event.sender()->isBeingLoaded()) {
        // Inform dependents that this data object's visual element was modified.
        // This is a separate notification event, because regular change messages from the visual element are
        // not propagated by the data object.
        notifyDependents(DataObject::VisualElementModified);
    }
    else if(event.type() == DataObject::VisualElementModified) {
        // Parent data objects propagate "VisualElementModified" events coming from child data objects.
        return true;
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void DataObject::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    RefTarget::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x02);
    // This chunk is reserved for future use.
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void DataObject::loadFromStream(ObjectLoadStream& stream)
{
    RefTarget::loadFromStream(stream);
    stream.expectChunk(0x02);
    // This chunk is reserved for future use.
    stream.closeChunk();
}

/******************************************************************************
* Determines if it is safe to modify this data object without unwanted side effects.
* Returns true if there is only one exclusive owner of this data object (if any).
* Returns false if there are multiple references to this data object from several
* data collections or other container data objects.
******************************************************************************/
bool DataObject::isSafeToModify() const
{
    OVITO_CHECK_OBJECT_POINTER(this);

    if(_dataReferenceCount.load(std::memory_order_acquire) <= 1) {
        bool isExclusivelyOwned = true;
        visitDependents([&](const RefMaker* dependent) noexcept {
            // Recursively determine if the container of this data object is safe to modify as well.
            // Only if the entire hierarchy of objects is safe to modify, we can safely modify
            // the leaf object.
            if(const DataObject* owner = dynamic_object_cast<DataObject>(dependent)) {
                if(owner->editableProxy() != this) // Note: Proxy objects are always considered safe to modify.
                    if(!owner->isSafeToModify())
                        isExclusivelyOwned = false;
            }
        });
        return isExclusivelyOwned;
    }
    return false;
}

/******************************************************************************
* Determines whether it is safe to modify the given child object without unwanted side effects.
* This method just checks the use count of the child object. It assumes this parent object
* is already safe to modify.
******************************************************************************/
bool DataObject::isSafeToModifySubObject(const DataObject* subObject) const
{
    OVITO_CHECK_OBJECT_POINTER(subObject);
    OVITO_ASSERT(this->hasReferenceTo(subObject));
    OVITO_ASSERT_MSG(this->isSafeToModify(), "DataObject::isSafeToModifySubobject()", qPrintable(QString("Cannot make sub-object %1 mutable, because parent object %2 itself is not safe to modify.").arg(subObject->getOOClass().name()).arg(getOOClass().name())));

    return (subObject->_dataReferenceCount.load(std::memory_order_acquire) <= 1);
}

/******************************************************************************
* Duplicates the given sub-object from this container object if it is shared
* with others. After this method returns, the returned sub-object will be
* exclusively owned by this container can be safely modified without unwanted
* side effects.
******************************************************************************/
DataObject* DataObject::makeMutable(const DataObject* subObject)
{
    OVITO_CHECK_OBJECT_POINTER(this);

    if(subObject && !isSafeToModifySubObject(subObject)) {
        OORef<DataObject> clone = CloneHelper::cloneSingleObject(subObject, false);
        replaceReferencesTo(subObject, clone);
        OVITO_ASSERT(hasReferenceTo(clone));
        OVITO_ASSERT(!hasReferenceTo(subObject));
        subObject = clone;
    }
#ifdef OVITO_DEBUG
    if(subObject && !subObject->isSafeToModify()) {
        qDebug() << "ERROR: Data sub-object" << subObject << "owned by" << this << "is not mutable after a call to DataObject::makeMutable().";
        qDebug() << "Data reference count of sub-object is" << subObject->_dataReferenceCount.load();
        qDebug() << "Listing dependents of sub-object:";
        subObject->visitDependents([](RefMaker* dependent) {
            qDebug() << "  -" << dependent;
        });
        qDebug() << "Data reference count of parent object is" << _dataReferenceCount.load();
        qDebug() << "Listing dependents of parent object:";
        visitDependents([](RefMaker* dependent) {
            qDebug() << "  -" << dependent;
        });
        OVITO_ASSERT(false);
    }
#endif
    return const_cast<DataObject*>(subObject);
}

/******************************************************************************
* Duplicates the given sub-object from this container object if it is shared
* with others. After this method returns, the returned sub-object will be
* exclusively owned by this container can be safely modified without unwanted
* side effects.
******************************************************************************/
DataObject* DataObject::makeMutable(const DataObject* subObject, CloneHelper& cloneHelper)
{
    OVITO_CHECK_OBJECT_POINTER(this);

    if(DataObject* clone = cloneHelper.lookupCloneOf(subObject)) {
        OVITO_ASSERT(!hasReferenceTo(subObject));
        OVITO_ASSERT(hasReferenceTo(clone));
        OVITO_ASSERT(clone->isSafeToModify());
        return clone;
    }
    OVITO_ASSERT(!subObject || hasReferenceTo(subObject));

    if(subObject && !isSafeToModifySubObject(subObject)) {
        OORef<DataObject> clone = cloneHelper.cloneObject(subObject, false);
        replaceReferencesTo(subObject, clone);
        OVITO_ASSERT(hasReferenceTo(clone));
        OVITO_ASSERT(!hasReferenceTo(subObject));
        subObject = clone;
    }
#ifdef OVITO_DEBUG
    if(subObject && !subObject->isSafeToModify()) {
        qDebug() << "ERROR: Data sub-object" << subObject << "owned by" << this << "is not mutable after a call to DataObject::makeMutable().";
        qDebug() << "Data reference count of sub-object is" << subObject->_dataReferenceCount.load();
        qDebug() << "Listing dependents of sub-object:";
        subObject->visitDependents([](RefMaker* dependent) {
            qDebug() << "  -" << dependent;
        });
        qDebug() << "Data reference count of parent object is" << _dataReferenceCount.load();
        qDebug() << "Listing dependents of parent object:";
        visitDependents([](RefMaker* dependent) {
            qDebug() << "  -" << dependent;
        });
        OVITO_ASSERT(false);
    }
#endif
    return const_cast<DataObject*>(subObject);
}

/******************************************************************************
* Returns the absolute path of this DataObject within the DataCollection.
* Returns an empty path if the DataObject is not exclusively owned by one
* DataCollection.
******************************************************************************/
ConstDataObjectPath DataObject::exclusiveDataObjectPath() const
{
    ConstDataObjectPath path;
    const DataObject* obj = this;
    do {
        path.push_back(obj);
        const DataObject* parent = nullptr;
        obj->visitDependents([&](RefMaker* dependent) {
            if(const DataObject* dataParent = dynamic_object_cast<DataObject>(dependent)) {
                if(!parent)
                    parent = dataParent;
                else
                    path.clear();
            }
        });
        obj = parent;
    }
    while(obj && !path.empty());
    std::reverse(path.begin(), path.end());
    return path;
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void DataObject::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath, bool forceProxyReplacement) const
{
    // Note: 'this' may no longer exist at this point, because the sub-class implementation of the method may
    // have already replaced it with a mutable copy.

    const DataObject* self = dataPath.back();
    Q_DECL_UNUSED const OvitoClass& selfClass = self->getOOClass();
    OVITO_ASSERT(selfClass == this->getOOClass());
    OVITO_ASSERT(!self->isUndoRecording());
    OVITO_ASSERT(!self->editableProxy() || !static_object_cast<DataObject>(self->editableProxy())->editableProxy());

    // Visit all sub-objects recursively.
    for(const PropertyFieldDescriptor* field : self->getOOMetaClass().propertyFields()) {
        if(field->isReferenceField() && !field->isWeakReference() && field->targetClass()->isDerivedFrom(DataObject::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
            if(!field->isVector()) {
                if(const DataObject* subObject = static_object_cast<DataObject>(self->getReferenceFieldTarget(field))) {
                    OVITO_ASSERT(self->hasReferenceTo(subObject));
                    dataPath.push_back(subObject);
                    subObject->updateEditableProxies(state, dataPath, forceProxyReplacement);
                    dataPath.pop_back();
                    OVITO_ASSERT(selfClass == dataPath.back()->getOOClass());
                    self = dataPath.back();
                }
            }
            else {
                // Note: 'self' may get replaced or deleted at any time!
                int count = self->getVectorReferenceFieldSize(field);
                for(int i = 0; i < count; i++) {
                    if(const DataObject* subObject = static_object_cast<DataObject>(self->getVectorReferenceFieldTarget(field, i))) {
                        dataPath.push_back(subObject);
                        subObject->updateEditableProxies(state, dataPath, forceProxyReplacement);
                        dataPath.pop_back();
                        OVITO_ASSERT(selfClass == dataPath.back()->getOOClass());
                        self = dataPath.back();
                    }
                }
            }
        }
    }
}

#ifdef OVITO_DEBUG
void DataObject::trackReferenceIncrement() const
{
    qDebug() << "Incrementing data reference count of" << this << "to" << _dataReferenceCount.load() + 1;
}

void DataObject::trackReferenceDecrement() const
{
    qDebug() << "Decrementing data reference count of" << this << "to" << _dataReferenceCount.load() - 1;
}
#endif

}   // End of namespace
