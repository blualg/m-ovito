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

#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/NoninteractiveContext.h>
#include "ObjectSaveStream.h"

namespace Ovito {

/******************************************************************************
* The destructor closes the stream.
******************************************************************************/
ObjectSaveStream::~ObjectSaveStream()
{
    try {
        ObjectSaveStream::close();
    }
    catch(const Exception& ex) {
        if(this_task::get())
            this_task::ui()->reportError(ex);
        else
            ex.logError();
    }
}

/******************************************************************************
* Registers an object reference to be written to the stream.
******************************************************************************/
quint32 ObjectSaveStream::registerObjectReference(const RefTarget* source, const RefTarget* target, bool excludeRecomputableData, const RefTarget* deltaReferenceObject)
{
    // Delta reference object should only be provided when saving only modified data.
    OVITO_ASSERT(_saveOnlyModifiedData || deltaReferenceObject == nullptr);

    // We cannot register further objects for serialization
    // while we are already in the process of serializing objects.
    OVITO_ASSERT(!_currentObjectRecord);
    if(_currentObjectRecord)
        throw Exception(tr("Cannot register another object reference for serialization while objects are already being serialized."));

    if(target == nullptr) {
        // Object ID zero is reserved for null pointers.
        return 0;
    }

    // Instead of saving the object's data immediately, we only assign a unique instance ID to the object here
    // and write that ID to the stream. The object itself will get saved later when the stream is being closed.
    OVITO_CHECK_OBJECT_POINTER(target);
    OVITO_ASSERT(_objects.size() == _objectMap.size());

    // Don't use delta reference object if types differ.
    if(deltaReferenceObject && &target->getOOClass() != &deltaReferenceObject->getOOClass()) {
        deltaReferenceObject = nullptr;
    }

    if(source != nullptr) {
        // Look up the source object record of the reference.
        auto iter = _objectMap.find(source); // Ensure source is registered.
        if(iter == _objectMap.end()) {
            OVITO_ASSERT(false); // Source object has not been registered before.
            throw Exception(tr("Source object of reference has not been registered for serialization."));
        }
        // Register the reference from source to target.
        ObjectRecord& sourceRecord = _objects[iter->second - 1];
        sourceRecord.references.push_back(target);

        // Inherit excludeRecomputableData flag from source object.
        if(sourceRecord.excludeRecomputableData)
            excludeRecomputableData = true;
    }
    else {
        OVITO_ASSERT(deltaReferenceObject == nullptr);
    }

    quint32& id = _objectMap[target];
    if(id == 0) {
        // To serialize only changed parameters, create a default-constructed object instance as reference to compare against.
        OORef<const RefTarget> defaultConstructedObject = deltaReferenceObject;
        if(_saveOnlyModifiedData && !defaultConstructedObject) {
            // Temporarily establish a non-interactive context to always initialize
            // object parameters to factory default settings.
            NoninteractiveContext noninteractiveContext;
            defaultConstructedObject = static_object_cast<RefTarget>(target->getOOClass().createInstance());
        }

        // Add object to serialization set and give it a unique serialization ID.
        _objects.push_back({target, defaultConstructedObject, excludeRecomputableData, false, deltaReferenceObject != nullptr, source != nullptr && deltaReferenceObject != nullptr});
        id = static_cast<quint32>(_objects.size());

        // Also register the references this object holds to other sub-objects.
        target->registerObjectReferencesForSerialization(*this, defaultConstructedObject);
    }
    else {
        // Object has already been registered for serialization before.
        // Verify that the previous registration is consistent with the current one.
        ObjectRecord& record = _objects[id - 1];
        OVITO_ASSERT(record.object == target);

        // The excludeRecomputableData option must be specified by all registrations.
        // Otherwise, we reset the flag for the object and all its sub-objects.
        if(!excludeRecomputableData && record.excludeRecomputableData) {
            clearExcludeRecomputableDataFlag(record);
        }

        // Turn off weak reference flag if a strong reference is registered later.
        if(record.isWeaklyReferenced) {
            OVITO_ASSERT(!record.deltaReferenceObject);
            OVITO_ASSERT(!record.isReusedSubobject);
            record.isWeaklyReferenced = false;

            // Create a default constructed object instance as reference to compare against.
            if(_saveOnlyModifiedData) {
                // Temporarily establish a non-interactive context to always initialize
                // object parameters to factory default settings.
                NoninteractiveContext noninteractiveContext;
                record.deltaReferenceObject = static_object_cast<RefTarget>(target->getOOClass().createInstance());
            }

            // Also register the references this object holds to other sub-objects.
            target->registerObjectReferencesForSerialization(*this, record.deltaReferenceObject);
        }
        else {
            // Don't skip object if it has an ambiguous reference state or no parent.
            if(deltaReferenceObject != record.deltaReferenceObject || source == nullptr) {
                record.maybeSkipped = false;
                assignDefaultConstructedReferenceObject(record);
            }
        }
    }

    return id;
}

/******************************************************************************
* Registers a weak object reference for serialization.
******************************************************************************/
quint32 ObjectSaveStream::registerWeakObjectReference(const RefTarget* target)
{
    // We cannot register further objects for serialization
    // while we are already in the process of serializing objects.
    OVITO_ASSERT(!_currentObjectRecord);
    if(_currentObjectRecord)
        throw Exception(tr("Cannot register another object reference for serialization while objects are already being serialized."));

    if(target == nullptr) {
        // Object ID zero is reserved for null pointers.
        return 0;
    }

    quint32& id = _objectMap[target];
    if(id == 0) {
        // Add object to serialization set and give it a unique serialization ID.
        _objects.push_back({target, nullptr, false, true, false, false});
        id = static_cast<quint32>(_objects.size());
    }
    else {
        // Object has already been registered for serialization before.
        // Verify that the previous registration is consistent with the current one.
        ObjectRecord& record = _objects[id - 1];
        OVITO_ASSERT(record.object == target);

        // Cannot completely skip object if it has a weak reference to it.
        record.maybeSkipped = false;
    }

    return id;
}

/******************************************************************************
* Returns the unique serialization ID for a previously registered object instance.
******************************************************************************/
quint32 ObjectSaveStream::lookupObjectInstance(const RefTarget* object) const
{
    if(object == nullptr) {
        // Object ID zero is reserved for null pointers.
        return 0;
    }

    OVITO_CHECK_OBJECT_POINTER(object);
    OVITO_ASSERT(_objects.size() == _objectMap.size());

    auto iter = _objectMap.find(object);
    if(iter == _objectMap.end()) {
        OVITO_ASSERT(false); // Object has not been registered before.
        throw Exception(tr("Object has not been registered for serialization."));
    }
    return iter->second;
}

/******************************************************************************
* Serializes an object to the output stream.
******************************************************************************/
void ObjectSaveStream::saveObject(const RefTarget* object)
{
    // Register object and write its unique serialization ID to the stream.
    *this << registerObjectReference(nullptr, object, false, nullptr);
}

/******************************************************************************
* Serializes a weak reference to an object and writes it to the output stream.
******************************************************************************/
void ObjectSaveStream::saveWeakObjectReference(const RefTarget* object)
{
    // The weak object reference must have been registered before using registerWeakObjectReference().
    quint32 id = lookupObjectInstance(object);

    if(object) {
        // Ensure that the object has been registered before.
        OVITO_ASSERT(id != 0 && _objects[id - 1].object == object);
        // If the object was only registered as a weak reference before, write a null reference instead.
        if(_objects[id - 1].isWeaklyReferenced || _objects[id - 1].maybeSkipped)
            id = 0;
    }

    // Write the object's unique serialization ID to the stream.
    *this << id;
}

/******************************************************************************
* Clears the excludeRecomputableData flag in the given object record and all its sub-objects.
******************************************************************************/
void ObjectSaveStream::clearExcludeRecomputableDataFlag(ObjectRecord& record)
{
    record.excludeRecomputableData = false;

    for(const RefTarget* target : record.references) {
        // Sub-object must have been registered already.
        OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
        clearExcludeRecomputableDataFlag(_objects[_objectMap.find(target)->second - 1]);
    }
}

/******************************************************************************
* If an object has been using a custom delta reference object, replace it and all its sub-objects
* by default-constructed instances.
******************************************************************************/
void ObjectSaveStream::assignDefaultConstructedReferenceObject(ObjectRecord& record)
{
    if(record.isReusedSubobject) {
        record.isReusedSubobject = false;

        // Create a default constructed object instance as reference to compare against.
        NoninteractiveContext noninteractiveContext;
        record.deltaReferenceObject = static_object_cast<RefTarget>(record.object->getOOClass().createInstance());

        // Also assign default constructed reference objects to all sub-objects.
        for(const RefTarget* target : record.references) {
            // Sub-object must have been registered already.
            OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
            assignDefaultConstructedReferenceObject(_objects[_objectMap.find(target)->second - 1]);
        }
    }
}

/******************************************************************************
* Registers an object class to be written to the stream.
******************************************************************************/
quint32 ObjectSaveStream::registerObjectClass(OvitoClassPtr clazz)
{
    if(clazz == nullptr) {
        // Class ID zero is reserved for null pointers.
        return 0;
    }
    else {
        OVITO_ASSERT(clazz->isDerivedFrom(RefTarget::OOClass()));
        OVITO_ASSERT(_classes.size() == _classMap.size());
        quint32& id = _classMap[clazz];
        if(id == 0) {
            // Add class to serialization set and give it a unique serialization ID.
            _classes.push_back(clazz);
            id = static_cast<quint32>(_classes.size());
        }
        else {
            OVITO_ASSERT(_classes[id - 1] == clazz);
        }
        return id;
    }
}

/******************************************************************************
* Determines whether the subtree rooted at the given object can be completely
* skipped from serialization.
******************************************************************************/
bool ObjectSaveStream::checkSkippableSubTree(ObjectRecord& objectRecord)
{
    // Objects that are only weakly referenced can be skipped.
    if(objectRecord.isWeaklyReferenced) {
        objectRecord.maybeSkipped = true;
        return true;
    }

    // If object has already been determined to be non-skippable, return immediately.
    if(!objectRecord.maybeSkipped || !_saveOnlyModifiedData)
        return objectRecord.maybeSkipped;

    // If object has no delta reference object, it can never be skippable.
    OVITO_ASSERT(objectRecord.deltaReferenceObject);
    // If object is not a resuable sub-object, it can never be skippable.
    OVITO_ASSERT(objectRecord.isReusedSubobject);

    for(const PropertyFieldDescriptor* field : objectRecord.object->getOOMetaClass().propertyFields()) {
        if(!objectRecord.maybeSkipped)
            break; // No need to check further fields.
        if(field->dontSerialize())
            continue; // Skip non-serializable fields.

        if(field->isReferenceField()) {
            if(!field->isVector()) {
                // Get the current referenced target and obtain the corresponding sub-object from the delta reference object.
                const RefTarget* target = objectRecord.object->getReferenceFieldTarget(field);
                const RefTarget* refTarget = objectRecord.deltaReferenceObject->getReferenceFieldTarget(field);
                objectRecord.maybeSkipped &= ((bool)target == (bool)refTarget);
                if(target && objectRecord.maybeSkipped) {
                    auto iter = _objectMap.find(target);
                    OVITO_ASSERT(iter != _objectMap.end());
                    objectRecord.maybeSkipped &= checkSkippableSubTree(_objects[iter->second - 1]);
                }
            }
            else {
                const auto count = objectRecord.object->getVectorReferenceFieldSize(field);
                const auto refCount = objectRecord.deltaReferenceObject->getVectorReferenceFieldSize(field);
                objectRecord.maybeSkipped &= (count == refCount);
                for(int i = 0; i < count; i++) {
                    // Get the current referenced target.
                    const RefTarget* target = objectRecord.object->getVectorReferenceFieldTarget(field, i);
                    // Obtain the corresponding sub-object from the delta reference object.
                    const RefTarget* refTarget = nullptr;
                    if(i < refCount) {
                        refTarget = objectRecord.deltaReferenceObject->getVectorReferenceFieldTarget(field, i);
                        objectRecord.maybeSkipped &= ((bool)target == (bool)refTarget);
                        if(target && objectRecord.maybeSkipped) {
                            auto iter = _objectMap.find(target);
                            OVITO_ASSERT(iter != _objectMap.end());
                            objectRecord.maybeSkipped &= checkSkippableSubTree(_objects[iter->second - 1]);
                        }
                    }
                }
            }
        }
        else {
            // Check if the property value differs from the parameter's default value.
            objectRecord.maybeSkipped &= objectRecord.object->comparePropertyFieldValue(field, *objectRecord.deltaReferenceObject);
        }
    }

    return objectRecord.maybeSkipped;
}

/******************************************************************************
* Closes the stream.
******************************************************************************/
void ObjectSaveStream::close()
{
    if(!isOpen())
        return;

    // Prevent re-entrance in case of an exception.
    if(!_currentObjectRecord) {
        try {
            // Determine which objects can be skipped.
            for(ObjectRecord& record : _objects) {
                checkSkippableSubTree(record);
            }

            // Serialize each object.
            // Note: Not using range-based for-loop here, because further objects may be appended to the list as we save objects.
            qint64 startObjectSection = filePosition();
            beginChunk(0x100);
            for(size_t i = 0; i < _objects.size(); i++) { // NOLINT(modernize-loop-convert)
                ObjectRecord& record = _objects[i];
                if(record.maybeSkipped)
                    continue; // Omit objects from file that fully match their reference state or are only weakly referenced.

                OVITO_CHECK_OBJECT_POINTER(record.object);
                record.byteOffset = filePosition();
                _currentObjectRecord = &record;

                // Register the object's class for serialization.
                record.classId = registerObjectClass(&record.object->getOOClass());

                // Let the object serialize its internal state.
                record.object->saveToStream(*this, record.excludeRecomputableData);
            }
            endChunk();
            //qDebug() << "Objects section:" << (filePosition() - startObjectSection) << "bytes";

            // Save the list of parameter fields.
            qint64 fieldTableStart = filePosition();
            beginChunk(0x101);
            for(const PropertyFieldDescriptor* field : _fields) {
                serializeParameterField(field);
            }
            endChunk();
            //qDebug() << "Field table section:" << (filePosition() - fieldTableStart) << "bytes (" << _fields.size() << "fields)";

            // Save the list of classes.
            // Note: Not using range-based for-loop here, because further classes may be appended to the list as we save classes.
            qint64 classTableStart = filePosition();
            beginChunk(0x200);
            for(size_t i = 0; i < _classes.size(); i++) { // NOLINT(modernize-loop-convert)
                serializeObjectClass(_classes[i]);
            }
            endChunk();
            //qDebug() << "Class table section:" << (filePosition() - classTableStart) << "bytes (" << _classes.size() << "classes)";

            // Save object index table.
            qint64 objectIndexStart = filePosition();
            beginChunk(0x300);
            quint32 numObjectsWritten = 0;
            for(const auto& [id, objectRecord] : Ovito::enumerate(_objects)) {
                if(objectRecord.maybeSkipped)
                    continue; // Omit objects from file that fully match their reference state or are only weakly referenced.
                *this << (quint32)(id + 1);
                *this << objectRecord.classId;
                *this << objectRecord.byteOffset;
                numObjectsWritten++;
            }
            endChunk();
            //qDebug() << "Objects table section:" << (filePosition() - objectIndexStart) << "bytes (" << numObjectsWritten << "objects)";

            // Write index of tables.
            *this << fieldTableStart << (quint32)_fields.size();
            *this << classTableStart << (quint32)_classes.size();
            *this << objectIndexStart << (quint32)numObjectsWritten;
        }
        catch(...) {
            SaveStream::close();
            throw;
        }
    }

    SaveStream::close();
}

/******************************************************************************
* Writes the information associated with an object class to the stream.
******************************************************************************/
void ObjectSaveStream::serializeObjectClass(OvitoClassPtr clazz)
{
    OVITO_ASSERT(clazz);
    OVITO_ASSERT(clazz->isDerivedFrom(RefTarget::OOClass()));

    // Is the class tagged as nonessential?
    // For non-essential classes it is not an error in case they get removed in a future version of OVITO
    // or if objects of this class cannot be deserialized from a state file.
    // In other words, it's okay to remove such classes in a future version of OVITO without breaking compatibility with older state files.
    bool isNonessentialClass = (clazz->classMetadata("NonessentialClass").isEmpty() == false);
    beginChunk(isNonessentialClass ? 0x202 : 0x201);

    // Write the class' name to the stream.
    *this << QByteArray::fromRawData(clazz->className(), qstrlen(clazz->className()));
    // Write the class' super class to the stream. Cut off at the RefTarget root class.
    *this << registerObjectClass(clazz != &RefTarget::OOClass() ? clazz->superClass() : nullptr);

    endChunk();
}

/******************************************************************************
* Registers a parameter field of a class for serialization.
******************************************************************************/
quint32 ObjectSaveStream::registerParameterField(const PropertyFieldDescriptor* field)
{
    OVITO_ASSERT(field);
    OVITO_ASSERT(_fields.size() == _fieldMap.size());

    quint32& id = _fieldMap[field];
    if(id == 0) {
        // Add field to serialization set and give it a unique serialization ID.
        _fields.push_back(field);
        id = static_cast<quint32>(_fields.size());
    }
    else {
        OVITO_ASSERT(_fields[id - 1] == field);
    }
    return id;
}

/******************************************************************************
* Writes the metadata associated with a parameter field to the stream.
******************************************************************************/
void ObjectSaveStream::serializeParameterField(const PropertyFieldDescriptor* field)
{
    *this << registerObjectClass(field->definingClass());
    *this << QByteArray::fromRawData(field->identifier(), qstrlen(field->identifier()));
    *this << field->flags();
    *this << field->isReferenceField();
    if(field->isReferenceField())
        *this << registerObjectClass(field->targetClass());
}

/******************************************************************************
* Serializes the values of an object's parameter fields.
* This method is called from RefTarget::saveToStream().
******************************************************************************/
void ObjectSaveStream::serializeParameterFieldValues(const RefTarget* object)
{
    OVITO_ASSERT(_currentObjectRecord != nullptr);
    OVITO_ASSERT(_currentObjectRecord->object.get() == object);
    const ObjectRecord& record = *_currentObjectRecord;
    const RefTarget* refObject = record.deltaReferenceObject.get();

#if 0
    qDebug() << "Saving object" << object << "(ID" << _objectMap[object] << ")";
#endif

    // Iterate over all property fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : object->getOOMetaClass().propertyFields()) {
        if(field->dontSerialize())
            continue; // Skip non-serializable fields.

        try {
            // Serialize reference or property field.
            if(field->isReferenceField()) {
                // Write the object pointed to by the reference field to the stream.
                if(!field->isVector()) {
                    // Get the current referenced target.
                    const RefTarget* target = object->getReferenceFieldTarget(field);

                    if(refObject) {
                        // Get the corresponding target from the reference object.
                        const RefTarget* refTarget = refObject->getReferenceFieldTarget(field);
                        // Skip field if the value is null in both the object and the reference object.
                        if(!target && !refTarget)
                            continue;
                    }

                    // Serialize the referenced sub-object.
                    if(target) {
                        OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
                        quint32 subobjectId = _objectMap.find(target)->second;
                        if(!_objects[subobjectId - 1].maybeSkipped) {
#if 0
                            qDebug() << "  Writing reference field" << field->identifier() << "of" << object;
#endif
                            beginChunk(registerParameterField(field));
                            *this << subobjectId;
                            endChunk();
                        }
                    }
                    else {
                        beginChunk(registerParameterField(field));
                        *this << (quint32)0; // Write an explicit null reference.
                        endChunk();
                    }
                }
                else {
                    qint32 count = object->getVectorReferenceFieldSize(field);

                    // Check if the complete list of referenced targets can be skipped.
                    if(refObject) {
                        qint32 refcount = refObject->getVectorReferenceFieldSize(field);
                        if(refcount == count) {
                            bool canSkipAll = true;
                            for(qint32 i = 0; i < count; i++) {
                                const RefTarget* target = object->getVectorReferenceFieldTarget(field, i);
                                if(!target) {
                                    canSkipAll = false;
                                    break;
                                }
                                else {
                                    OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
                                    quint32 subobjectId = _objectMap.find(target)->second;
                                    if(!_objects[subobjectId - 1].maybeSkipped) {
                                        canSkipAll = false;
                                        break;
                                    }
                                }
                            }
                            if(canSkipAll)
                                continue;
                        }
                    }
#if 0
                    qDebug() << "  Writing reference field" << field->identifier() << "of" << object;
#endif
                    beginChunk(registerParameterField(field));
                    *this << count;
                    for(qint32 i = 0; i < count; i++) {
                        // Get the current referenced target.
                        RefTarget* target = object->getVectorReferenceFieldTarget(field, i);
                        // Serialize the referenced sub-object.
                        quint32 subobjectId = 0;
                        if(target) {
                            OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
                            subobjectId = _objectMap.find(target)->second;
                            if(_objects[subobjectId - 1].maybeSkipped) {
                                subobjectId = std::numeric_limits<quint32>::max(); // Indicate skipped sub-object.
                            }
                        }
                        *this << subobjectId;
                    }
                    endChunk();
                }
            }
            else {
                // Check if the property value differs from the parameter's default value.
                if(refObject) {
                    if(object->comparePropertyFieldValue(field, *refObject)) {
#if 0
                        qDebug() << "  Skipping unchanged parameter" << field->identifier() << "of" << object;
#endif
                        continue; // Skip serializing this value if it is equal to the default value.
                    }
                }

                // Write the value stored in the property field to the stream.
                beginChunk(registerParameterField(field));
#if 0
                qDebug() << "  Writing parameter" << field->identifier() << "of" << object;
#endif
                OVITO_ASSERT(field->_propertyStorageSaveFunc != nullptr);
                field->_propertyStorageSaveFunc(object, field, *this);
                endChunk();
            }
        }
        catch(Exception& ex) {
            throw ex.prependGeneralMessage(tr("Failed to serialize contents of property field %1 of class %2.").arg(field->identifier()).arg(field->definingClass()->name()));
        }
    }
    beginChunk(0); // Terminator to mark end of parameter list.
    endChunk();
}

}   // End of namespace
