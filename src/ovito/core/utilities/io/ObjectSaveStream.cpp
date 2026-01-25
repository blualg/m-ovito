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
* Registers an object instance to be written to the stream.
******************************************************************************/
quint32 ObjectSaveStream::registerObjectInstance(const RefTarget* object, bool excludeRecomputableData, const RefTarget* deltaReferenceObject)
{
    OVITO_ASSERT(_saveOnlyModifiedObjects || deltaReferenceObject == nullptr);

    if(object == nullptr) {
        // Object ID zero is reserved for null pointers.
        return 0;
    }

    // Instead of saving the object's data immediately, we only assign a unique instance ID to the object here
    // and write that ID to the stream. The object itself will get saved later when the stream is being closed.
    OVITO_CHECK_OBJECT_POINTER(object);
    OVITO_ASSERT(_objects.size() == _objectMap.size());
    OVITO_ASSERT(!deltaReferenceObject || &object->getOOClass() == &deltaReferenceObject->getOOClass());

    quint32& id = _objectMap[object];
    if(id == 0) {
        // If the saveOnlyModifiedObjects mode is active, we cannot register further objects for serialization
        // while we are already in the process of serializing objects.
        OVITO_ASSERT(!_saveOnlyModifiedObjects || !_currentObjectRecord);
        if(_saveOnlyModifiedObjects && _currentObjectRecord)
            throw Exception("Cannot register another object instances for serialization while objects are already being serialized.");

        // To serialize only changed parameters, create a default-constructed object instance as reference to compare against.
        OORef<const RefTarget> defaultConstructedObject = deltaReferenceObject;
        if(_saveOnlyModifiedObjects && !defaultConstructedObject) {
            // Temporarily establish a non-interactive context to always initialize
            // object parameters to factory default settings.
            NoninteractiveContext noninteractiveContext;
            defaultConstructedObject = static_object_cast<RefTarget>(object->getOOClass().createInstance());
        }

        // Add object to serialization set and give it a unique serialization ID.
        _objects.push_back({object, defaultConstructedObject, excludeRecomputableData});
        id = static_cast<quint32>(_objects.size());

        // Determine whether the object completely matches the provided reference object state
        // and recursively gather all sub-objects for serialization.
        if(_saveOnlyModifiedObjects)
            gatherSubObjectsAndDetectInitialState(id);
    }
    else if(!_saveOnlyModifiedObjects || !_currentObjectRecord) {
        // Object has already been registered for serialization before.
        // Verify that the previous registration is consistent with the current one.
        ObjectRecord& record = _objects[id - 1];
        OVITO_ASSERT(record.object == object);

        // The excludeRecomputableData option must be specified by all registrations.
        // Otherwise, we have to reset the flag and include recomputable data.
        if(!excludeRecomputableData) {
            record.excludeRecomputableData = false;
        }

        // If not the same delta reference object is used for all registrations,
        // the object must be serialized.
        if(deltaReferenceObject != record.deltaReferenceObject) {
            record.canBeSkipped = false;
        }
    }

    return id;
}

/******************************************************************************
* Saves an object with runtime type information to the stream.
******************************************************************************/
void ObjectSaveStream::saveObject(const RefTarget* object, bool excludeRecomputableData)
{
    // TODO: Allow serialization of further objects during object serialization?
    OVITO_ASSERT(!_saveOnlyModifiedObjects || _currentObjectRecord == nullptr);

    // Register object and write its unique serialization ID to the stream.
    *this << registerObjectInstance(object, excludeRecomputableData);
}

/******************************************************************************
* Determines whether the given object completely matches its corresponding reference state
* and recursively gathers its sub-object references for serialization.
******************************************************************************/
void ObjectSaveStream::gatherSubObjectsAndDetectInitialState(quint32 objectId)
{
    const RefTarget* object = _objects[objectId - 1].object.get();
    const RefTarget* refObject = _objects[objectId - 1].deltaReferenceObject.get();
    bool excludeRecomputableData = _objects[objectId - 1].excludeRecomputableData;
    bool allParametersMatch = true;
    OVITO_ASSERT(refObject);

    // Visit all property fields of the object.
    for(const PropertyFieldDescriptor* field : object->getOOMetaClass().propertyFields()) {
        if(field->dontSerialize())
            continue; // Skip non-serializable fields.

        if(field->isReferenceField()) {
            if(!field->isVector()) {
                // Get the current referenced target and obtain the corresponding sub-object from the delta reference object.
                const RefTarget* target = object->getReferenceFieldTarget(field);
                const RefTarget* refTarget = refObject->getReferenceFieldTarget(field);
                allParametersMatch &= ((bool)target == (bool)refTarget);

                // Don't use delta reference if types differ.
                if(refTarget && target && &target->getOOClass() != &refTarget->getOOClass()) {
                    refTarget = nullptr;
                    allParametersMatch = false;
                }

                // Process the referenced sub-object.
                if(quint32 subobjectId = registerObjectInstance(target, excludeRecomputableData || field->dontSaveRecomputableData(), refTarget)) {
                    allParametersMatch &= _objects[subobjectId - 1].canBeSkipped;
                }
            }
            else {
                const auto count = object->getVectorReferenceFieldSize(field);
                const auto refCount = refObject->getVectorReferenceFieldSize(field);
                allParametersMatch &= (count == refCount);
                for(int i = 0; i < count; i++) {
                    // Get the current referenced target.
                    const RefTarget* target = object->getVectorReferenceFieldTarget(field, i);

                    // Obtain the corresponding sub-object from the delta reference object.
                    const RefTarget* refTarget = nullptr;
                    if(i < refCount) {
                        refTarget = refObject->getVectorReferenceFieldTarget(field, i);
                        allParametersMatch &= ((bool)target == (bool)refTarget);
                        // Don't use delta reference if types differ.
                        if(refTarget && target && &target->getOOClass() != &refTarget->getOOClass()) {
                            refTarget = nullptr;
                            allParametersMatch = false;
                        }
                    }

                    // Process the referenced sub-object.
                    if(quint32 subobjectId = registerObjectInstance(target, excludeRecomputableData || field->dontSaveRecomputableData(), refTarget)) {
                        allParametersMatch &= _objects[subobjectId - 1].canBeSkipped;
                    }
                }
            }
        }
        else if(allParametersMatch) {
            // Check if the property value differs from the parameter's default value.
            allParametersMatch &= object->comparePropertyFieldValue(field, *refObject);
        }
    }

    // TODO: Ask the object's class if it considers the object to match its reference state.
    //if(allParametersMatch)
    //    allParametersMatch &= object->doesMatchReferenceState(*this, excludeRecomputableData, refObject);

    // Mark object as skippable if all its parameters match the reference state.
    _objects[objectId - 1].canBeSkipped = allParametersMatch;
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
    if(!objectRecord.canBeSkipped)
        return false; // Already determined to be non-skippable.

    const RefTarget* object = objectRecord.object.get();

    // Visit all reference fields of the object.
    for(const PropertyFieldDescriptor* field : object->getOOMetaClass().propertyFields()) {
        if(field->dontSerialize() || !field->isReferenceField())
            continue; // Skip non-serializable fields and non-reference fields.

        if(!field->isVector()) {
            if(RefTarget* target = object->getReferenceFieldTarget(field)) {
                // Sub-object must have been registered already.
                OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
                // The current object can only be skipped if all its referenced sub-objects can also be skipped.
                objectRecord.canBeSkipped &= checkSkippableSubTree(_objects[_objectMap.find(target)->second - 1]);
            }
        }
        else {
            const auto count = object->getVectorReferenceFieldSize(field);
            for(int i = 0; i < count; i++) {
                if(RefTarget* target = object->getVectorReferenceFieldTarget(field, i)) {
                    // Sub-object must have been registered already.
                    OVITO_ASSERT(_objectMap.find(target) != _objectMap.end());
                    // The current object can only be skipped if all its referenced sub-objects can also be skipped.
                    objectRecord.canBeSkipped &= checkSkippableSubTree(_objects[_objectMap.find(target)->second - 1]);
                }
            }
        }
    }

    return objectRecord.canBeSkipped;
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
            if(_saveOnlyModifiedObjects) {
                for(ObjectRecord& record : _objects) {
                    checkSkippableSubTree(record);
                }
            }

            // Serialize each object.
            // Note: Not using range-based for-loop here, because further objects may be appended to the list as we save objects.
            qint64 startObjectSection = filePosition();
            beginChunk(0x100);
            for(size_t i = 0; i < _objects.size(); i++) { // NOLINT(modernize-loop-convert)
                ObjectRecord& record = _objects[i];
                if(record.canBeSkipped)
                    continue; // Omit object from file that match their reference state.

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
                OVITO_ASSERT(_saveOnlyModifiedObjects || !objectRecord.canBeSkipped);
                if(objectRecord.canBeSkipped)
                    continue; // Omit object from file that match their reference state.
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
    qDebug() << "Saving object" << object;
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
                    quint32 subobjectId = registerObjectInstance(target, record.excludeRecomputableData || field->dontSaveRecomputableData());

                    // Serialize the referenced sub-object (unless it is skipped).
                    if(!subobjectId || !_objects[subobjectId - 1].canBeSkipped) {
#if 0
                        qDebug() << "  Writing reference field" << field->identifier() << "of" << object;
#endif
                        beginChunk(registerParameterField(field));
                        *this << subobjectId;
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
                                quint32 subobjectId = registerObjectInstance(target, record.excludeRecomputableData || field->dontSaveRecomputableData());
                                if(!subobjectId || !_objects[subobjectId - 1].canBeSkipped) {
                                    canSkipAll = false;
                                    break;
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
                        quint32 subobjectId = registerObjectInstance(target, record.excludeRecomputableData || field->dontSaveRecomputableData());
                        if(subobjectId && _objects[subobjectId - 1].canBeSkipped)
                            subobjectId = std::numeric_limits<quint32>::max(); // Indicate skipped sub-object.
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
