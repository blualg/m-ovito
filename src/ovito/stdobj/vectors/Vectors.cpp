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

#include "./Vectors.h"
#include "./VectorVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Vectors);
OVITO_CLASSINFO(Vectors, "DisplayName", "Vectors");

/******************************************************************************
 * Registers all standard properties with the property traits class.
 ******************************************************************************/
void Vectors::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Vectors"));
    setPythonName(QStringLiteral("vectors"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";
    registerStandardProperty(PositionProperty, tr("Position"), Property::FloatDefault, xyzList);
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList);
    registerStandardProperty(TransparencyProperty, tr("Transparency"), Property::FloatGraphics, emptyList);
    registerStandardProperty(DirectionProperty, tr("Direction"), Property::FloatDefault, xyzList);
    registerStandardProperty(DirectionMagnitudeProperty, tr("Direction Magnitude"), Property::FloatDefault, emptyList);
}

/******************************************************************************
 * Creates a storage object for standard properties.
 ******************************************************************************/
PropertyPtr Vectors::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
                                                                 const ConstDataObjectPath& containerPath) const
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
        case TransparencyProperty:
            dataType = Property::FloatGraphics;
            componentCount = 1;
            break;
        case DirectionProperty:
            dataType = Property::FloatDefault;
            componentCount = 3;
            OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Vector3));
            break;
        case DirectionMagnitudeProperty:
            dataType = Property::FloatDefault;
            componentCount = 1;
            break;
        default:
            OVITO_ASSERT_MSG(false, "Vectors::createStandardProperty()", "Invalid standard property type");
            throw Exception(tr("This is not a valid standard property type: %1").arg(type));
    }

    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property =
        PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && !containerPath.empty()) {
        // Certain standard properties need to be initialized with default values determined by the attached visual element.
        if(type == ColorProperty) {
            if(const Vectors* vectors = dynamic_object_cast<Vectors>(containerPath.back())) {
                if(VectorVis* vectorsVis = dynamic_object_cast<VectorVis>(vectors->visElement())) {
                    property->fill<ColorG>(vectorsVis->arrowColor().toDataType<GraphicsFloatType>());
                    init = DataBuffer::Uninitialized;
                }
            }
        }
        else if(type == TransparencyProperty) {
            if(const Vectors* vectors = dynamic_object_cast<Vectors>(containerPath.back())) {
                if(VectorVis* vectorsVis = dynamic_object_cast<VectorVis>(vectors->visElement())) {
                    property->fill<GraphicsFloatType>(static_cast<GraphicsFloatType>(vectorsVis->transparency()));
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
Vectors::Vectors(ObjectInitializationFlags flags) : PropertyContainer(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject) || !flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
        // Create and attach a default visualization element for rendering the vectors.
        setVisElement(OORef<VectorVis>::create(flags));
    }
}

}  // namespace Ovito
