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
    if(object == nullptr) {
        // Object ID zero is reserved for null pointers.
        return 0;
    }

    // Instead of saving the object's data immediately, we only assign a unique instance ID to the object here
    // and write that ID to the stream. The object itself will get saved later when the stream
    // is being closed.
    OVITO_CHECK_OBJECT_POINTER(object);
    OVITO_ASSERT(_objects.size() == _objectMap.size());
    OVITO_ASSERT(!deltaReferenceObject || &object->getOOClass() == &deltaReferenceObject->getOOClass());
    quint32& id = _objectMap[object];
    if(id == 0) {
        // Add object to serialization queue.
        _objects.push_back({object, deltaReferenceObject, excludeRecomputableData});
        id = (quint32)_objects.size();
    }
    else {
        // Object has already been registered for serialization before.
        // Verify that the previous registration is consistent with the current one.
        OVITO_ASSERT(_objects[id-1].object == object);
        OVITO_ASSERT(!deltaReferenceObject || _objects[id-1].deltaReferenceObject);
        if(!excludeRecomputableData) {
            _objects[id-1].excludeRecomputableData = false;
        }
    }
    // Write the object's unique serialization ID to the stream.
    return id;
}

/******************************************************************************
* Saves an object with runtime type information to the stream.
******************************************************************************/
void ObjectSaveStream::saveObject(const RefTarget* object, bool excludeRecomputableData, const RefTarget* deltaReferenceObject)
{
    *this << registerObjectInstance(object, excludeRecomputableData, deltaReferenceObject);
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
            _classes.push_back(clazz);
            id = (quint32)_classes.size();
        }
        else {
            OVITO_ASSERT(_classes[id - 1] == clazz);
        }
        return id;
    }
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
            // Serialize each object.
            // Note: Not using range-based for-loop here, because further objects may be appended to the list as we save objects.
            qint64 startObjectSection = filePosition();
            beginChunk(0x100);
            for(size_t i = 0; i < _objects.size(); i++) { // NOLINT(modernize-loop-convert)
                ObjectRecord& record = _objects[i];
                OVITO_CHECK_OBJECT_POINTER(record.object);
                record.byteOffset = filePosition();
                _currentObjectRecord = &record;

                // Register the object's class for serialization.
                record.classId = registerObjectClass(&record.object->getOOClass());

                // Let the object class serialize its internal state.
                record.object->saveToStream(*this, record.excludeRecomputableData, record.deltaReferenceObject);
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
            //qDebug() << "Field table section:" << (filePosition() - fieldTableStart) << "bytes";

            // Save the list of classes.
            // Note: Not using range-based for-loop here, because further classes may be appended to the list as we save classes.
            qint64 classTableStart = filePosition();
            beginChunk(0x200);
            for(size_t i = 0; i < _classes.size(); i++) { // NOLINT(modernize-loop-convert)
                serializeObjectClass(_classes[i]);
            }
            endChunk();
            //qDebug() << "Class table section:" << (filePosition() - classTableStart) << "bytes";

            // Save object index table.
            qint64 objectIndexStart = filePosition();
            beginChunk(0x300);
            for(const auto& objectRecord : _objects) {
                serializeObjectInstance(objectRecord);
            }
            endChunk();
            //qDebug() << "Objects table section:" << (filePosition() - objectIndexStart) << "bytes";

            // Write index of tables.
            *this << fieldTableStart << (quint32)_fields.size();
            *this << classTableStart << (quint32)_classes.size();
            *this << objectIndexStart << (quint32)_objects.size();
        }
        catch(...) {
            SaveStream::close();
            throw;
        }
    }

    SaveStream::close();
}

/******************************************************************************
* Writes the information associated with an object instance to the stream.
******************************************************************************/
void ObjectSaveStream::serializeObjectInstance(const ObjectRecord& objectRecord)
{
    *this << objectRecord.classId;
    *this << objectRecord.byteOffset;
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
        _fields.push_back(field);
        id = (quint32)_fields.size();
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
                beginChunk(registerParameterField(field));
                if(!field->isVector()) {

                    // Get the current referenced target.
                    RefTarget* target = object->getReferenceFieldTarget(field);

                    // Check if the current referenced target is of the same type as the one in the delta reference object.
                    const RefTarget* deltaReferenceTarget = nullptr;
                    if(refObject) {
                        deltaReferenceTarget = refObject->getReferenceFieldTarget(field);
                        if(deltaReferenceTarget && target && &target->getOOClass() != &deltaReferenceTarget->getOOClass())
                            deltaReferenceTarget = nullptr; // Don't use delta reference if types differ.
                    }

                    // Serialize the referenced subobject.
                    saveObject(target, record.excludeRecomputableData || field->dontSaveRecomputableData(), deltaReferenceTarget);
                }
                else {
                    qint32 count = object->getVectorReferenceFieldSize(field);
                    *this << count;
                    for(int i = 0; i < count; i++) {
                        // Get the current referenced target.
                        RefTarget* target = object->getVectorReferenceFieldTarget(field, i);

                        // Check if the current referenced target is of the same type as the one in the delta reference object.
                        const RefTarget* deltaReferenceTarget = nullptr;
                        if(refObject) {
                            if(refObject->getVectorReferenceFieldSize(field) > i) {
                                deltaReferenceTarget = refObject->getVectorReferenceFieldTarget(field, i);
                                if(deltaReferenceTarget && target && &target->getOOClass() != &deltaReferenceTarget->getOOClass())
                                    deltaReferenceTarget = nullptr; // Don't use delta reference if types differ.
                            }
                        }

                        // Serialize the referenced subobject.
                        saveObject(target, record.excludeRecomputableData || field->dontSaveRecomputableData(), deltaReferenceTarget);
                    }
                }
                endChunk();
            }
            else {
                // Check if the property value differs from the parameter's default value.
                if(refObject) {
                    QVariant curValue = object->getPropertyFieldValue(field);
                    QVariant refValue = refObject->getPropertyFieldValue(field);
                    if(curValue.isValid() && refValue.isValid() && curValue == refValue) {
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
#if 0
                qDebug() << "  Property field" << field->identifier() << " contains" << field->propertyStorageReadFunc(object, field);
#endif
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
