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
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "VectorVis.h"
#include "Vectors.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorVis);
OVITO_CLASSINFO(VectorVis, "DisplayName", "Vectors");
IMPLEMENT_ABSTRACT_OVITO_CLASS(VectorPickInfo);
DEFINE_PROPERTY_FIELD(VectorVis, reverseArrowDirection);
DEFINE_PROPERTY_FIELD(VectorVis, arrowPosition);
DEFINE_PROPERTY_FIELD(VectorVis, arrowColor);
DEFINE_PROPERTY_FIELD(VectorVis, arrowWidth);
DEFINE_PROPERTY_FIELD(VectorVis, scalingFactor);
DEFINE_PROPERTY_FIELD(VectorVis, shadingMode);
DEFINE_REFERENCE_FIELD(VectorVis, transparencyController);
DEFINE_PROPERTY_FIELD(VectorVis, offset);
DEFINE_PROPERTY_FIELD(VectorVis, coloringMode);
DEFINE_REFERENCE_FIELD(VectorVis, colorMapping);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, reverseArrowDirection);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, arrowPosition);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, arrowColor);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, arrowWidth);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, scalingFactor);
DEFINE_SHADOW_PROPERTY_FIELD(VectorVis, shadingMode);
SET_PROPERTY_FIELD_LABEL(VectorVis, arrowColor, "Arrow color");
SET_PROPERTY_FIELD_LABEL(VectorVis, arrowWidth, "Arrow width");
SET_PROPERTY_FIELD_LABEL(VectorVis, scalingFactor, "Scaling factor");
SET_PROPERTY_FIELD_LABEL(VectorVis, reverseArrowDirection, "Reverse direction");
SET_PROPERTY_FIELD_LABEL(VectorVis, arrowPosition, "Position");
SET_PROPERTY_FIELD_LABEL(VectorVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(VectorVis, transparencyController, "Transparency");
SET_PROPERTY_FIELD_LABEL(VectorVis, offset, "Offset");
SET_PROPERTY_FIELD_LABEL(VectorVis, coloringMode, "Coloring mode");
SET_PROPERTY_FIELD_LABEL(VectorVis, colorMapping, "Color mapping");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(VectorVis, arrowWidth, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(VectorVis, scalingFactor, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(VectorVis, transparencyController, PercentParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS(VectorVis, offset, WorldParameterUnit);

/******************************************************************************
* Constructor.
******************************************************************************/
void VectorVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create animation controller for the transparency parameter.
        setTransparencyController(ControllerManager::createFloatController());

        // Create a color mapping object for pseudo-color visualization of an auxiliary property.
        setColorMapping(OORef<PropertyColorMapping>::create(flags));
    }
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void VectorVis::loadFromStreamComplete(ObjectLoadStream& stream)
{
    DataVis::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.5.4.
    // Create a color mapping sub-object if it wasn't loaded from the state file.
    if(!colorMapping())
        setColorMapping(OORef<PropertyColorMapping>::create());
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 VectorVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const PropertyContainer* container = path.lastAs<PropertyContainer>(1);
    if(!container)
        return {};

    VectorData vectorData = container->getVectorVisData(
        path, flowState, this_task::ui()->datasetContainer().visCache()->acquireResourceFrame());
    OVITO_ASSERT(!vectorData.positions || vectorData.positions->size() == container->elementCount());
    OVITO_ASSERT(!vectorData.positions || vectorData.positions->dataType() == Property::FloatDefault);
    OVITO_ASSERT(!vectorData.positions ||
                 (vectorData.positions->componentCount() == 3 && vectorData.positions->dataType() == DataBuffer::FloatDefault));
    if(vectorData.directions &&
       ((vectorData.directions->dataType() != Property::Float32 && vectorData.directions->dataType() != Property::Float64) ||
        vectorData.directions->componentCount() != 3))
        vectorData.directions = nullptr;

    return arrowBoundingBox(vectorData.directions, vectorData.positions);
}

/******************************************************************************
* Computes the bounding box of the arrows.
******************************************************************************/
Box3 VectorVis::arrowBoundingBox(const DataBuffer* vectorProperty, const DataBuffer* basePositions) const
{
    if(!basePositions || !vectorProperty)
        return Box3();

    OVITO_ASSERT(basePositions->dataType() == Property::FloatDefault);
    OVITO_ASSERT(basePositions->componentCount() == 3);
    OVITO_ASSERT(vectorProperty->dataType() == Property::Float32 || vectorProperty->dataType() == Property::Float64);
    OVITO_ASSERT(vectorProperty->componentCount() == 3);
    OVITO_ASSERT(basePositions->size() == vectorProperty->size());

    // Compute bounding box of base positions (only those with non-zero vector).
    Box3 bbox;
    FloatType maxMagnitude = 0;
    BufferReadAccess<Point3> positions(basePositions);
    const Point3* p = positions.cbegin();

    vectorProperty->forTypes<Property::Float32, Property::Float64>([&](auto _) {
        using T = decltype(_);
        BufferReadAccess<Vector_3<T>> vectorData(vectorProperty);
        for(const Vector_3<T>& v : vectorData) {
            if(v != typename Vector_3<T>::Zero()) {
                bbox.addPoint(*p);
            }
            ++p;
        }

        // Find largest vector magnitude.
        for(const Vector_3<T>& v : vectorData) {
            auto m = v.squaredLength();
            if(m > maxMagnitude) maxMagnitude = m;
        }
    });

    // Apply displacement offset.
    bbox.minc += offset();
    bbox.maxc += offset();

    // Enlarge the bounding box by the largest vector magnitude + padding.
    return bbox.padBox((std::sqrt(maxMagnitude) * std::abs(scalingFactor())) + arrowWidth());
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> VectorVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    PipelineStatus status;

    // Get input data.
    DataOORef<const PropertyContainer> container;
    // Check last element in path:
    container = path.lastAs<PropertyContainer>();
    // If last element is not the container - check second to last element:
    if(!container)
        container = path.lastAs<const PropertyContainer>(1);
    // Nothing to do
    if(!container)
        return {};

    container->verifyIntegrity();
    VectorData vectorData = container->getVectorVisData(path, flowState, frameGraph.visCache());
    OVITO_ASSERT(!vectorData.positions || vectorData.positions->size() == container->elementCount());
    OVITO_ASSERT(!vectorData.positions || vectorData.positions->dataType() == DataBuffer::FloatDefault);
    OVITO_ASSERT(!vectorData.positions ||
                 (vectorData.positions->componentCount() == 3 && vectorData.positions->dataType() == DataBuffer::FloatDefault));
    if(vectorData.directions &&
       ((vectorData.directions->dataType() != Property::Float32 && vectorData.directions->dataType() != Property::Float64) ||
        vectorData.directions->componentCount() != 3))
        vectorData.directions.reset();

    // Make sure we don't exceed our internal limits.
    if(vectorData.directions && vectorData.directions->size() > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("This version of OVITO cannot render more than %1 vector arrows.").arg(std::numeric_limits<int>::max()));
    }

    // Only use selection in interactive context
    if(!frameGraph.isInteractive()) {
        vectorData.selection = nullptr;
    }

    // Look for selected pseudo-coloring property.
    const Property* pseudoColorProperty = nullptr;
    int pseudoColorPropertyComponent = 0;
    PseudoColorMapping pseudoColorMapping;
    if(coloringMode() == PseudoColoring && colorMapping() && colorMapping()->sourceProperty() && !vectorData.colors) {
        QString errorDescription;
        std::tie(pseudoColorProperty, pseudoColorPropertyComponent) = colorMapping()->sourceProperty().findInContainerWithComponent(container, errorDescription);
        if(!pseudoColorProperty) {
            status = PipelineStatus(PipelineStatus::Error, std::move(errorDescription));
        }
        else {
            pseudoColorMapping = colorMapping()->pseudoColorMapping();
        }
    }

    // The key type used for caching the rendering primitive:
    using CacheKey = RendererResourceKey<struct VectorVisCache,
                                         ConstDataObjectRef,  // Vector property
                                         ConstDataObjectRef,  // Base positions
                                         ConstDataObjectRef,  // Selection property
                                         ShadingMode,         // Arrow shading mode
                                         FloatType,           // Scaling factor
                                         FloatType,           // Arrow width
                                         Color,               // Arrow color
                                         GraphicsFloatType,   // Arrow transparency
                                         bool,                // Reverse arrow direction
                                         ArrowPosition,       // Arrow position
                                         ConstDataObjectRef,  // Vector color property
                                         ConstDataObjectRef,  // Vector transparency property
                                         ConstDataObjectRef,  // Pseudo-color property
                                         int,                 // Pseudo-color vector component
                                         PseudoColorMapping   // Pseudo-color mapping
                                         >;

    // Determine effective color including alpha value.
    GraphicsFloatType transparency = 0;
    TimeInterval iv;
    if(transparencyController())
        transparency = transparencyController()->getFloatValue(frameGraph.time(), iv);

    // Lookup the rendering primitive in the vis cache.
    const CylinderPrimitive& arrows = frameGraph.visCache().lookup<CylinderPrimitive>(
        CacheKey{
            vectorData.directions,
            vectorData.positions,
            vectorData.selection,
            shadingMode(),
            scalingFactor(),
            arrowWidth(),
            arrowColor(),
            transparency,
            reverseArrowDirection(),
            arrowPosition(),
            vectorData.colors,
            vectorData.transparencies,
            pseudoColorProperty,
            pseudoColorPropertyComponent,
            pseudoColorMapping,
        },
        [&](CylinderPrimitive& arrows) {
            // Determine number of non-zero vectors.
            int vectorCount = 0;
            BufferReadAccess<SelectionIntType> selectionProperty(vectorData.selection);

            vectorData.directions->forTypes<DataBuffer::Float32, DataBuffer::Float64>([&](auto _) {
                using T = decltype(_);
                for(const auto& v : BufferReadAccess<Vector_3<T>>(vectorData.directions)) {
                    vectorCount += (v != typename Vector_3<T>::Zero());
                }
            });

            // Allocate data buffers.
            BufferFactory<Point3G> arrowBasePositions(vectorCount);
            BufferFactory<Point3G> arrowHeadPositions(vectorCount);
            BufferFactory<ColorG> arrowColors =
                (vectorData.colors || pseudoColorProperty) ? BufferFactory<ColorG>(vectorCount) : BufferFactory<ColorG>{};
            BufferFactory<GraphicsFloatType> arrowTransparencies =
                vectorData.transparencies ? BufferFactory<GraphicsFloatType>(vectorCount) : BufferFactory<GraphicsFloatType>{};
            BufferFactory<SelectionIntType> arrowSelection(vectorData.selection ? vectorCount : 0);

            // Fill data buffers.
            if(vectorCount) {
                FloatType scalingFac = scalingFactor();
                if(reverseArrowDirection())
                    scalingFac = -scalingFac;
                BufferReadAccess<Point3> basePositionData(vectorData.positions);
                BufferReadAccess<ColorG> vectorColorData(vectorData.colors);
                BufferReadAccess<GraphicsFloatType> vectorTransparencyData(vectorData.transparencies);
                BufferReadAccess<SelectionIntType> vectorSelectionData(vectorData.selection);
                RawBufferReadAccess vectorPseudoColorData(pseudoColorProperty);
                size_t outIndex = 0;
                const auto arrowPosition = this->arrowPosition();

                vectorData.directions->forTypes<DataBuffer::Float32, DataBuffer::Float64>([&](auto _) {
                    using T = decltype(_);
                    BufferReadAccess<Vector_3<T>> vectorDirs(vectorData.directions);
                    for(size_t inIndex = 0; inIndex < basePositionData.size(); inIndex++) {
                        const Vector3G vec = vectorDirs[inIndex].template toDataType<GraphicsFloatType>();
                        if(vec != Vector3G::Zero()) {
                            Vector3G v = vec * scalingFac;
                            Point3G base = basePositionData[inIndex].toDataType<GraphicsFloatType>();
                            if(arrowPosition == Head)
                                base -= v;
                            else if(arrowPosition == Center)
                                base -= v * GraphicsFloatType(0.5);
                            arrowBasePositions[outIndex] = base;
                            arrowHeadPositions[outIndex] = base + v;
                            if(vectorData.colors)
                                arrowColors[outIndex] = vectorColorData[inIndex];
                            else if(pseudoColorProperty)
                                arrowColors[outIndex] = pseudoColorMapping.valueToColor(
                                    vectorPseudoColorData.get<GraphicsFloatType>(inIndex, pseudoColorPropertyComponent));
                            if(vectorData.selection) {
                                arrowSelection[outIndex] = vectorSelectionData[inIndex];
                            }
                            if(vectorData.transparencies) arrowTransparencies[outIndex] = vectorTransparencyData[inIndex];
                            outIndex++;
                        }
                    }
                });
                OVITO_ASSERT(outIndex == vectorCount);
            }

            // Create arrow rendering primitive.
            arrows.setShape(CylinderPrimitive::ArrowShape);
            arrows.setShadingMode(static_cast<CylinderPrimitive::ShadingMode>(shadingMode()));
            arrows.setUniformWidth(2 * arrowWidth());
            arrows.setUniformColor(arrowColor());
            arrows.setPositions(arrowBasePositions.take(), arrowHeadPositions.take());
            arrows.setColors(arrowColors.take());
            if(arrowTransparencies) {
                arrows.setTransparencies(arrowTransparencies.take());
            }
            else if(transparency > 0) {
                DataBufferPtr transparencyBuffer = DataBufferPtr::create(vectorCount, DataBuffer::FloatGraphics);
                transparencyBuffer->fill<GraphicsFloatType>(transparency);
                arrows.setTransparencies(std::move(transparencyBuffer));
            }
            if(vectorData.selection) {
                arrows.setSelection(arrowSelection.take());
            }
        });

    // Get world transformation matrix of scene node.
    const AffineTransformation& nodeTM = pipeline->getWorldTransform(frameGraph.time());

    // Apply offset translation.
    const AffineTransformation tm = AffineTransformation::translation(offset()) * nodeTM;

    // Add arrow glyphs to the frame graph.
    frameGraph.addCommandGroup(FrameGraph::SceneLayer).addPrimitive(std::make_unique<CylinderPrimitive>(arrows), tm, arrows.computeBoundingBox(frameGraph.visCache()), pipeline, OORef<VectorPickInfo>::create(this, path));

    return status;
}

/******************************************************************************
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding data element index.
******************************************************************************/
size_t VectorPickInfo::elementIndexFromSubObjectID(quint32 subobjID) const
{
    size_t elementIndex = std::numeric_limits<size_t>::max();

    const Property* vectorProperty = nullptr;
    // Check if last element in path is a vectors container.
    if(const Vectors* container = dataPath().lastAs<Vectors>())
        // Extract direction property from vectors container.
        vectorProperty = container->getProperty(Vectors::Type::DirectionProperty);
    else
        // Otherwise grab property from path.
        vectorProperty = dataPath().lastAs<Property>();

    if(vectorProperty) {
        vectorProperty->forTypes<DataBuffer::Float32, DataBuffer::Float64>([&](auto _) {
            using T = decltype(_);
            size_t i = 0;
            for(const auto& v : BufferReadAccess<Vector_3<T>>(vectorProperty)) {
                if(v != typename Vector_3<T>::Zero()) {
                    if(subobjID == 0) {
                        elementIndex = i;
                        return;
                    }
                    subobjID--;
                }
                i++;
            }
        });
    }
    return elementIndex;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString VectorPickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    size_t elementIndex = elementIndexFromSubObjectID(subobjectId);
    if(elementIndex != std::numeric_limits<size_t>::max()) {
        // Check last element in path:
        const PropertyContainer* container = dataPath().lastAs<PropertyContainer>(0);
        // If last element is not the container - check second to last element:
        if(!container)
            container = dataPath().lastAs<PropertyContainer>(1);
        if(container)
            return container->elementInfoString(elementIndex, dataPath());
    }
    return {};
}

}   // End of namespace
