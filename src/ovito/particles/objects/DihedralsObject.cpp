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

#include <ovito/particles/Particles.h>
#include "DihedralsObject.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(DihedralsObject);

/******************************************************************************
* Constructor.
******************************************************************************/
DihedralsObject::DihedralsObject(ObjectCreationParams params) : PropertyContainer(params)
{
    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr DihedralsObject::OOMetaClass::createStandardPropertyInternal(size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case TypeProperty:
        dataType = PropertyObject::Int32;
        componentCount = 1;
        break;
    case TopologyProperty:
        dataType = PropertyObject::Int64;
        componentCount = 4;
        break;
    default:
        OVITO_ASSERT_MSG(false, "DihedralsObject::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid dihedral standard property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(elementCount, dataType, componentCount, propertyName, flags & ~DataBuffer::InitializeMemory, type, componentNames);

    if(flags.testFlag(DataBuffer::InitializeMemory)) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void DihedralsObject::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Dihedrals"));
    setElementDescriptionName(QStringLiteral("dihedrals"));
    setPythonName(QStringLiteral("dihedrals"));

    const QStringList emptyList;
    const QStringList abcdList = QStringList() << "A" << "B" << "C" << "D";

    registerStandardProperty(TypeProperty, tr("Dihedral Type"), PropertyObject::Int32, emptyList, &ElementType::OOClass(), tr("Dihedral types"));
    registerStandardProperty(TopologyProperty, tr("Topology"), PropertyObject::Int64, abcdList);
}

}   // End of namespace
