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
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include "Property.h"
#include "PropertyReference.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Constructs a reference to a standard property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, int typeId, int vectorComponentIndex) :
    _containerClass(pclass),
    _typeId(typeId),
    _name(pclass->standardPropertyName(typeId)),
    _vectorComponentIndex(vectorComponentIndex)
{
}

/******************************************************************************
* Constructs a reference to a user-defined property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, const QString& name, int vectorComponentIndex) :
    _containerClass(pclass),
    _name(name),
    _vectorComponentIndex(vectorComponentIndex)
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!_name.isEmpty());
}

/******************************************************************************
* Constructs a reference to a named vector component of a user-defined property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, const QString& name, const QString& vectorComponentName) :
    _containerClass(pclass),
    _name(name),
    _vectorComponentName(vectorComponentName)
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!_name.isEmpty());
}

/******************************************************************************
* Constructs a reference based on an existing Property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, const Property* property, int vectorComponentIndex) :
    _containerClass(pclass),
    _typeId(property->typeId()),
    _name(property->name()),
    _vectorComponentIndex(vectorComponentIndex)
{
    if(!isStandardProperty() && vectorComponentIndex >= 0 && vectorComponentIndex < property->componentNames().size())
        _vectorComponentName = property->componentNames()[vectorComponentIndex];
}

/******************************************************************************
* Returns the display name of the referenced property including the
* optional vector component.
******************************************************************************/
QString PropertyReference::nameWithComponent() const
{
    if(isStandardProperty()) {
        if(vectorComponentIndex() < 0 || containerClass()->standardPropertyComponentCount(typeId()) <= 1) {
            return name();
        }
        else {
            const QStringList& names = containerClass()->standardPropertyComponentNames(typeId());
            if(vectorComponentIndex() < names.size())
                return QStringLiteral("%1.%2").arg(name()).arg(names[vectorComponentIndex()]);
        }
    }
    if(!vectorComponentName().isEmpty())
        return QStringLiteral("%1.%2").arg(name()).arg(vectorComponentName());
    else if(vectorComponentIndex() < 0)
        return name();
    else
        return QStringLiteral("%1.%2").arg(name()).arg(vectorComponentIndex() + 1);
}

/******************************************************************************
* Strict ordering function.
******************************************************************************/
bool PropertyReference::operator<(const PropertyReference& other) const
{
    if(containerClass() == other.containerClass())
        return nameWithComponent() < other.nameWithComponent();
    else
        return containerClass() < other.containerClass();
}

/******************************************************************************
* Returns a new property reference that uses the same name as the current one,
* but with a different property container class.
******************************************************************************/
PropertyReference PropertyReference::convertToContainerClass(PropertyContainerClassPtr containerClass) const
{
    if(containerClass) {
        PropertyReference newref = *this;
        if(containerClass != this->containerClass()) {
            newref._containerClass = containerClass;

            // Split string into property name and vector component name.
            QStringList parts = this->name().split(QChar('.'));
            if((parts.length() == 1 || parts.length() == 2) && !parts[0].isEmpty()) {
                // Determine property type.
                QString name = parts[0];
                newref._typeId = containerClass->standardPropertyIds().value(name, 0);
                if(newref.isStandardProperty())
                    newref._name = name;

                // Determine vector component.
                if(parts.length() == 2 && newref._vectorComponentIndex == -1) {
                    // First try to convert component to integer.
                    bool ok;
                    newref._vectorComponentIndex = parts[1].toInt(&ok) - 1;
                    if(!ok) {
                        if(newref.typeId() != 0) {
                            // Perhaps the standard property's component name was used instead of an integer.
                            const QString componentName = parts[1].toUpper();
                            QStringList standardNames = containerClass->standardPropertyComponentNames(newref.typeId());
                            newref._vectorComponentIndex = standardNames.indexOf(componentName);
                        }
                    }
                }
            }
        }
        return newref;
    }
    else {
        return {};
    }
}

/******************************************************************************
* Writes a PropertyReference to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const PropertyReference& r)
{
    stream.beginChunk(0x03);
    stream << static_cast<const OvitoClassPtr&>(r.containerClass());
    stream << r.typeId();
    stream << r.name();
    stream << r.vectorComponentIndex();
    stream << r.vectorComponentName();
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a PropertyReference from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, PropertyReference& r)
{
    int version = stream.expectChunkRange(0x02, 1);
    OvitoClassPtr clazz;
    stream >> clazz;
    r._containerClass = static_cast<PropertyContainerClassPtr>(clazz);
    stream >> r._typeId;
    stream >> r._name;
    stream >> r._vectorComponentIndex;
    if(version >= 1)
        stream >> r._vectorComponentName;
    if(!r._containerClass)
        r = PropertyReference();
    else {
        // For backward compatibility with older OVITO versions:
        // If the reference is to a standard property type that has been deprecated,
        // we should turn the reference into a user-property reference.
        if(r._typeId != 0 && !r._containerClass->isValidStandardPropertyId(r._typeId))
            r._typeId = 0;
    }
    stream.closeChunk();
    return stream;
}

/******************************************************************************
* Outputs a PropertyReference to a debug stream.
******************************************************************************/
QDebug operator<<(QDebug debug, const PropertyReference& r)
{
    if(!r.isNull()) {
        debug.nospace() << "PropertyReference("
            << r.containerClass()->name()
            << ", "
            << r.name()
            << ", "
            << r.vectorComponentIndex()
            << ", "
            << r.vectorComponentName() << ")";
    }
    else {
        debug << "PropertyReference(<null>)";
    }
    return debug;
}

/******************************************************************************
* Finds the referenced property in the given property container object.
******************************************************************************/
const Property* PropertyReference::findInContainer(const PropertyContainer* container) const
{
    if(isNull())
        return nullptr;

    OVITO_ASSERT(container != nullptr);
    OVITO_ASSERT(containerClass()->isMember(container));

    if(isStandardProperty())
        return container->getProperty(typeId());
    else
        return container->getProperty(name());
}

/******************************************************************************
* Finds the referenced property in the given property container object and
* resolves the referenced vector component (if any). If resolution fails,
* an error message is returned in the errorDescription parameter.
******************************************************************************/
std::pair<const Property*, int> PropertyReference::findInContainerWithComponent(const PropertyContainer* container, QString& errorDescription) const
{
    const Property* property = findInContainer(container);

    if(!property) {
        errorDescription = Property::tr("The property with the name '%1' does not exist or was not computed by the pipeline.").arg(name());
        return {nullptr, -1};
    }

    int resolvedVectorComponent = 0;
    if(!vectorComponentName().isEmpty()) {
        // Try to resolve named vector component into a numeric component index.
        if(!property->componentNames().empty()) {
            resolvedVectorComponent = property->componentNames().indexOf(vectorComponentName());
            if(resolvedVectorComponent < 0) {
                errorDescription = Property::tr("The selected vector property component '%1' is invalid. Property '%2' has the following named components: %3")
                    .arg(vectorComponentName())
                    .arg(property->name())
                    .arg(property->componentNames().join(QStringLiteral(", ")));
                return {nullptr, -1};
            }
        }
        else {
            bool ok;
            resolvedVectorComponent = vectorComponentName().toInt(&ok) - 1;
            if(ok) {
                if(resolvedVectorComponent < 0 || resolvedVectorComponent >= property->componentCount()) {
                    errorDescription = Property::tr("The selected vector property component '%1' is out of range. Property '%2' has %3 component(s).")
                        .arg(vectorComponentName())
                        .arg(property->name())
                        .arg(property->componentCount());
                    return {nullptr, -1};
                }
            }
            else {
                errorDescription = Property::tr("The selected vector property component '%1' cannot be resolved, because property '%2' does not have named components.")
                    .arg(vectorComponentName())
                    .arg(property->name());
                return {nullptr, -1};
            }
        }
    }
    else {
        resolvedVectorComponent = vectorComponentIndex();
    }

    if(resolvedVectorComponent >= (int)property->componentCount()) {
        errorDescription = Property::tr("The selected vector property component is out of range. The property '%1' has only %2 values per data element.")
                                        .arg(property->name())
                                        .arg(resolvedVectorComponent);
        return {nullptr, -1};
    }

    return { property, std::max(0, resolvedVectorComponent) };
}

}   // End of namespace
