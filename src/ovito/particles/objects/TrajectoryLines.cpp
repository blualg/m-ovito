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
#include "TrajectoryLines.h"
#include "TrajectoryVis.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(TrajectoryLines);

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void TrajectoryLines::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Trajectories"));
    setElementDescriptionName(QStringLiteral("vertex"));
    setPythonName(QStringLiteral("trajectories"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList);
    registerStandardProperty(PositionProperty, tr("Position"), Property::FloatDefault, xyzList);
    registerStandardProperty(SampleTimeProperty, tr("Time"), Property::Int32, emptyList);
    registerStandardProperty(ParticleIdentifierProperty, tr("Particle Identifier"), Property::Int64, emptyList);
}

/******************************************************************************
* Creates a storage object for standard properties.
******************************************************************************/
PropertyPtr TrajectoryLines::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case PositionProperty:
        dataType = Property::FloatDefault;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Point3));
        break;
    case ColorProperty:
        dataType = Property::FloatGraphics;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(ColorG));
        break;
    case SampleTimeProperty:
        dataType = Property::Int32;
        componentCount = 1;
        break;
    case ParticleIdentifierProperty:
        dataType = Property::Int64;
        componentCount = 1;
        break;
    default:
        OVITO_ASSERT_MSG(false, "TrajectoryLines::createStandardProperty()", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard property type: %1").arg(type));
    }

    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && !containerPath.empty()) {
        // Certain standard properties need to be initialized with default values determined by the attached visual element.
        if(type == ColorProperty) {
            if(const TrajectoryLines* trajectory = dynamic_object_cast<TrajectoryLines>(containerPath.back())) {
                if(TrajectoryVis* trajectoryVis = dynamic_object_cast<TrajectoryVis>(trajectory->visElement())) {
                    property->fill<ColorG>(trajectoryVis->lineColor().toDataType<GraphicsFloatType>());
                    init = DataBuffer::Uninitialized;
                }
            }
        }
    }

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Constructor.
******************************************************************************/
TrajectoryLines::TrajectoryLines(ObjectInitializationFlags flags) : PropertyContainer(flags)
{
    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Create and attach a default visualization element for rendering the trajectory lines.
            setVisElement(OORef<TrajectoryVis>::create(flags));
        }
    }
}

}   // End of namespace
