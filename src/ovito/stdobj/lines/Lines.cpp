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
#include <ovito/stdobj/lines/LinesVis.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "Lines.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Lines);
OVITO_CLASSINFO(Lines, "DisplayName", "Lines");
OVITO_CLASSINFO(Lines, "ClassNameAlias", "TrajectoryLines");   // For backward compatibility with OVITO 3.9.2
OVITO_CLASSINFO(Lines, "ClassNameAlias", "TrajectoryObject");  // For backward compatibility with OVITO 3.9.2
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
void Lines::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Create and attach a default visualization element for rendering the lines.
            setVisElement(OORef<LinesVis>::create(flags));
        }
    }
}

/******************************************************************************
 * Returns the data for visualizing a vector property from this container using a VectorVis element.
 ******************************************************************************/
VectorVis::VectorData Lines::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                              const RendererResourceCache::ResourceFrame& visCache) const
{
    // Get lines object
    if(const Lines* lines = path.lastAs<Lines>(1)) {
        OVITO_ASSERT(path.lastAs<Lines>(1) == this);
        lines->verifyIntegrity();

        // Get the simulation cell if needed. This depends on the settings made in the visualization element.
        const SimulationCell* simulationCell = nullptr;
        if(const LinesVis* linesVis = dynamic_object_cast<LinesVis>(lines->visElement())) {
            if(linesVis->wrappedLines()) {
                simulationCell = state.getObject<SimulationCell>();
            }
        }

        // Get the input data buffer
        ConstPropertyPtr vectorProperty = path.lastAs<Property>();
        if(vectorProperty && vectorProperty->componentCount() == 3) {
            OVITO_ASSERT(vectorProperty->dataType() == Property::FloatDefault);
            if(vectorProperty->dataType() == Property::FloatDefault) {
                if(const Property* positions = getProperty(PositionProperty)) {
                    // The line points are expensive to filter. That's why we store them in the vis cache.
                    const auto& [filteredPositions, filteredVectors] = visCache.lookup<std::tuple<ConstDataBufferPtr, ConstDataBufferPtr>>(
                        RendererResourceKey<struct LinesVectorVisCache, ConstDataObjectRef, ConstDataObjectRef, DataOORef<const SimulationCell>>{
                            positions,
                            vectorProperty,
                            simulationCell
                        },
                        [&](ConstDataBufferPtr& filteredPositions, ConstDataBufferPtr& filteredVectors) {
                            if(simulationCell) {
                                // Use wrapped point positions.
                                filteredPositions = simulationCell->wrapPoints(positions);
                            }
                            else {
                                // Use unwrapped point positions
                                filteredPositions = positions;
                            }

                            if(cuttingPlanes().empty()) {
                                // Use vectors as they are.
                                filteredVectors = vectorProperty;
                            }
                            else {
                                BufferReadAccess<Vector3> vecInAcc{vectorProperty};
                                BufferWriteAccessAndRef<Vector3, access_mode::discard_write> vecOutAcc{vectorProperty->cloneWithoutData(vectorProperty->size())};
                                // Cull points at the clipping planes. Culled points get hidden by setting their correponding vector to zero.
                                size_t i = 0;
                                for(const Point3& p : BufferReadAccess<Point3>{filteredPositions}) {
                                    if(isPointCulled(p))
                                        vecOutAcc[i].setZero();
                                    else
                                        vecOutAcc[i] = vecInAcc[i];
                                    i++;
                                }
                                filteredVectors = vecOutAcc.take();
                            }
                        });

                    return { filteredPositions, filteredVectors, nullptr, nullptr };
                }
            }
        }
    }
    return {};
}

}  // namespace Ovito
