////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include "DataObject.h"
#include "DataObjectPath.h"

namespace Ovito {

/**
 * \brief A reference to a DataObject in a PipelineFlowState.
 */
class OVITO_CORE_EXPORT DataObjectReference
{
public:

    /// \brief Default constructor. Creates a null reference.
    DataObjectReference() = default;

    /// \brief Constructs a reference to a data object.
    DataObjectReference(DataObjectClassPtr dataClass, const QString& dataPath = QString(), const QString& dataTitle = QString()) :
        _dataClass(dataClass), _dataPath(dataPath), _dataTitle(dataTitle) {}

    /// \brief Constructs a reference to a data object from a data object path.
    DataObjectReference(const ConstDataObjectPath& path) : DataObjectReference(path.empty() ? nullptr : &path.back()->getOOMetaClass(), path.toString(), path.toUIString()) {}

    /// Returns the DataObject subclass being referenced.
    DataObjectClassPtr dataClass() const { return _dataClass; }

    /// Returns the identifier and path of the data object being referenced.
    const QString& dataPath() const { return _dataPath; }

    /// Returns the title of the data object used in the user interface.
    const QString& dataTitle() const { return _dataTitle; }

    /// Returns the UI title of the referenced data object if available. Otherwise returns the data collection path as a fallback string representation.
    const QString& dataTitleOrPath() const { return _dataTitle.isEmpty() ? _dataPath : _dataTitle; }

    /// \brief Compares two references for equality.
    bool operator==(const DataObjectReference& other) const {
        return dataClass() == other.dataClass() && (dataPath() == other.dataPath() || dataPath().isEmpty() || other.dataPath().isEmpty());
    }

    /// \brief Compares two references for inequality.
    bool operator!=(const DataObjectReference& other) const { return !(*this == other); }

    /// \brief Strict ordering function.
    bool operator<(const DataObjectReference& other) const {
        if(dataClass() == other.dataClass()) {
            if(dataPath() == other.dataPath() || dataPath().isEmpty() || other.dataPath().isEmpty()) {
                return false;
            }
            else return dataPath() < other.dataPath();
        }
        else return dataClass() < other.dataClass();
    }

    /// \brief Returns whether this reference points to any data object.
    explicit operator bool() const {
        return dataClass() != nullptr;
    }

private:

    /// The DataObject subclass being referenced.
    DataObjectClassPtr _dataClass = nullptr;

    /// The identifier and path of the data object being referenced.
    QString _dataPath;

    /// The title of the data object used in the user interface (optional).
    QString _dataTitle;

    friend OVITO_CORE_EXPORT SaveStream& operator<<(SaveStream& stream, const DataObjectReference& r);
    friend OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, DataObjectReference& r);
};

/// Writes a DataObjectReference to a debug stream.
/// \relates DataObjectReference
inline OVITO_CORE_EXPORT QDebug operator<<(QDebug debug, const DataObjectReference& r)
{
    if(r) {
        debug.nospace() << "DataObjectReference("
            << r.dataClass()->name()
            << ", "
            << r.dataPath()
            << ", "
            << r.dataTitle() << ")";
    }
    else {
        debug << "DataObjectReference(<null>)";
    }
    return debug;
}

/// Writes a DataObjectReference to an output stream.
/// \relates DataObjectReference
inline OVITO_CORE_EXPORT SaveStream& operator<<(SaveStream& stream, const DataObjectReference& r)
{
    stream.beginChunk(0x02);
    stream << static_cast<const OvitoClassPtr&>(r.dataClass());
    stream << r.dataPath();
    stream << r.dataTitle();
    stream.endChunk();
    return stream;
}

/// Reads a DataObjectReference from an input stream.
/// \relates DataObjectReference
inline OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, DataObjectReference& r)
{
    stream.expectChunk(0x02);
    OvitoClassPtr clazz;
    stream >> clazz;
    r._dataClass = static_cast<DataObjectClassPtr>(clazz);
    stream >> r._dataPath;
    stream >> r._dataTitle;
    if(!r._dataClass)
        r._dataPath.clear();
    stream.closeChunk();
    // For backward compatibility with OVITO 3.2.1: The SpatialBinningModifier used to generate a VoxelGrid
    // with ID of the form "binning[<PROPERTY>]", but now the grid's ID is just "binning". We automatically
    // update references to the voxel grid when loading an OVITO file written by an old program version.
    if(stream.formatVersion() < 30006 && r._dataPath.startsWith(QStringLiteral("binning[")))
        r._dataPath = QStringLiteral("binning");
    return stream;
}

/**
 * A reference to a DataObject subclass.
 */
template<class DataObjectType>
class TypedDataObjectReference : public DataObjectReference
{
public:

    /// \brief Default constructor. Creates a null reference.
    TypedDataObjectReference() = default;

    /// \brief Conversion copy constructor.
    TypedDataObjectReference(const DataObjectReference& other) : DataObjectReference(other) {
        OVITO_ASSERT(!DataObjectReference::dataClass() || DataObjectReference::dataClass()->isDerivedFrom(DataObjectType::OOClass()));
    }

    /// \brief Conversion move constructor.
    TypedDataObjectReference(DataObjectReference&& other) : DataObjectReference(std::move(other)) {
        OVITO_ASSERT(!DataObjectReference::dataClass() || DataObjectReference::dataClass()->isDerivedFrom(DataObjectType::OOClass()));
    }

    /// \brief Constructs a reference to a data object.
    TypedDataObjectReference(const typename DataObjectType::OOMetaClass* dataClass, const QString& dataPath = QString(), const QString& dataTitle = QString()) : DataObjectReference(dataClass, dataPath, dataTitle) {}

    /// \brief Constructs a reference to a data object from an existing data path.
    TypedDataObjectReference(const ConstDataObjectPath& path) : DataObjectReference(path) {
        OVITO_ASSERT(!dataClass() || dataClass()->isDerivedFrom(DataObjectType::OOClass()));
    }

    /// Returns the DataObject subclass being referenced.
    const typename DataObjectType::OOMetaClass* dataClass() const {
        return static_cast<const typename DataObjectType::OOMetaClass*>(DataObjectReference::dataClass());
    }

    /// \brief Compares two references for equality.
    bool operator==(const TypedDataObjectReference& other) const { return DataObjectReference::operator==(other); }

    /// \brief Compares two references for inequality.
    bool operator!=(const TypedDataObjectReference& other) const { return DataObjectReference::operator!=(other); }

    /// \brief Strict ordering function.
    bool operator<(const TypedDataObjectReference& other) const { return DataObjectReference::operator<(other); }

    friend SaveStream& operator<<(SaveStream& stream, const TypedDataObjectReference& r) {
        return stream << static_cast<const DataObjectReference&>(r);
    }

    friend LoadStream& operator>>(LoadStream& stream, TypedDataObjectReference& r) {
        return stream >> static_cast<DataObjectReference&>(r);
    }

    friend QDebug operator<<(QDebug debug, const TypedDataObjectReference& r) {
        return debug << static_cast<const DataObjectReference&>(r);
    }
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::DataObjectReference);
