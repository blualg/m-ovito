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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/utilities/io/LoadStream.h>

namespace Ovito {

/**
 * \brief An input stream that can deserialize a RefTarget object graph stored in a file.
 *
 * This class restores an object graph previously saved with the ObjectSaveStream class.
 *
 * \sa ObjectSaveStream
 */
class OVITO_CORE_EXPORT ObjectLoadStream : public LoadStream
{
    Q_OBJECT

public:

    /// \brief Initializes the ObjectLoadStream.
    /// \param source The Qt data stream from which the data is read. This stream must support random access.
    /// \throw Exception if the source stream does not support random access, or if an I/O error occurs.
    explicit ObjectLoadStream(QDataStream& source);

    // Calls close() to close the ObjectLoadStream.
    virtual ~ObjectLoadStream() { ObjectLoadStream::close(); }

    /// \brief Closes the ObjectLoadStream, but not the underlying QDataStream passed to the constructor.
    virtual void close();

    /// \brief Returns an object to be deserialized from the stream.
    /// \note The returned object is not fully initialized yet when the function returns, and should not be accessed.
    ///       The object's contents are loaded later when close() is called.
    template<class T>
    OORef<T> lookupObject(quint32 objectId, T* existingObject = nullptr) {
        OORef<RefTarget> ptr = lookupObjectInternal(objectId, existingObject, true);
        OVITO_ASSERT(!ptr || ptr->getOOClass().isDerivedFrom(T::OOClass()));
        if(ptr && !ptr->getOOClass().isDerivedFrom(T::OOClass()))
            throw Exception(tr("Class hierarchy mismatch in file. The object class '%1' is not derived from '%2'.").arg(ptr->getOOClass().name()).arg(T::OOClass().name()));
        return static_object_cast<T>(std::move(ptr));
    }

    /// \brief Loads an object from the stream.
    /// \note The returned object is not initialized yet when the function returns, and should not be accessed.
    ///       The object's contents are loaded when close() is called.
    template<class T>
    OORef<T> loadObject(T* existingObject = nullptr) {
        quint32 objectId;
        (*this) >> objectId;
        return lookupObject<T>(objectId, existingObject);
    }

    /// \brief Loads a weak reference to an object from the stream.
    /// \note The returned object is not initialized yet when the function returns, and should not be accessed.
    ///       The object's contents are loaded when close() is called.
    template<class T>
    OORef<T> loadWeakObjectReference() {
        return loadObject<T>();
    }

    /// Registers a callback function that will be executed after the object graph has been completely loaded.
    /// This is useful for post-processing or manipulating of the loaded data if access to the entire object graph is required.
    void registerPostLoadCallback(fu2::unique_function<void()> callback) {
        OVITO_ASSERT(callback);
        _postLoadCallbacks.push_back(std::move(callback));
    }

    /// \brief Returns the DataSet object that is currently being loaded from the stream, if any.
    DataSet* datasetBeingLoaded() const;

private:

    /// Loads an object with runtime type information from the stream.
    OORef<RefTarget> lookupObjectInternal(quint32 objectId, RefTarget* existingObject, bool resetExistingObject = false);

    /// Metadata for a parameter field loaded from the stream.
    struct FieldRecord : RefTarget::SerializedPropertyField
    {
        /// An optional pointer to a custom function that takes are of the deserialization
        /// of this property field.
        CustomDeserializationFunctionPtr customDeserializationFunction = nullptr;
    };

    /// Metadata for a RefTarget-derived class loaded from the stream.
    struct ClassRecord
    {
        /// The class' name.
        QByteArray name;

        /// The run-time metaclass.
        const RefTarget::OOMetaClass* clazz = nullptr;

        /// The parent class.
        /// May not be set for legacy state files.
        const ClassRecord* superClass = nullptr;

        /// All serialized fields associated with this class, including those of parent classes.
        std::vector<FieldRecord*> fields;

        /// Indicates whether the list above already includes fields from all parent classes.
        /// Initially, it only contains fields defined in this class.
        bool parentClassFieldsIncluded = false;

        /// All parameter field definitions loaded from a legacy state file (OVITO 3.14.x or earlier).
        std::vector<FieldRecord> legacyFields;
    };

    /// Data structure describing a single object instance loaded from the file.
    struct ObjectRecord {

        /// The object instance created from the serialized data.
        OORef<RefTarget> object;

        /// The serialized class information.
        const ClassRecord* classRecord = nullptr;

        /// The byte offset at which the object's serialized data is stored in the file.
        qint64 fileOffset;

        /// Indicates whether existing sub-objects of this object should be reused during deserialization.
        bool reuseExistingSubobjects = true;
    };

    /// Reads a reference to a class definition from the stream.
    ClassRecord* readClassReference();

    /// Parses the definition of a parameter field from the stream.
    void deserializeParameterField(FieldRecord& fieldRecord);

    /// Parses the definition of an object class from the stream.
    void deserializeObjectClass(ClassRecord& classRecord);

    /// Parses the definition of an object instance from the stream.
    void deserializeObjectInstance(quint32 index, ObjectRecord& objectRecord);

    /// Lets runtime classes override deserialization behavior for individual parameter fields.
    void registerParameterFieldHandlers(ClassRecord& classRecord);

    /// Deserializes the values of the current object's parameter fields.
    /// This method is called from RefTarget::loadFromStream().
    void deserializeParameterFieldValues(RefTarget* object);

    /// Parses the definition of an object class's parameter fields from a state
    /// file written by OVITO 3.14.x or earlier.
    void parseLegacyParameterFields(ClassRecord& classRecord);

    /// All serialized parameter field definitions found in the file across all classes.
    std::vector<FieldRecord> _fields;

    /// All serialized classes found in the file.
    std::vector<ClassRecord> _classes;

    /// All object instances found in the file.
    std::vector<ObjectRecord> _objects;

    /// Objects that need to be deserialized.
    std::vector<quint32> _objectsToLoad;

    /// All objects mapped by their object ID.
    std::unordered_map<quint32, ObjectRecord*> _objectMap;

    /// The object object currently being loaded from the stream.
    ObjectRecord* _currentObjectRecord = nullptr;

    /// List of callbacks that are executed after the object graph has been completely loaded.
    std::vector<fu2::unique_function<void()>> _postLoadCallbacks;

    friend class RefTarget; // for access to deserializeParameterFieldValues()
};

}   // End of namespace
