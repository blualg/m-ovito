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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/utilities/io/NumberParsing.h>
#include "InputColumnMapping.h"

namespace Ovito::StdObj {

/******************************************************************************
 * Maps a file column to a standard property unless there is already another
 * column mapped to the same property.
 *****************************************************************************/
bool InputColumnMapping::mapStandardColumn(int column, int typeId, int vectorComponent)
{
    OVITO_ASSERT(column >= 0 && column < this->size());
    OVITO_ASSERT(typeId != PropertyObject::GenericUserProperty);
    OVITO_ASSERT(containerClass());

    // Check if there is another file column already mapped to the same target property.
    for(const InputColumnInfo& columnInfo : *this) {
        if(columnInfo.property.type() == typeId && columnInfo.property.vectorComponent() == vectorComponent)
            return false;
    }

    // If not, record the mapping.
    (*this)[column].mapStandardColumn(containerClass(), typeId, vectorComponent);
    return true;
}

/******************************************************************************
 * Maps this column to a user-defined property unless there is already another
 * column mapped to the same property.
 *****************************************************************************/
bool InputColumnMapping::mapCustomColumn(int column, const QString& propertyName, int dataType, int vectorComponent)
{
    OVITO_ASSERT(column >= 0 && column < this->size());
    OVITO_ASSERT(containerClass());

    // Check if there is another file column already mapped to the same target property.
    for(const InputColumnInfo& columnInfo : *this) {
        if(columnInfo.property.type() == PropertyObject::GenericUserProperty && columnInfo.property.name() == propertyName && columnInfo.property.vectorComponent() == vectorComponent)
            return false;
    }

    // If not, record the mapping.
    (*this)[column].mapCustomColumn(containerClass(), propertyName, dataType, vectorComponent);
    return true;
}

/******************************************************************************
 * Saves the mapping to the given stream.
 *****************************************************************************/
SaveStream& operator<<(SaveStream& stream, const InputColumnMapping& m)
{
    stream.beginChunk(0x02);
    stream << static_cast<const OvitoClassPtr&>(m.containerClass());
    stream.writeSizeT(m.size());
    for(const InputColumnInfo& col : m) {
        stream << col.property;
        stream << col.columnName;
        stream << col.dataType;
    }
    stream.endChunk();
    return stream;
}

/******************************************************************************
 * Loads the mapping from the given stream.
 *****************************************************************************/
LoadStream& operator>>(LoadStream& stream, InputColumnMapping& m)
{
    int version = stream.expectChunkRange(0x0, 0x02);

    // For backward compatibility with OVITO 3.1:
    if(version == 1) {
        int numColumns;
        stream >> numColumns;
        m.resize(numColumns);
        for(InputColumnInfo& col : m) {
            stream >> col.columnName;
            int propertyType;
            stream >> propertyType;
            QString propertyName;
            stream >> propertyName;
            stream >> col.dataType;
            int vectorComponent;
            stream >> vectorComponent;
            if(col.dataType != QMetaType::Void) {
                if(propertyType == PropertyObject::GenericUserProperty)
                    col.property = PropertyReference(m.containerClass(), propertyName, vectorComponent);
                else
                    col.property = PropertyReference(m.containerClass(), propertyType, vectorComponent);
            }
        }
    }
    else {
        OvitoClassPtr clazz;
        stream >> clazz;
        m._containerClass = static_cast<PropertyContainerClassPtr>(clazz);
        m.resize(stream.readSizeT());
        for(InputColumnInfo& col : m) {
            stream >> col.property;
            stream >> col.columnName;
            stream >> col.dataType;
        }
    }
    stream.closeChunk();
    return stream;
}

/******************************************************************************
 * Saves the mapping into a byte array.
 *****************************************************************************/
QByteArray InputColumnMapping::toByteArray() const
{
    QByteArray buffer;
    QDataStream dstream(&buffer, QIODevice::WriteOnly);
    SaveStream stream(dstream);
    stream << *this;
    stream.close();
    return buffer;
}

/******************************************************************************
 * Loads the mapping from a byte array.
 *****************************************************************************/
void InputColumnMapping::fromByteArray(const QByteArray& array)
{
    QDataStream dstream(array);
    LoadStream stream(dstream);
    stream >> *this;
    stream.close();
}

/******************************************************************************
 * Checks if the mapping is valid; throws an exception if not.
 *****************************************************************************/
void InputColumnMapping::validate() const
{
    OVITO_ASSERT(containerClass());

    // Let the property container class perform specific checks.
    containerClass()->validateInputColumnMapping(*this);

    // Check for conflicting mappings, i.e. several file columns being mapped to the same particle property.
    int numMapped = 0;
    for(auto m1 = begin(); m1 != end(); ++m1) {
        if(!m1->isMapped()) continue;
        numMapped++;
        OVITO_ASSERT(m1->property.containerClass() == containerClass());
        for(auto m2 = std::next(m1); m2 != end(); ++m2) {
            if(m1->property == m2->property)
                throw Exception(InputColumnReader::tr("Invalid file column mapping: File columns %1 and %2 cannot both be mapped to the same property '%3'.")
                    .arg(std::distance(begin(), m1) + 1)
                    .arg(std::distance(begin(), m2) + 1)
                    .arg(m1->property.nameWithComponent()));
        }
    }

    if(numMapped == 0)
        throw Exception(InputColumnReader::tr("File column mapping is empty. Please specify how data columns of the input file should be mapped to the properties of %1.").arg(containerClass()->elementDescriptionName()));
}

/******************************************************************************
 * Initializes the object.
 *****************************************************************************/
InputColumnReader::InputColumnReader(StandardFrameLoader& frameLoader, const InputColumnMapping& mapping, PropertyContainer* container, bool removeExistingProperties, bool validateMapping)
    : _frameLoader(frameLoader), _mapping(mapping), _container(container)
{
    if(validateMapping)
        mapping.validate();

    // Create target properties as defined by the mapping.
    for(int i = 0; i < (int)mapping.size(); i++) {

        const PropertyReference& pref = mapping[i].property;

        int vectorComponent = std::max(0, pref.vectorComponent());
        int dataType = mapping[i].dataType;

        TargetPropertyRecord rec;

        if(dataType != QMetaType::Void) {

            if(dataType != PropertyObject::Int32 && dataType != PropertyObject::Int64 && dataType != PropertyObject::Float32 && dataType != PropertyObject::Float64)
                throw Exception(tr("Invalid user-defined target property (data type %1) for input file column %2").arg(dataType).arg(i+1));

            PropertyObject* property;
            if(pref.type() != PropertyObject::GenericUserProperty) {
                // Create standard property.
                property = container->createProperty(DataBuffer::Initialized, pref.type());
                // File reader may want to override the property's name.
                property->setName(pref.name());
                // If this is a typed property, determine the kind of ElementType objects to create for it.
                rec.elementTypeClass = container->getOOMetaClass().typedPropertyElementClass(pref.type());
            }
            else {
                // Determine the number of vector components we need for this user-defined property.
                int componentCount = vectorComponent + 1;
                for(const InputColumnInfo& col : mapping)
                    if(col.property.type() == PropertyObject::GenericUserProperty && col.property.name() == pref.name())
                        componentCount = std::max(componentCount, col.property.vectorComponent() + 1);

                // Look for existing user-defined property with the same name.
                if(const PropertyObject* existingProperty = container->getProperty(pref.name())) {
                    // If the existing property is incompatible, remove it from the container and create a new one.
                    if(existingProperty->type() != pref.type() || existingProperty->componentCount() != componentCount || existingProperty->dataType() != dataType)
                        container->removeProperty(existingProperty);
                }

                // Create a new user-defined property for the column.
                property = container->createProperty(DataBuffer::Initialized, pref.name(), dataType, componentCount);
            }

            OVITO_ASSERT(vectorComponent < (int)property->componentCount());

            rec.property = property;
            rec.vectorComponent = vectorComponent;
            rec.count = rec.property->size();
            rec.numericElementTypes = true;
            rec.dataType = rec.property->dataType();
            rec.stride = rec.property->stride();

            // Create a property memory accessor, but only one per property if multiple columns are mapped to that property.
            auto sharedTargetProperty = std::find_if(_properties.begin(), _properties.end(), [&](const TargetPropertyRecord& other) { return other.property == property; });
            if(sharedTargetProperty == _properties.end()) {
                rec.propertyArray = BufferWriteAccess(rec.property);
                rec.data = reinterpret_cast<uint8_t*>(rec.propertyArray.data(rec.vectorComponent));
            }
            else {
                rec.data = reinterpret_cast<uint8_t*>(sharedTargetProperty->propertyArray.data(rec.vectorComponent));
            }
            OVITO_ASSERT(container->properties().contains(rec.property));
        }

        // Build list of target properties for fast look up during parsing.
        _properties.push_back(std::move(rec));
    }

    // Remove properties from the container which are not being parsed.
    if(removeExistingProperties) {
        for(int index = container->properties().size() - 1; index >= 0; index--) {
            const PropertyObject* property = container->properties()[index];
            if(std::none_of(_properties.cbegin(), _properties.cend(), [&](const TargetPropertyRecord& rec) { return rec.property == property; }))
                container->removeProperty(property);
        }
    }
}

/******************************************************************************
 * Tells the parser to read the names of element types from the given file column
 *****************************************************************************/
void InputColumnReader::readTypeNamesFromColumn(int nameColumn, int numericIdColumn)
{
    OVITO_ASSERT(nameColumn >= 0 && nameColumn < _properties.size());
    OVITO_ASSERT(numericIdColumn >= 0 && numericIdColumn < _properties.size());
    _properties[nameColumn].nameOfNumericTypeColumn = numericIdColumn;
    _readingTypeNamesFromSeparateColumns = true;
}

/******************************************************************************
 * Parses the string tokens from one line of the input file and stores the values
 * in the target properties.
 *****************************************************************************/
const char* InputColumnReader::readElement(size_t elementIndex, const char* s, const char* s_end)
{
    OVITO_ASSERT(_properties.size() == _mapping.size());
    OVITO_ASSERT(s <= s_end);

    int columnIndex = 0;
    while(columnIndex < _properties.size()) {
        // Skip initial whitespace.
        while(s != s_end && (*s == ' ' || *s == '\t' || *s == '\r'))
            ++s;
        if(s == s_end || *s == '\n') break;
        const char* token = s;
        // Go to end of token.
        while(s != s_end && (*s > ' ' || *s < 0))
            ++s;
        if(s != token) {
            parseField(elementIndex, columnIndex, token, s);
            columnIndex++;
        }
        if(s == s_end) break;
    }
    if(columnIndex < _properties.size())
        throw Exception(tr("Data line in input file does not contain enough columns. Expected %1 file columns, but found only %2.").arg(_properties.size()).arg(columnIndex));

    if(_readingTypeNamesFromSeparateColumns)
        assignTypeNamesFromSeparateColumns();

    // Skip to end of line.
    while(s != s_end && *s != '\n')
        ++s;
    if(s != s_end) ++s;
    return s;
}

/******************************************************************************
 * Parses the string tokens from one line of the input file and stores the values
 * in the target properties.
 *****************************************************************************/
void InputColumnReader::readElement(size_t elementIndex, const char* s)
{
    OVITO_ASSERT(_properties.size() == _mapping.size());

    int columnIndex = 0;
    while(columnIndex < _properties.size()) {
        while(*s == ' ' || *s == '\t')
            ++s;
        const char* token = s;
        while(*s > ' ' || *s < 0)
            ++s;
        if(s != token) {
            parseField(elementIndex, columnIndex, token, s);
            columnIndex++;
        }
        if(*s == '\0') break;
        s++;
    }
    if(columnIndex < _properties.size())
        throw Exception(tr("Data line in input file does not contain enough columns. Expected %1 file columns, but found only %2.").arg(_properties.size()).arg(columnIndex));

    if(_readingTypeNamesFromSeparateColumns)
        assignTypeNamesFromSeparateColumns();
}

/******************************************************************************
 * Assigns textual names, read from separate file columns, to numeric element types.
 *****************************************************************************/
void InputColumnReader::assignTypeNamesFromSeparateColumns()
{
    for(TargetPropertyRecord& record : _properties) {
        if(record.elementTypeClass && record.typeName.second != record.typeName.first) {
            if(const ElementType* type = record.property->elementType(record.lastTypeId)) {
                QLatin1String name(record.typeName.first, record.typeName.second);
                if(type->name() != name) {
                    ElementType* elementType = record.property->makeMutable(type);
                    elementType->setName(name);
                    // Load the color and radius presets for named particle types:
                    elementType->initializeType(PropertyReference(&_container->getOOMetaClass(), record.property));

                    // Log in type name assigned by the file reader as default value for the element type.
                    // This is needed for the Python code generator to detect manual changes subsequently made by the user.
                    elementType->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ElementType::name)});
                }
            }
        }
    }
}

/******************************************************************************
 * Parse a single field from a text line.
 *****************************************************************************/
void InputColumnReader::parseField(size_t elementIndex, int columnIndex, const char* token, const char* token_end)
{
    TargetPropertyRecord& prec = _properties[columnIndex];
    if(prec.nameOfNumericTypeColumn != -1) {
        _properties[prec.nameOfNumericTypeColumn].typeName = std::make_pair(token, token_end);
    }
    if(!prec.property || !prec.data) return;

    if(elementIndex >= prec.count)
        throw Exception(tr("Too many data lines in input file. Expected only %1 lines.").arg(prec.count));

    if(prec.dataType == PropertyObject::Float32) {
        if(!parseFloatType(token, token_end, *reinterpret_cast<float*>(prec.data + elementIndex * prec.stride)))
            throw Exception(tr("Invalid floating-point value in column %1 (%2): \"%3\"").arg(columnIndex+1).arg(prec.property->name()).arg(QString::fromLocal8Bit(token, token_end - token)));
    }
    else if(prec.dataType == PropertyObject::Float64) {
        if(!parseFloatType(token, token_end, *reinterpret_cast<double*>(prec.data + elementIndex * prec.stride)))
            throw Exception(tr("Invalid floating-point value in column %1 (%2): \"%3\"").arg(columnIndex+1).arg(prec.property->name()).arg(QString::fromLocal8Bit(token, token_end - token)));
    }
    else if(prec.dataType == PropertyObject::Int32) {
        int32_t& d = *reinterpret_cast<int32_t*>(prec.data + elementIndex * prec.stride);
        bool ok = parseInt(token, token_end, d);
        if(prec.elementTypeClass == nullptr) {
            if(!ok) {
                ok = parseBool(token, token_end, d);
                if(!ok)
                    throw Exception(tr("Invalid integer/bool value in column %1 (%2): \"%3\"").arg(columnIndex+1).arg(prec.property->name()).arg(QString::fromLocal8Bit(token, token_end - token)));
            }
        }
        else {
            // Automatically register a new element type if a new type identifier is encountered.
            if(ok) {
                // Instantiate a new element type with a numeric ID and add it to the property's type list.
                _frameLoader.addNumericType(_container->getOOMetaClass(), prec.property, d, QString{}, prec.elementTypeClass);
            }
            else {
                // Instantiate a new named element type and add it to the property's type list.
                d = _frameLoader.addNamedType(_container->getOOMetaClass(), prec.property, QLatin1String(token, token_end), prec.elementTypeClass)->numericId();
                prec.numericElementTypes = false;
            }
            prec.lastTypeId = d;
        }
    }
    else if(prec.dataType == PropertyObject::Int64) {
        int64_t& d = *reinterpret_cast<int64_t*>(prec.data + elementIndex * prec.stride);
        if(!parseInt64(token, token_end, d))
            throw Exception(tr("Invalid 64-bit integer value in column %1 (%2): \"%3\"").arg(columnIndex+1).arg(prec.property->name()).arg(QString::fromLocal8Bit(token, token_end - token)));
    }
}

/******************************************************************************
 * Processes the values from one line of the input file and stores them
 * in the target properties.
 *****************************************************************************/
void InputColumnReader::readElement(size_t elementIndex, const double* values, int nvalues)
{
    OVITO_ASSERT(_properties.size() == _mapping.size());
    if(nvalues < _properties.size())
        throw Exception(tr("Data record in input file does not contain enough columns. Expected %1 file columns, but found only %2.").arg(_properties.size()).arg(nvalues));

    auto prec = _properties.cbegin();
    const double* token = values;

    for(int columnIndex = 0; prec != _properties.cend(); ++columnIndex, ++token, ++prec) {
        if(!prec->property) continue;

        if(elementIndex >= prec->count)
            throw Exception(tr("Too many data values in input file. Expected only %1 values.").arg(prec->count));

        if(prec->data) {
            if(prec->dataType == PropertyObject::Float32) {
                *reinterpret_cast<float*>(prec->data + elementIndex * prec->stride) = static_cast<float>(*token);
            }
            else if(prec->dataType == PropertyObject::Float64) {
                *reinterpret_cast<double*>(prec->data + elementIndex * prec->stride) = *token;
            }
            else if(prec->dataType == PropertyObject::Int32) {
                int32_t ival = static_cast<int32_t>(*token);
                if(prec->elementTypeClass) {
                    // Instantiate a new element type with a numeric ID and add it to the property's type list.
                    _frameLoader.addNumericType(_container->getOOMetaClass(), prec->property, ival, QString{}, prec->elementTypeClass);
                }
                *reinterpret_cast<int32_t*>(prec->data + elementIndex * prec->stride) = ival;
            }
            else if(prec->dataType == PropertyObject::Int64) {
                *reinterpret_cast<int64_t*>(prec->data + elementIndex * prec->stride) = static_cast<int64_t>(*token);
            }
        }
    }
}

/******************************************************************************
 * Sorts the created element types either by numeric ID or by name,
 * depending on how they were stored in the input file.
 *****************************************************************************/
void InputColumnReader::sortElementTypes()
{
    for(const TargetPropertyRecord& p : _properties) {
        if(p.elementTypeClass && p.property) {
            // Since we created element types on the go while reading the elements, the ordering of the type list
            // depends on the storage order of data elements in the file. We rather want a well-defined type ordering, that's
            // why we sort them here according to their names or numeric IDs.
            if(p.numericElementTypes)
                p.property->sortElementTypesById();
            else
                p.property->sortElementTypesByName();
        }
    }
}

}   // End of namespace
