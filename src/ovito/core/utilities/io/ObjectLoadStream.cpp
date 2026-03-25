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
#include <ovito/core/oo/OORef.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/concurrent/NoninteractiveContext.h>
#include "ObjectLoadStream.h"

namespace Ovito {

/******************************************************************************
* Opens the stream for reading.
******************************************************************************/
ObjectLoadStream::ObjectLoadStream(QDataStream& source, bool untrustedContents) : LoadStream(source), _untrustedContents(untrustedContents)
{
    qint64 oldPos = filePosition();

    // Jump to index at the end of the file.
    int numIndexTables = (formatVersion() >= 30016 ? 3 : 2);
    setFilePosition(source.device()->size() - numIndexTables * (sizeof(qint64) + sizeof(quint32)));

    // Read index of tables.
    qint64 fieldTableStart, classTableStart, objectTableStart;
    quint32 fieldCount, classCount, objectCount;
    if(formatVersion() >= 30016) { // New file format with field table since OVITO 3.15.0.
        *this >> fieldTableStart;
        *this >> fieldCount;
    }
    *this >> classTableStart;
    *this >> classCount;
    *this >> objectTableStart;
    *this >> objectCount;

    // Read class table.
    setFilePosition(classTableStart);
    expectChunk(0x200);
    _classes.resize(classCount);
    for(auto& classRecord : _classes) {
        deserializeObjectClass(classRecord);
    }
    closeChunk();

    // Read parameter field table.
    if(formatVersion() >= 30016) {
        setFilePosition(fieldTableStart);
        expectChunk(0x101);
        _fields.resize(fieldCount);
        for(auto& fieldRecord : _fields) {
            deserializeParameterField(fieldRecord);
        }
        closeChunk();
    }

    // Let runtime classes override deserialization behavior for individual parameter fields.
    for(auto& classRecord : _classes) {
        registerParameterFieldHandlers(classRecord);
    }

    // Read object table.
    setFilePosition(objectTableStart);
    expectChunk(0x300);
    _objects.resize(objectCount);
    _objectMap.reserve(objectCount);
    for(auto [index, objectRecord] : Ovito::enumerate(_objects)) {
        deserializeObjectInstance(index, objectRecord);
    }
    closeChunk();

    // Go back to previous position in file.
    setFilePosition(oldPos);
}

/******************************************************************************
* Reads a reference to a class definition from the stream.
******************************************************************************/
ObjectLoadStream::ClassRecord* ObjectLoadStream::readClassReference()
{
    quint32 id;
    *this >> id;
    if(id == 0) {
        return nullptr;
    }
    else {
        if(id > _classes.size())
            throw Exception(tr("Class ID %1 is out of range.").arg(id));
        return &_classes[id - 1];
    }
}

/******************************************************************************
* Parses the definition of an object class from the stream.
******************************************************************************/
void ObjectLoadStream::deserializeObjectClass(ClassRecord& classRecord)
{
    int chunkId = expectChunkRange(0x201, 1);

    // Was the class tagged as nonessential?
    // For nonessential classes it is not an error if they got removed in the current version of OVITO
    // or if objects of this class cannot be deserialized from the state file.
    bool isNonessentialClass = (chunkId != 0);

    if(formatVersion() >= 30016) {
        // Parse the class' name.
        *this >> classRecord.name;
        // Parse the class' super class.
        classRecord.superClass = readClassReference();
    }
    else {
        // For backward compatibility with OVITO 3.14.x and earlier versions:
        // Read the runtime type from the stream.
        classRecord.clazz = static_cast<const RefTarget::OOMetaClass*>(OvitoClass::deserializeRTTI(*this, isNonessentialClass));
        if(classRecord.clazz)
            classRecord.name = classRecord.clazz->name().toLatin1();
        closeChunk();

        // Parse legacy parameter field definitions associated with this class.
        expectChunk(0x202);
        if(classRecord.clazz)
            parseLegacyParameterFields(classRecord);
    }
    closeChunk();

    // Resolve runtime metaclass.
    for(Plugin* plugin : PluginManager::instance().plugins()) {
        if(OvitoClassPtr clazz = plugin->findClass(classRecord.name)) {
            if(clazz->isDerivedFrom(RefTarget::OOClass())) {
                classRecord.clazz = static_cast<const RefTarget::OOMetaClass*>(clazz);
                break;
            }
        }
    }

    // Check whether the class is required but not available in this program version.
    if(!isNonessentialClass && !classRecord.clazz) {
        throw Exception(tr("Required object class '%1' not found in this program version. This state file cannot be loaded by this version of OVITO. Please contact the authors if you believe this is an error.").arg(classRecord.name));
    }
}

/******************************************************************************
* Parses the definition of an object instance from the stream.
******************************************************************************/
void ObjectLoadStream::deserializeObjectInstance(quint32 index, ObjectRecord& objectRecord)
{
    objectRecord.object = nullptr;
    quint32 objectId;
    if(formatVersion() >= 30016)
        *this >> objectId;
    else
        objectId = index + 1; // Legacy files had object IDs starting at 1 in sequential order.
    quint32 classId;
    *this >> classId;
    if(formatVersion() < 30016)
        classId++; // Adjust for legacy files where class IDs started at 0.
    if(classId > _classes.size())
        throw Exception(tr("Serialized class ID %1 is out of range.").arg(classId));
    objectRecord.classRecord = &_classes[classId - 1];
    *this >> objectRecord.fileOffset;
    _objectMap.emplace(objectId, &objectRecord);
}

/******************************************************************************
* Parses the definition of a parameter field from the stream.
******************************************************************************/
void ObjectLoadStream::deserializeParameterField(FieldRecord& fieldRecord)
{
    ClassRecord* definingClass = readClassReference();
    definingClass->fields.push_back(&fieldRecord);
    fieldRecord.definingClass = definingClass->clazz;
    *this >> fieldRecord.identifier;
    *this >> fieldRecord.flags;
    *this >> fieldRecord.isReferenceField;
    if(fieldRecord.isReferenceField)
        fieldRecord.targetClass = readClassReference()->clazz;
}

/******************************************************************************
* Parses the definition of an object class's parameter fields from a state
* file written by OVITO 3.14.x or earlier.
******************************************************************************/
void ObjectLoadStream::parseLegacyParameterFields(ClassRecord& classRecord)
{
    // Parse list of parameter field definitions.
    for(;;) {
        quint32 chunkId = openChunk();
        if(chunkId == 0x0) {
            closeChunk();
            break;  // End of list
        }
        if(chunkId != 0x1)
            throw Exception(tr("File format is invalid. Failed to load property fields of class %1.").arg(classRecord.name));

        FieldRecord fieldInfo;

        // Read serialized property field definition from input stream.
        *this >> fieldInfo.identifier;
        OvitoClassPtr definingClass = OvitoClass::deserializeRTTI(*this);
        OVITO_ASSERT(definingClass->isDerivedFrom(RefTarget::OOClass()));
        fieldInfo.definingClass = static_cast<const RefMakerClass*>(definingClass);
        *this >> fieldInfo.flags;
        *this >> fieldInfo.isReferenceField;
        fieldInfo.targetClass = fieldInfo.isReferenceField ? OvitoClass::deserializeRTTI(*this) : nullptr;
        closeChunk();

        // Give object class the chance to override deserialization behavior for this property field.
        fieldInfo.customDeserializationFunction = classRecord.clazz->overrideFieldDeserialization(*this, fieldInfo);
        if(!fieldInfo.customDeserializationFunction) {

            // Verify consistency of serialized and runtime class hierarchy.
            if(!classRecord.clazz->isDerivedFrom(*fieldInfo.definingClass)) {
                qDebug() << "WARNING:" << classRecord.clazz->name() << "is not derived from" << fieldInfo.definingClass->name() << ", which defines the field" << fieldInfo.identifier;
                throw Exception(tr("The class hierarchy stored in the file differs from the class hierarchy of the program."));
            }

            // Verify consistency  of serialized and runtime property field definitions.
            fieldInfo.field = fieldInfo.definingClass->findPropertyField(fieldInfo.identifier.constData(), true);
            if(fieldInfo.field) {
                if(fieldInfo.field->isReferenceField() != fieldInfo.isReferenceField ||
                        fieldInfo.field->isVector() != ((fieldInfo.flags & PROPERTY_FIELD_VECTOR) != 0) ||
                        (fieldInfo.isReferenceField && !fieldInfo.targetClass->isDerivedFrom(*fieldInfo.field->targetClass())))
                    throw Exception(RefMaker::tr("The type of stored property field '%1' in class %2 has changed.").arg(fieldInfo.identifier, fieldInfo.definingClass->name()));
            }
        }

        // Add property field to list of legacy fields.
        classRecord.legacyFields.push_back(std::move(fieldInfo));
    }

    // Add legacy fields to class's actual field list.
    for(FieldRecord& field : classRecord.legacyFields)
        classRecord.fields.push_back(&field);

    // Indicate that parent class fields are already included.
    classRecord.parentClassFieldsIncluded = true;
}

/******************************************************************************
* Lets runtime classes override deserialization behavior for individual parameter fields.
******************************************************************************/
void ObjectLoadStream::registerParameterFieldHandlers(ClassRecord& classRecord)
{
    // Collect all parameter fields associated with the class, including fields of the all parent classes.
    if(classRecord.parentClassFieldsIncluded == false) {
        for(const ClassRecord* clazz = classRecord.superClass; clazz != nullptr; clazz = clazz->superClass) {
            classRecord.fields.insert(classRecord.fields.end(), clazz->fields.cbegin(), clazz->fields.cend());
            if(clazz->parentClassFieldsIncluded == true)
                break;
        }
        classRecord.parentClassFieldsIncluded = true;
    }

    // Skip handler registration if this class is no longer defined in the current program version.
    if(!classRecord.clazz)
        return;

    // Give defining class the chance to override deserialization behavior for each property field.
    for(FieldRecord* fieldRecord : classRecord.fields) {
        // Skip if another class has already registered a custom handler function.
        if(fieldRecord->customDeserializationFunction)
            continue;

        // Check if class provides a custom handler function for the field.
        fieldRecord->customDeserializationFunction = classRecord.clazz->overrideFieldDeserialization(*this, *fieldRecord);

        // If not, perform a consistency check of serialized field definition and runtime field definition.
        if(!fieldRecord->customDeserializationFunction && !fieldRecord->field) {

            // Resolve runtime field and verify consistency of serialized and runtime property field definitions.
            fieldRecord->field = classRecord.clazz->findPropertyField(fieldRecord->identifier.constData(), true);
            if(fieldRecord->field) {
                if(fieldRecord->field->isReferenceField() != fieldRecord->isReferenceField ||
                        fieldRecord->field->isVector() != ((fieldRecord->flags & PROPERTY_FIELD_VECTOR) != 0) ||
                        (fieldRecord->isReferenceField && !fieldRecord->targetClass->isDerivedFrom(*fieldRecord->field->targetClass())))
                    throw Exception(RefMaker::tr("The type of stored property field '%1' in class %2 has changed.").arg(fieldRecord->identifier, fieldRecord->definingClass->name()));
            }
        }
    }
}

/******************************************************************************
* Loads an object with runtime type information from the stream.
* The method returns a pointer to the object but this object will be
* in an uninitialized state until it is loaded at a later time.
******************************************************************************/
OORef<RefTarget> ObjectLoadStream::lookupObjectInternal(quint32 objectId, RefTarget* existingObject, bool resetExistingObject)
{
    if(objectId == 0) {
        return {};
    }
    else {
        auto iter = _objectMap.find(objectId);
        if(iter == _objectMap.end())
            throw Exception(tr("Serialized object ID %1 is not defined or out of range.").arg(objectId));
        ObjectRecord& record = *iter->second;
        if(record.object != nullptr) {
            return record.object;
        }
        else if(record.classRecord == nullptr) {
            // The object's class is not available in this program version but it is marked as optional.
            return {};
        }
        else if(record.classRecord->clazz == nullptr) {
            throw Exception(tr("Tried to deserialize an object of type '%1' that is no longer available in this version of OVITO.")
                .arg(QString::fromLatin1(record.classRecord->name)));
        }
        else if(existingObject && &existingObject->getOOClass() == record.classRecord->clazz && !existingObject->isBeingLoaded()) {
            // Use the provided existing object instance.
            record.object = existingObject;
            record.reuseExistingSubobjects = !resetExistingObject;

            // Mark the object as being loaded.
            record.object->_flags.setFlag(OvitoObject::BeingLoaded, true);

            // Schedule the object for later deserialization.
            _objectsToLoad.push_back(objectId);

            return record.object;
        }
        else {
            // Temporarily establish a non-interactive context to always initialize
            // object parameters to factory default settings. This is necessary to
            // ensure that the loaded objects are in a consistent state even if parameters have
            // been added to the objects in newer OVITO versions since the session state file was written.
            NoninteractiveContext noninteractiveContext;

            // Create an instance of the object class.
            record.object = static_object_cast<RefTarget>(record.classRecord->clazz->createInstance());

            // Mark the object as being loaded.
            record.object->_flags.setFlag(OvitoObject::BeingLoaded, true);

            // Schedule the object for later deserialization.
            _objectsToLoad.push_back(objectId);

            return record.object;
        }
    }
}

/******************************************************************************
* Returns the DataSet object that is currently being loaded from the stream, if any.
******************************************************************************/
DataSet* ObjectLoadStream::datasetBeingLoaded() const
{
    for(const ObjectRecord& record : _objects) {
        if(record.object && record.object->getOOClass().isDerivedFrom(DataSet::OOClass())) {
            return static_object_cast<DataSet>(record.object.get());
        }
    }
    return nullptr;
}

/******************************************************************************
* Closes the stream.
******************************************************************************/
void ObjectLoadStream::close()
{
    // This prevents re-entrance in case of an exception.
    if(!_currentObjectRecord) {
        try {
            // Note: Not using range-based for-loop here, because new objects may be appended to the list at any time.
            for(int i = 0; i < _objectsToLoad.size(); i++) { // NOLINT(modernize-loop-convert)
                quint32 objectId = _objectsToLoad[i];
                _currentObjectRecord = _objectMap[objectId];
                RefTarget* currentObject = _currentObjectRecord->object.get();
                OVITO_ASSERT(currentObject != nullptr);
                OVITO_ASSERT(currentObject->isBeingLoaded());

                // Seek to object data.
                setFilePosition(_currentObjectRecord->fileOffset);

                // Load class contents.
                try {
                    // Let the object load its internal state.
                    currentObject->loadFromStream(*this);
                }
                catch(Exception& ex) {
                    throw ex.appendDetailMessage(tr("Object of class type %1 failed to load.").arg(currentObject->getOOClass().name()));
                }
            }

            // Now that all references are in place, call post-processing function on each loaded object.
            for(const ObjectRecord& record : _objects) {
                if(record.object)
                    record.object->loadFromStreamComplete(*this);
            }

            // Call post-load callbacks.
            for(auto& callback : _postLoadCallbacks)
                std::move(callback)();
            _postLoadCallbacks.clear();

            // Reset the is-being-loaded flag of all objects.
            for(const ObjectRecord& record : _objects) {
                if(record.object) {
                    OVITO_ASSERT(record.object->isBeingLoaded());
                    record.object->_flags.setFlag(OvitoObject::BeingLoaded, false);
                }
            }
        }
        catch(...) {
            // Clean up by resetting the is-being-loaded flag of all objects.
            for(const ObjectRecord& record : _objects) {
                if(record.object) {
                    OVITO_ASSERT(record.object->isBeingLoaded());
                    record.object->_flags.setFlag(OvitoObject::BeingLoaded, false);
                }
            }
            throw;
        }
    }

    LoadStream::close();
}

/******************************************************************************
* Deserializes the values of the current object's parameter fields.
* This method is called from RefTarget::loadFromStream().
******************************************************************************/
void ObjectLoadStream::deserializeParameterFieldValues(RefTarget* object)
{
    OVITO_ASSERT(_currentObjectRecord != nullptr);
    OVITO_ASSERT(_currentObjectRecord->object == object);
    OVITO_ASSERT(!object->isUndoRecording());

#if 0
    qInfo() << "Loading object" << object << "with reuseExistingSubobjects=" << _currentObjectRecord->reuseExistingSubobjects;
#endif

    // Helper function that deserializes the value of a single reference or property field.
    auto deserializeField = [&](const FieldRecord& fieldRecord) {
        const PropertyFieldDescriptor* field = fieldRecord.field;
        if(fieldRecord.customDeserializationFunction) {
            // The object class has provided its own custom deserialization function for this property field.
            fieldRecord.customDeserializationFunction(fieldRecord, *this, *object);
        }
        else if(fieldRecord.isReferenceField) {
            OVITO_ASSERT(fieldRecord.targetClass != nullptr);

            if(field != nullptr) {
                OVITO_ASSERT(field->isVector() == field->flags().testFlag(PROPERTY_FIELD_VECTOR));
                OVITO_ASSERT(fieldRecord.targetClass->isDerivedFrom(*field->targetClass()));
                if(!field->isVector()) {
                    quint32 objectId;
                    (*this) >> objectId;
                    OORef<RefTarget> target = lookupObjectInternal(objectId, _currentObjectRecord->reuseExistingSubobjects ? object->getReferenceFieldTarget(field) : nullptr);
                    if(target && !target->getOOClass().isDerivedFrom(*fieldRecord.targetClass)) {
                        throw Exception(tr("Incompatible object stored in reference field %1 of class %2. Expected class %3 but found class %4 in file.")
                            .arg(QString::fromUtf8(fieldRecord.identifier)).arg(fieldRecord.definingClass->name()).arg(fieldRecord.targetClass->name()).arg(target->getOOClass().name()));
                    }
#if 0
                    qInfo() << "  Reference field" << fieldRecord.identifier << "contains" << target;
#endif
                    field->_singleReferenceWriteFuncRef(object, field, std::move(target));
                }
                else {
                    // Load each serialized object and insert it into the vector reference field.
                    qint32 numEntries;
                    *this >> numEntries;
                    OVITO_ASSERT(numEntries >= 0);
                    qint32 oldCount = object->getVectorReferenceFieldSize(field);
                    for(qint32 i = 0; i < numEntries; i++) {
                        quint32 subobjectId;
                        *this >> subobjectId;
                        if(subobjectId != std::numeric_limits<quint32>::max()) {
                            OORef<RefTarget> target = lookupObjectInternal(subobjectId, (i < oldCount && _currentObjectRecord->reuseExistingSubobjects) ? object->getVectorReferenceFieldTarget(field, i) : nullptr);
                            if(target && !target->getOOClass().isDerivedFrom(*fieldRecord.targetClass)) {
                                throw Exception(tr("Incompatible object stored in reference field %1 of class %2. Expected class %3 but found class %4 in file.")
                                    .arg(QString::fromUtf8(fieldRecord.identifier)).arg(fieldRecord.definingClass->name(), fieldRecord.targetClass->name(), target->getOOClass().name()));
                            }
#if 0
                            qInfo() << "  Vector reference field" << fieldRecord.identifier << "contains" << target;
#endif
                            if(oldCount > i) {
                                // Reuse existing entry in the vector if possible.
                                field->_vectorReferenceSetFunc(object, field, i, target.get());
                            }
                            else {
                                // Insert new entry at the end of the vector.
                                field->_vectorReferenceInsertFunc(object, field, i, std::move(target));
                            }
                        }
                    }
                    if(oldCount > numEntries) {
                        // Remove any excess entries that may have existed in the vector before loading.
                        for(qint32 i = oldCount - 1; i >= numEntries; i--) {
                            field->_vectorReferenceRemoveFunc(object, field, i);
                        }
                    }
                }
            }
            else {
#if 0
                qInfo() << "  Reference field" << fieldRecord.identifier << "no longer exists.";
#endif
                // The serialized reference field no longer exists in the current program version.
                // Don't deserialize dead object(s).
                if(fieldRecord.flags & PROPERTY_FIELD_VECTOR) {
                    qint32 numEntries;
                    *this >> numEntries;
                    for(qint32 i = 0; i < numEntries; i++) {
                        quint32 subobjectId;
                        *this >> subobjectId;
                    }
                }
                else {
                    quint32 objectId;
                    *this >> objectId;
                }
            }
        }
        else {
            // Read the value of the property field from the stream.
            if(field && !field->dontSerialize()) {
                // For backward compatibility with OVITO 3.14.x and earlier:
                // Skip loading of runtime property fields that have been converted to regular property fields in OVITO 3.15.0.
                if(!field->flags().testFlag(PROPERTY_FIELD_WAS_RUNTIME_PROPERTY_FIELD) || formatVersion() >= 30016) {
#if 0
                    qDebug() << "  Loading parameter field" << fieldRecord.identifier;
#endif
                    // Call the property field's deserialization function.
                    OVITO_ASSERT(field->_propertyStorageLoadFunc);
                    field->_propertyStorageLoadFunc(object, field, *this);
                }
            }
            else {
                // The property field no longer exists.
                // Ignore chunk contents.
            }
        }
    };

    // Read property field values from the stream.
    if(formatVersion() >= 30016) {
        // Parse parameter field values stored in the new format.
        // List is terminated by a field ID of zero.
        for(;;) {
            quint32 fieldId = openChunk();
            if(fieldId == 0) {
                closeChunk();
                break;
            }
            if(fieldId > _fields.size())
                throw Exception(tr("Parameter field ID %1 is out of range.").arg(fieldId));
            deserializeField(_fields[fieldId - 1]);
            closeChunk();
        }
    }
    else {
        // For backward compatibility with OVITO 3.14.x and earlier versions:
        const ClassRecord* classRecord = _currentObjectRecord->classRecord;
        for(const FieldRecord& fieldRecord : classRecord->legacyFields) {
            int chunkId = openChunk();
            if(chunkId != 0x05)
                deserializeField(fieldRecord);
            closeChunk();
        }
    }

#if 0
    qInfo() << "Done loading automatic fields of" << object;
#endif
}

}   // End of namespace
