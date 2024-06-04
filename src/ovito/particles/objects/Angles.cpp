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

#include <ovito/particles/Particles.h>
#include "Angles.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Angles);
OVITO_CLASSINFO(Angles, "DisplayName", "Angles");
OVITO_CLASSINFO(Angles, "ClassNameAlias", "AnglesObject");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void Angles::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr Angles::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case TypeProperty:
        dataType = Property::Int32;
        componentCount = 1;
        break;
    case TopologyProperty:
        dataType = Property::Int64;
        componentCount = 3;
        break;
    default:
        OVITO_ASSERT_MSG(false, "Angles::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard angle property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void Angles::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Angles"));
    setElementDescriptionName(QStringLiteral("angles"));
    setPythonName(QStringLiteral("angles"));

    const QStringList emptyList;
    const QStringList abcList = QStringList() << "A" << "B" << "C";

    registerStandardProperty(TypeProperty, tr("Angle Type"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Angle types"));
    registerStandardProperty(TopologyProperty, tr("Topology"), Property::Int64, abcList);
}

}   // End of namespace
