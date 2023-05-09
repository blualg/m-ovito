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
#include "TrajectoryObject.h"
#include "TrajectoryVis.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(TrajectoryObject);

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void TrajectoryObject::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Trajectories"));
    setElementDescriptionName(QStringLiteral("vertex"));
    setPythonName(QStringLiteral("trajectories"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";
    registerStandardProperty(ColorProperty, tr("Color"), PropertyObject::FloatGraphics, rgbList);
    registerStandardProperty(PositionProperty, tr("Position"), PropertyObject::FloatDefault, xyzList);
    registerStandardProperty(SampleTimeProperty, tr("Time"), PropertyObject::Int32, emptyList);
    registerStandardProperty(ParticleIdentifierProperty, tr("Particle Identifier"), PropertyObject::Int64, emptyList);
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr TrajectoryObject::OOMetaClass::createStandardPropertyInternal(size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case PositionProperty:
        dataType = PropertyObject::FloatDefault;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Point3));
        break;
    case ColorProperty:
        dataType = PropertyObject::FloatGraphics;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(ColorG));
        break;
    case SampleTimeProperty:
        dataType = PropertyObject::Int32;
        componentCount = 1;
        break;
    case ParticleIdentifierProperty:
        dataType = PropertyObject::Int64;
        componentCount = 1;
        break;
    default:
        OVITO_ASSERT_MSG(false, "TrajectoryObject::createStandardProperty()", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard property type: %1").arg(type));
    }

    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(elementCount, dataType, componentCount, propertyName, flags & ~DataBuffer::InitializeMemory, type, componentNames);

    // Initialize memory if requested.
    if(flags.testFlag(DataBuffer::InitializeMemory) && !containerPath.empty()) {
        // Certain standard properties need to be initialized with default values determined by the attached visual element.
        if(type == ColorProperty) {
            if(const TrajectoryObject* trajectory = dynamic_object_cast<TrajectoryObject>(containerPath.back())) {
                if(TrajectoryVis* trajectoryVis = dynamic_object_cast<TrajectoryVis>(trajectory->visElement())) {
                    property->fill<ColorG>(trajectoryVis->lineColor().toDataType<GraphicsFloatType>());
                    flags.setFlag(DataBuffer::InitializeMemory, false);
                }
            }
        }
    }

    if(flags.testFlag(DataBuffer::InitializeMemory)) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Default constructor.
******************************************************************************/
TrajectoryObject::TrajectoryObject(ObjectCreationParams params) : PropertyContainer(params)
{
    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());

    // Create and attach a default visualization element for rendering the trajectory lines.
    if(params.createVisElement())
        setVisElement(OORef<TrajectoryVis>::create(params));
}

}   // End of namespace
