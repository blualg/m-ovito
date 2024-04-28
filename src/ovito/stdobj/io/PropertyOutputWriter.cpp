////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include "PropertyOutputWriter.h"

namespace Ovito {

/******************************************************************************
 * Saves the mapping to the given stream.
 *****************************************************************************/
void OutputColumnMapping::saveToStream(SaveStream& stream) const
{
    stream.beginChunk(0x01);
    stream << (int)size();
    for(const PropertyReference& col : *this) {
        stream << col;
    }
    stream.endChunk();
}

/******************************************************************************
 * Loads the mapping from the given stream.
 *****************************************************************************/
void OutputColumnMapping::loadFromStream(LoadStream& stream)
{
    stream.expectChunk(0x01);
    int numColumns;
    stream >> numColumns;
    resize(numColumns);
    for(PropertyReference& col : *this) {
        stream >> col;
    }
    stream.closeChunk();
}

/******************************************************************************
 * Saves the mapping into a byte array.
 *****************************************************************************/
QByteArray OutputColumnMapping::toByteArray() const
{
    QByteArray buffer;
    QDataStream dstream(&buffer, QIODevice::WriteOnly);
    SaveStream stream(dstream);
    saveToStream(stream);
    stream.close();
    return buffer;
}

/******************************************************************************
 * Loads the mapping from a byte array.
 *****************************************************************************/
void OutputColumnMapping::fromByteArray(const QByteArray& array)
{
    QDataStream dstream(array);
    LoadStream stream(dstream);
    loadFromStream(stream);
    stream.close();
}

/******************************************************************************
 * Initializes the writer object.
 *****************************************************************************/
PropertyOutputWriter::PropertyOutputWriter(const OutputColumnMapping& mapping, const PropertyContainer* sourceContainer, TypedPropertyMode typedPropertyMode)
    : _sourceContainer(sourceContainer), _typedPropertyMode(typedPropertyMode)
{
    // Gather the source properties.
    for(int i = 0; i < (int)mapping.size(); i++) {
        const PropertyReference& pref = mapping[i];
        const Property* property = pref.findInContainer(sourceContainer);
        int resolvedVectorComponent = 0;
        if(property == nullptr && pref.typeId() != Property::GenericIdentifierProperty) {
            QStringList propertyList;
            for(const Property* p : sourceContainer->properties())
                propertyList.push_back(p->name());
            throw Exception(tr("Invalid output file column %1 specification: "
                               "Property '%2' does not exist or has not been computed by the pipeline. "
                               "The following properties are found in the %3 container: %4")
                    .arg(i+1)
                    .arg(pref.name())
                    .arg(sourceContainer->getOOMetaClass().propertyClassDisplayName())
                    .arg(propertyList.join(QStringLiteral(", "))));
        }
        if(property) {
            resolvedVectorComponent = pref.vectorComponentIndex();
            if(resolvedVectorComponent < 0) {
                if(!pref.vectorComponentName().isEmpty()) {
                    // Try to resolve named vector component to a component index.
                    if(!property->componentNames().empty()) {
                        resolvedVectorComponent = property->componentNames().indexOf(pref.vectorComponentName());
                        if(resolvedVectorComponent < 0)
                            throw Exception(tr("The property vector component '%1' specified for output file column %2 is invalid. Property '%3' has the following named components: %4")
                                .arg(pref.vectorComponentName())
                                .arg(i+1)
                                .arg(property->name())
                                .arg(property->componentNames().join(QStringLiteral(", "))));
                    }
                    else {
                        bool ok;
                        resolvedVectorComponent = pref.vectorComponentName().toInt(&ok) - 1;
                        if(ok) {
                            if(resolvedVectorComponent < 0 || resolvedVectorComponent >= property->componentCount())
                                throw Exception(tr("The property vector component '%1' specified for output file column %2 is out of range. Property '%3' has %4 component(s).")
                                    .arg(pref.vectorComponentName())
                                    .arg(i+1)
                                    .arg(property->name())
                                    .arg(property->componentCount()));
                        }
                        else {
                            throw Exception(tr("The property vector component '%1' specified for output file column %2 cannot be resolved because property '%3' does not have named components.")
                                .arg(pref.vectorComponentName())
                                .arg(i+1)
                                .arg(property->name()));
                        }
                    }
                }

                if(resolvedVectorComponent < 0) {
                    // If the user did not specify a specific component for a vector property, generate multiple columns to export all property components.
                    if(property->componentCount() > 1) {
                        for(int component = 0; component < property->componentCount(); component++) {
                            _properties.push_back(property);
                            _vectorComponents.push_back(component);
                            _accessors.emplace_back(property);
                        }
                        continue;
                    }
                    else {
                        resolvedVectorComponent = 0;
                    }
                }
            }

            // Error if user specified a vector component that is out of range.
            if((int)property->componentCount() <= resolvedVectorComponent)
                throw Exception(tr("The output vector component selected for column %1 is out of range. The property '%2' has only %3 component(s).").arg(i+1).arg(property->name()).arg(property->componentCount()));
        }

        // Build internal list of property objects for fast look up during writing.
        _properties.push_back(property);
        _vectorComponents.push_back(resolvedVectorComponent);
        _accessors.emplace_back(property);
    }
    _cachedTypeNames.resize(_properties.size());
}

/******************************************************************************
 * Writes the data record for a single data element to the output stream.
 *****************************************************************************/
void PropertyOutputWriter::writeElement(size_t index, CompressedTextWriter& stream)
{
    auto property = _properties.cbegin();
    auto vcomp = _vectorComponents.cbegin();
    auto accessor = _accessors.cbegin();
    auto typeNameCache = _cachedTypeNames.begin();
    for(; property != _properties.cend(); ++property, ++vcomp, ++accessor, ++typeNameCache) {
        if(property != _properties.cbegin())
            stream << ' ';
        if(*property) {
            const int dataType = (*property)->dataType();
            if(dataType == Property::Float32) {
                stream << *reinterpret_cast<const float*>(accessor->cdata(index, *vcomp));
            }
            else if(dataType == Property::Float64) {
                stream << *reinterpret_cast<const double*>(accessor->cdata(index, *vcomp));
            }
            else if(dataType == Property::Int32) {
                if(_typedPropertyMode == WriteNumericIds || (*property)->elementTypes().empty()) {
                    stream << *reinterpret_cast<const int32_t*>(accessor->cdata(index, *vcomp));
                }
                else {
                    // Write type name instead of type number.
                    int32_t numericTypeId = *reinterpret_cast<const int32_t*>(accessor->cdata(index, *vcomp));
                    auto entry = typeNameCache->find(numericTypeId);
                    if(entry == typeNameCache->end()) {
                        const ElementType* type = (*property)->elementType(numericTypeId);
                        if(type && !type->name().isEmpty()) {
                            if(_typedPropertyMode == WriteNamesUnmodified) {
                                entry = typeNameCache->emplace(numericTypeId, type->name()).first;
                            }
                            else if(_typedPropertyMode == WriteNamesUnderscore) {
                                // Replace spaces in the name with underscores.
                                QString s = type->name();
                                entry = typeNameCache->emplace(numericTypeId, s.replace(QChar(' '), QChar('_'))).first;
                            }
                            else if(_typedPropertyMode == WriteNamesInQuotes) {
                                // Surround name with quotes if necessary.
                                if(type->name().contains(QChar(' ')))
                                    entry = typeNameCache->emplace(numericTypeId, QChar('"') + type->name() + QChar('"')).first;
                                else
                                    entry = typeNameCache->emplace(numericTypeId, type->name()).first;
                            }
                        }
                        else {
                            entry = typeNameCache->emplace(numericTypeId, QString::number(numericTypeId)).first;
                        }
                    }
                    stream << entry->second;
                }
            }
            else if(dataType == Property::Int64) {
                stream << static_cast<qint64>(*reinterpret_cast<const int64_t*>(accessor->cdata(index, *vcomp)));
            }
            else if(dataType == Property::Int8) {
                stream << static_cast<qint32>(*reinterpret_cast<const int8_t*>(accessor->cdata(index, *vcomp)));
            }
            else {
                throw Exception(tr("The property '%1' cannot be written to the output file, because it has a non-standard data type.").arg((*property)->name()));
            }
        }
        else {
            // Write (implicit) element index:
            stream << static_cast<quint64>(index + 1);
        }
    }
    stream << '\n';
}

}   // End of namespace
