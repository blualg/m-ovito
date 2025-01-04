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

#include <ovito/particles/Particles.h>
#include "Dihedrals.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Dihedrals);
OVITO_CLASSINFO(Dihedrals, "DisplayName", "Dihedrals");
OVITO_CLASSINFO(Dihedrals, "ClassNameAlias", "DihedralsObject");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void Dihedrals::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr Dihedrals::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
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
        componentCount = 4;
        break;
    default:
        OVITO_ASSERT_MSG(false, "Dihedrals::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid dihedral standard property type: %1").arg(type));
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
void Dihedrals::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Dihedrals"));
    setElementDescriptionName(QStringLiteral("dihedrals"));
    setPythonName(QStringLiteral("dihedrals"));

    const QStringList emptyList;
    const QStringList abcdList = QStringList() << "A" << "B" << "C" << "D";

    registerStandardProperty(TypeProperty, tr("Dihedral Type"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Dihedral types"));
    registerStandardProperty(TopologyProperty, tr("Topology"), Property::Int64, abcdList);
}

}   // End of namespace
