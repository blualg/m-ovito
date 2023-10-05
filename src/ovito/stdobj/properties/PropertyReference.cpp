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
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include "Property.h"
#include "PropertyReference.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Constructs a reference to a standard property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, int typeId, int vectorComponent) :
    _containerClass(pclass), _type(typeId),
    _name(pclass->standardPropertyName(typeId)), _vectorComponent(vectorComponent)
{
}

/******************************************************************************
* Constructs a reference based on an existing Property.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, const Property* property, int vectorComponent) :
    _containerClass(pclass),
    _type(property->type()), _name(property->name()), _vectorComponent(vectorComponent)
{
}

/******************************************************************************
* Returns the display name of the referenced property including the
* optional vector component.
******************************************************************************/
QString PropertyReference::nameWithComponent() const
{
    if(type() != 0) {
        if(vectorComponent() < 0 || containerClass()->standardPropertyComponentCount(type()) <= 1) {
            return name();
        }
        else {
            const QStringList& names = containerClass()->standardPropertyComponentNames(type());
            if(vectorComponent() < names.size())
                return QStringLiteral("%1.%2").arg(name()).arg(names[vectorComponent()]);
        }
    }
    if(vectorComponent() < 0)
        return name();
    else
        return QStringLiteral("%1.%2").arg(name()).arg(vectorComponent() + 1);
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
                newref._type = containerClass->standardPropertyIds().value(name, 0);
                if(newref._type != 0)
                    newref._name = name;

                // Determine vector component.
                if(parts.length() == 2 && newref._vectorComponent == -1) {
                    // First try to convert component to integer.
                    bool ok;
                    newref._vectorComponent = parts[1].toInt(&ok) - 1;
                    if(!ok) {
                        if(newref._type != 0) {
                            // Perhaps the standard property's component name was used instead of an integer.
                            const QString componentName = parts[1].toUpper();
                            QStringList standardNames = containerClass->standardPropertyComponentNames(newref._type);
                            newref._vectorComponent = standardNames.indexOf(componentName);
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
    stream.beginChunk(0x02);
    stream << static_cast<const OvitoClassPtr&>(r.containerClass());
    stream << r.type();
    stream << r.name();
    stream << r.vectorComponent();
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a PropertyReference from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, PropertyReference& r)
{
    stream.expectChunk(0x02);
    OvitoClassPtr clazz;
    stream >> clazz;
    r._containerClass = static_cast<PropertyContainerClassPtr>(clazz);
    stream >> r._type;
    stream >> r._name;
    stream >> r._vectorComponent;
    if(!r._containerClass)
        r = PropertyReference();
    else {
        // For backward compatibility with older OVITO versions:
        // If the reference is to a standard property type that has been deprecated,
        // we should turn the reference into a user-property reference.
        if(r._type != 0 && !r._containerClass->isValidStandardPropertyId(r._type))
            r._type = 0;
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
            << r.vectorComponent() << ")";
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

    if(type() != 0)
        return container->getProperty(type());
    else
        return container->getProperty(name());
}

}   // End of namespace
