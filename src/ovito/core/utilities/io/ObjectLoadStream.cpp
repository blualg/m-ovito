////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include "ObjectLoadStream.h"

namespace Ovito {

/******************************************************************************
* Opens the stream for reading.
******************************************************************************/
ObjectLoadStream::ObjectLoadStream(QDataStream& source) : LoadStream(source)
{
    qint64 oldPos = filePosition();

    // Jump to index at the end of the file.
    setFilePosition(source.device()->size() - 2 * (qint64)(sizeof(qint64) + sizeof(quint32)));

    // Read index of tables.
    qint64 classTableStart, objectTableStart;
    quint32 classCount, objectCount;
    *this >> classTableStart;
    *this >> classCount;
    *this >> objectTableStart;
    *this >> objectCount;

    // Jump to beginning of the class table.
    setFilePosition(classTableStart);
    expectChunk(0x200);
    _classes.resize(classCount);
    for(auto& classInfo : _classes) {

        // Read the runtime type from the stream.
        expectChunk(0x201);
        OvitoClassPtr clazz = OvitoClass::deserializeRTTI(*this);
        closeChunk();

        // Load the plugin containing the class.
        clazz->plugin()->loadPlugin();

        // Create the class info structure.
        classInfo = clazz->createClassInfoStructure();
        classInfo->clazz = clazz;

        // Let the metaclass read its specific information from the stream.
        expectChunk(0x202);
        clazz->loadClassInfo(*this, classInfo.get());
        closeChunk();
    }
    closeChunk();

    // Jump to start of object table.
    setFilePosition(objectTableStart);
    expectChunk(0x300);
    _objects.resize(objectCount);
    for(ObjectRecord& record : _objects) {
        record.object = nullptr;
        quint32 classId;
        *this >> classId;
        record.classInfo = _classes[classId].get();
        *this >> record.fileOffset;
    }
    closeChunk();

    // Go back to previous position in file.
    setFilePosition(oldPos);
}

/******************************************************************************
* Loads an object with runtime type information from the stream.
* The method returns a pointer to the object but this object will be
* in an uninitialized state until it is loaded at a later time.
******************************************************************************/
OORef<OvitoObject> ObjectLoadStream::loadObjectInternal()
{
    quint32 objectId;
    *this >> objectId;
    if(objectId == 0) {
        return {};
    }
    else {
        ObjectRecord& record = _objects[objectId - 1];
        if(record.object != nullptr) {
            return record.object;
        }
        else {
            // Create an instance of the object class.
            if(record.classInfo->clazz->isDerivedFrom(RefTarget::OOClass()))
                record.object = record.classInfo->clazz->createInstance(ObjectInitializationFlag::DontInitializeObject);
            else
                record.object = record.classInfo->clazz->createInstance();

            if(record.classInfo->clazz == &DataSet::OOClass()) {
                // When deserializing a DataSet, the caller may have provided a pre-existing DataSet which should be populated with the serialized sub-objects.
                // Otherwise, create a new DataSet instance.
                if(_dataset == nullptr) {
                    setDatasetToBePopulated(static_object_cast<DataSet>(record.object.get()));
                }
                else {
                    // If an existing DataSet has been provided, load the objects from the stream into
                    // that existing Dataset instead of creating a new one. This feature is used in DataSet::loadFromFile();
                    record.object = _dataset;
                }
            }
            else {
                OVITO_ASSERT(!record.classInfo->clazz->isDerivedFrom(DataSet::OOClass()));
            }

            _objectsToLoad.push_back(objectId - 1);
            return record.object;
        }
    }
}

/******************************************************************************
* Closes the stream.
******************************************************************************/
void ObjectLoadStream::close()
{
    // This prevents re-entrance in case of an exception.
    if(!_currentObject) {

        // Note: Not using range-based for-loop here, because new objects may be appended to the list at any time.
        for(int i = 0; i < _objectsToLoad.size(); i++) { // NOLINT(modernize-loop-convert)
            quint32 index = _objectsToLoad[i];
            _currentObject = &_objects[index];
            OVITO_CHECK_POINTER(_currentObject);
            OVITO_CHECK_OBJECT_POINTER(_currentObject->object);

            // Seek to object data.
            setFilePosition(_currentObject->fileOffset);

            // Load class contents.
            try {
                // Make the object being loaded a child of this stream object.
                // This is to let the OvitoObject::isBeingLoaded() function detect that
                // the object is being loaded from this stream.
                OVITO_ASSERT(_currentObject->object->parent() == nullptr);
                _currentObject->object->setParent(this);
                OVITO_ASSERT(_currentObject->object->isBeingLoaded());

                // Let the object load its data fields.
                _currentObject->object->loadFromStream(*this);
            }
            catch(Exception& ex) {
                // Clear the being-loaded status of all objects.
                for(const ObjectRecord& record : _objects) {
                    if(record.object)
                        record.object->setParent(nullptr);
                }
                throw ex.appendDetailMessage(tr("Object of class type %1 failed to load.").arg(_currentObject->object->getOOClass().name()));
            }
        }

        // Now that all references are in place call, post-processing function on each loaded object.
        for(const ObjectRecord& record : _objects) {
            if(record.object)
                record.object->loadFromStreamComplete(*this);
        }

        // Clear the being-loaded status of all objects.
        for(const ObjectRecord& record : _objects) {
            if(record.object) {
                OVITO_ASSERT(record.object->parent() == this);
                record.object->setParent(nullptr);
            }
        }
    }
    LoadStream::close();
}

}   // End of namespace
