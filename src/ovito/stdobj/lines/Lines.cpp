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
#include <ovito/stdobj/lines/LinesVis.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "Lines.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Lines);
DEFINE_PROPERTY_FIELD(Lines, cuttingPlanes);
SET_PROPERTY_FIELD_LABEL(Lines, cuttingPlanes, "Cutting planes");

/******************************************************************************
 * Registers all standard properties with the property traits class.
 ******************************************************************************/
void Lines::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Lines"));
    setElementDescriptionName(QStringLiteral("vertex"));
    setPythonName(QStringLiteral("lines"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X"
                                              << "Y"
                                              << "Z";
    const QStringList rgbList = QStringList() << "R"
                                              << "G"
                                              << "B";
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList);
    registerStandardProperty(PositionProperty, tr("Position"), Property::FloatDefault, xyzList);
    registerStandardProperty(SampleTimeProperty, tr("Time"), Property::Int32, emptyList);
    registerStandardProperty(SectionProperty, tr("Section"), Property::Int64, emptyList);
}

/******************************************************************************
 * Creates a storage object for standard properties.
 ******************************************************************************/
PropertyPtr Lines::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
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
        case SampleTimeProperty:
            dataType = Property::Int32;
            componentCount = 1;
            break;
        case SectionProperty:
            dataType = Property::Int64;
            componentCount = 1;
            break;
        default:
            OVITO_ASSERT_MSG(false, "Lines::createStandardProperty()", "Invalid standard property type");
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
            if(const Lines* lines = dynamic_object_cast<Lines>(containerPath.back())) {
                if(LinesVis* linesVis = dynamic_object_cast<LinesVis>(lines->visElement())) {
                    property->fill<ColorG>(linesVis->lineColor().toDataType<GraphicsFloatType>());
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
Lines::Lines(ObjectInitializationFlags flags) : PropertyContainer(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Create and attach a default visualization element for rendering the lines.
            setVisElement(OORef<LinesVis>::create(flags));
        }
    }
}

/******************************************************************************
 * Returns the base point and vector information for visualizing a vector
 * property from this container using a VectorVis element.
 ******************************************************************************/
std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> Lines::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                                           MixedKeyCache& visCache) const
{
    // Get lines object
    if(const Lines* lines = path.lastAs<Lines>(1)) {
        OVITO_ASSERT(path.lastAs<Lines>(1) == this);

        if(const LinesVis* linesVis = dynamic_object_cast<LinesVis>(lines->visElement())) {
            // Get the simulation cell.
            const SimulationCell* simulationCell = linesVis->wrappedLines() ? state.getObject<SimulationCell>() : nullptr;
            // Get the input data buffer
            ConstDataBufferPtr vectorProperty = path.lastAs<DataBuffer>();
            if(vectorProperty && vectorProperty->componentCount() == 3) {
                OVITO_ASSERT(vectorProperty->dataType() == Property::FloatDefault);
                if(vectorProperty->dataType() == Property::FloatDefault) {
                    if(BufferReadAccess<Point3> positions = getProperty(PositionProperty)) {
                        BufferWriteAccessAndRef<Vector3, access_mode::write> filteredVectors = vectorProperty.makeCopy();
                        // wrap points
                        if(simulationCell) {
                            // create output buffer for wrapped points
                            ConstDataBufferPtr wrappedPositionsPtr = getProperty(PositionProperty);
                            BufferWriteAccessAndRef<Point3, access_mode::discard_write> wrappedPositions =
                                Lines::OOClass().createStandardProperty(DataBuffer::Uninitialized, lines->elementCount(),
                                                                        Lines::PositionProperty);
                            // process points
                            for(size_t i = 0; i < positions.size(); ++i) {
                                wrappedPositions[i] = simulationCell->wrapPoint(positions[i]);
                                if(isPointCulled(wrappedPositions[i])) {
                                    filteredVectors[i].setZero();
                                }
                            }
                            wrappedPositionsPtr = wrappedPositions.take();
                            vectorProperty = filteredVectors.take();
                            return {std::move(wrappedPositionsPtr), std::move(vectorProperty)};
                        }
                        // use unwrapped points
                        else {
                            // process points
                            for(size_t i = 0; i < positions.size(); ++i) {
                                if(isPointCulled(positions[i])) {
                                    filteredVectors[i].setZero();
                                }
                            }
                            vectorProperty = filteredVectors.take();
                            return {getProperty(PositionProperty), std::move(vectorProperty)};
                        }
                    }
                }
            }
        }
    }
    return {};
}
}  // namespace Ovito
