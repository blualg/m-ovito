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

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/io/CompressedTextWriter.h>

namespace Ovito::StdObj {

/**
 * \brief This class lists the properties to be written to an output file as data columns.
 *
 * This is simply a vector of PropertyReference instances. Each reference represents one column in the output file.
 */
class OVITO_STDOBJ_EXPORT OutputColumnMapping : public std::vector<PropertyReference>
{
public:

    using std::vector<PropertyReference>::size_type;

    /// Inherit constructors from std::vector.
    using std::vector<PropertyReference>::vector;

    /// \brief Saves the mapping to the given stream.
    void saveToStream(SaveStream& stream) const;

    /// \brief Loads the mapping from the given stream.
    void loadFromStream(LoadStream& stream);

    /// \brief Converts the mapping data into a byte array.
    QByteArray toByteArray() const;

    /// \brief Loads the mapping from a byte array.
    void fromByteArray(const QByteArray& array);
};

template<class PropertyContainerType>
class TypedOutputColumnMapping : public std::vector<TypedPropertyReference<PropertyContainerType>>
{
public:

    /// Inherit constructors from std::vector.
    using std::vector<TypedPropertyReference<PropertyContainerType>>::vector;

    /// Transparent conversion to an untyped OutputColumnMapping.
    operator OutputColumnMapping&() { return *reinterpret_cast<OutputColumnMapping*>(this); }

    /// Transparent conversion to an untyped OutputColumnMapping.
    operator const OutputColumnMapping&() const { return *reinterpret_cast<const OutputColumnMapping*>(this); }

    /// \brief Saves the mapping to the given stream.
    void saveToStream(SaveStream& stream) const { static_cast<const OutputColumnMapping&>(*this).saveToStream(stream); }

    /// \brief Loads the mapping from the given stream.
    void loadFromStream(LoadStream& stream) { static_cast<OutputColumnMapping&>(*this).loadFromStream(stream); }

    /// \brief Converts the mapping data into a byte array.
    QByteArray toByteArray() const { return static_cast<const OutputColumnMapping&>(*this).toByteArray(); }

    /// \brief Loads the mapping from a byte array.
    void fromByteArray(const QByteArray& array) { static_cast<OutputColumnMapping&>(*this).fromByteArray(array); }
};

/**
 * \brief Writes the data columns to the output file as specified by an OutputColumnMapping.
 */
class OVITO_STDOBJ_EXPORT PropertyOutputWriter : public QObject
{
public:

    /// These modes control how the values of typed properties are
    /// written to the output file.
    enum TypedPropertyMode {
        WriteNumericIds,        ///< Write the integer numeric ID of the type.
        WriteNamesUnmodified,   ///< Write the type name as a string.
        WriteNamesUnderscore,   ///< Write the type name as a string, with whitespace replaced with underscores.
        WriteNamesInQuotes      ///< Write the type name as a string, in quotes if the name contains whitespace.
    };

    /// \brief Initializes the helper object.
    /// \param mapping The mapping between the properties and the columns in the output file.
    /// \param sourceContainer The data source container for the properties.
    /// \throws Exception if the mapping is not valid.
    ///
    /// This constructor checks that all necessary properties referenced in the OutputColumnMapping
    /// are present in the source property container.
    PropertyOutputWriter(const OutputColumnMapping& mapping, const PropertyContainer* sourceContainer, TypedPropertyMode typedPropertyMode);

    /// \brief Writes the output line for a single data element to the output stream.
    /// \param index The index of the data element to write (starting at 0).
    /// \param stream An output text stream.
    void writeElement(size_t index, CompressedTextWriter& stream);

    /// Returns the number of output columns that will be written.
    size_t columnCount() const { return _properties.size(); }

    /// Returns the property that will be written to the i-th file column.
    const PropertyObject* property(size_t columnIndex) const {
        OVITO_ASSERT(columnIndex < _properties.size());
        return _properties[columnIndex];
    }

    /// Returns the property component that will be written to the i-th file column.
    int vectorComponent(size_t columnIndex) const {
        OVITO_ASSERT(columnIndex < _vectorComponents.size());
        return _vectorComponents[columnIndex];
    }

    /// Returns a PropertyReference for the i-th output column.
    PropertyReference propertyRef(size_t columnIndex) const {
        OVITO_ASSERT(columnIndex < _properties.size());
        OVITO_ASSERT(columnIndex < _vectorComponents.size());
        if(_properties[columnIndex])
            return PropertyReference(&_sourceContainer->getOOMetaClass(), _properties[columnIndex], _vectorComponents[columnIndex]);
        else
            return PropertyReference(&_sourceContainer->getOOMetaClass(), PropertyObject::GenericIdentifierProperty, 0);
    }

    // Determines whether the i-th column contains a vector property component.
    bool isVectorProperty(size_t columnIndex) const {
        if(!property(columnIndex))
            return false;
        return property(columnIndex)->componentCount() > 1 || !property(columnIndex)->componentNames().empty();
    }

private:

    /// The property container;
    const PropertyContainer* _sourceContainer;

    /// Stores the source properties for each column in the output file.
    /// A nullptr instead of a PropertyObject means that the implicit element indices should be output in this file column.
    std::vector<const PropertyObject*> _properties;

    /// Stores the source vector component for each output column.
    std::vector<int> _vectorComponents;

    /// Stores the data accessor for each output property.
    std::vector<RawBufferReadAccess> _accessors;

    /// The names corresponding to numeric types of each typed property.
    std::vector<std::map<int, QString>> _cachedTypeNames;

    /// Controls how type names are output.
    TypedPropertyMode _typedPropertyMode;
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::StdObj::OutputColumnMapping);
