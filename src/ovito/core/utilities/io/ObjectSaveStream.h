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
#include <ovito/core/utilities/io/SaveStream.h>

namespace Ovito {

/**
 * \brief An output stream that can serialize a RefTarget object graph a file.
 *
 * This class is used to write OVITO state files, which are on-disk representations of
 * object reference graphs. The object graph can be read back from the file using ObjectLoadStream.
 *
 * \note All objects written to a stream must belong to the same DataSet.
 *
 * \sa ObjectLoadStream
 */
class OVITO_CORE_EXPORT ObjectSaveStream : public SaveStream
{
    Q_OBJECT

public:

    /// \brief Initializes the ObjectSaveStream.
    /// \param destination The Qt data stream to which data is written. This stream must support random access.
    /// \throw Exception if the source stream does not support random access, or if an I/O error occurs.
    explicit ObjectSaveStream(QDataStream& destination) : SaveStream(destination) {}

    /// Calls close() to close the ObjectSaveStream.
    virtual ~ObjectSaveStream();

    /// \brief Closes this ObjectSaveStream, but not the underlying QDataStream passed to the constructor.
    /// \throw Exception if an I/O error has occurred.
    virtual void close() override;

    /// \brief Registers an object class to be written to the stream.
    /// \return The unique serialization ID assigned to the class.
    quint32 registerObjectClass(OvitoClassPtr clazz);

    /// \brief Registers an object instance to be written to the stream.
    /// \return The unique serialization ID assigned to the object instance.
    quint32 registerObjectInstance(const RefTarget* object, bool excludeRecomputableData = false, const RefTarget* deltaReferenceObject = nullptr);

    /// \brief Serializes an object and writes its data to the output stream.
    /// \throw Exception if an I/O error has occurred.
    /// \sa ObjectLoadStream::loadObject()
    void saveObject(const RefTarget* object, bool excludeRecomputableData = false, const RefTarget* deltaReferenceObject = nullptr);

    /// \brief Serializes a weak reference to an object and writes it to the output stream.
    /// \throw Exception if an I/O error has occurred.
    /// \sa ObjectLoadStream::loadWeakObjectReference()
    void saveWeakObjectReference(const RefTarget* object) {
        saveObject(object, true); // exclude recomputable data for weak references until someone else requests saving a full object
    }

private:

    /// A data record kept for each object written to the stream.
    struct ObjectRecord {
        OORef<const RefTarget> object;
        OORef<const RefTarget> deltaReferenceObject;
        bool excludeRecomputableData;
        quint32 classId;
        qint64 byteOffset;
    };

    /// Serializes the values of the current object's parameter fields.
    /// This method is called from RefTarget::saveToStream().
    void serializeParameterFieldValues(const RefTarget* object);

    /// Registers a parameter field of a class for serialization.
    quint32 registerParameterField(const PropertyFieldDescriptor* field);

    /// Writes the metadata associated with a parameter field to the stream.
    void serializeParameterField(const PropertyFieldDescriptor* field);

    /// Writes the information associated with an object class to the stream.
    void serializeObjectClass(OvitoClassPtr clazz);

    /// Writes the information associated with an object instance to the stream.
    void serializeObjectInstance(const ObjectRecord& objectRecord);

    /// All objects registered for serialization and their corresponding numeric IDs.
    std::unordered_map<const RefTarget*, quint32> _objectMap;

    /// All objects registered for serialization ordered by numeric ID.
    std::vector<ObjectRecord> _objects;

    /// All object classes that have been written to the stream so far and their corresponding numeric IDs.
    std::unordered_map<OvitoClassPtr, quint32> _classMap;

    /// All object classes that have been written to the stream.
    std::vector<OvitoClassPtr> _classes;

    /// All parameter fields that have been written to the stream so far and their corresponding numeric IDs.
    std::unordered_map<const PropertyFieldDescriptor*, quint32> _fieldMap;

    /// All parameter fields that have been written to the stream.
    std::vector<const PropertyFieldDescriptor*> _fields;

    /// The object object currently being saved to the stream.
    ObjectRecord* _currentObjectRecord = nullptr;

    friend class RefTarget; // for access to serializeParameterFieldValues()
};

}   // End of namespace
