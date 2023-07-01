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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "VectorVis.h"
#include "ParticlesVis.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(VectorVis);
IMPLEMENT_OVITO_CLASS(VectorPickInfo);
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
VectorVis::VectorVis(ObjectInitializationFlags flags) : DataVis(flags),
    _reverseArrowDirection(false),
    _arrowPosition(Base),
    _arrowColor(1, 1, 0),
    _arrowWidth(0.5),
    _scalingFactor(1),
    _shadingMode(FlatShading),
    _offset(Vector3::Zero()),
    _coloringMode(UniformColoring)
{
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
Box3 VectorVis::boundingBox(AnimationTime time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval)
{
    const PropertyContainer* container = path.lastAs<PropertyContainer>(1);
    if(!container) return {};
    auto [basePositions, vectorProperty] = container->getVectorVisData(path, flowState, visCache);
    OVITO_ASSERT(!basePositions || basePositions->size() == container->elementCount());
    OVITO_ASSERT(!basePositions || basePositions->dataType() == PropertyObject::FloatDefault);
    OVITO_ASSERT(!basePositions || (basePositions->componentCount() == 3 && basePositions->dataType() == DataBuffer::FloatDefault));
    if(vectorProperty && ((vectorProperty->dataType() != PropertyObject::Float32 && vectorProperty->dataType() != PropertyObject::Float64) || vectorProperty->componentCount() != 3))
        vectorProperty = nullptr;

    // The key type used for caching the computed bounding box:
    using CacheKey = RendererResourceKey<struct VectorVisBoundingBoxCache,
        ConstDataObjectRef,     // Vector property
        ConstDataObjectRef,     // Base positions
        FloatType,              // Scaling factor
        FloatType,              // Arrow width
        Vector3                 // Offset
    >;

    // Look up the bounding box in the vis cache.
    auto& bbox = visCache.get<Box3>(CacheKey(
            vectorProperty,
            basePositions,
            scalingFactor(),
            arrowWidth(),
            offset()));

    // Check if the cached bounding box information is still up to date.
    if(bbox.isEmpty()) {
        // If not, recompute bounding box.
        bbox = arrowBoundingBox(vectorProperty, basePositions);
    }
    return bbox;
}

/******************************************************************************
* Computes the bounding box of the arrows.
******************************************************************************/
Box3 VectorVis::arrowBoundingBox(const DataBuffer* vectorProperty, const DataBuffer* basePositions) const
{
    if(!basePositions || !vectorProperty)
        return Box3();

    OVITO_ASSERT(basePositions->dataType() == PropertyObject::FloatDefault);
    OVITO_ASSERT(basePositions->componentCount() == 3);
    OVITO_ASSERT(vectorProperty->dataType() == PropertyObject::Float32 || vectorProperty->dataType() == PropertyObject::Float64);
    OVITO_ASSERT(vectorProperty->componentCount() == 3);
    OVITO_ASSERT(basePositions->size() == vectorProperty->size());

    // Compute bounding box of base positions (only those with non-zero vector).
    Box3 bbox;
    FloatType maxMagnitude = 0;
    BufferReadAccess<Point3> positions(basePositions);
    const Point3* p = positions.cbegin();

    if(vectorProperty->dataType() == PropertyObject::Float64) {
        BufferReadAccess<Vector_3<double>> vectorData(vectorProperty);
        for(const Vector_3<double>& v : vectorData) {
            if(v != Vector_3<double>::Zero())
                bbox.addPoint(*p);
            ++p;
        }

        // Find largest vector magnitude.
        for(const Vector_3<double>& v : vectorData) {
            auto m = v.squaredLength();
            if(m > maxMagnitude) maxMagnitude = m;
        }
    }
    else if(vectorProperty->dataType() == PropertyObject::Float32) {
        BufferReadAccess<Vector_3<float>> vectorData(vectorProperty);
        for(const Vector_3<float>& v : vectorData) {
            if(v != Vector_3<float>::Zero())
                bbox.addPoint(*p);
            ++p;
        }

        // Find largest vector magnitude.
        for(const Vector_3<float>& v : vectorData) {
            auto m = v.squaredLength();
            if(m > maxMagnitude) maxMagnitude = m;
        }
    }

    // Apply displacement offset.
    bbox.minc += offset();
    bbox.maxc += offset();

    // Enlarge the bounding box by the largest vector magnitude + padding.
    return bbox.padBox((std::sqrt(maxMagnitude) * std::abs(scalingFactor())) + arrowWidth());
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus VectorVis::render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
    PipelineStatus status;

    if(renderer->isBoundingBoxPass()) {
        TimeInterval validityInterval;
        renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, renderer->visCache(), validityInterval));
        return status;
    }

    // Get input data.
    const PropertyContainer* container = path.lastAs<PropertyContainer>(1);
    if(!container) return {};
    container->verifyIntegrity();
    auto [basePositions, vectorProperty] = container->getVectorVisData(path, flowState, renderer->visCache());
    OVITO_ASSERT(!basePositions || basePositions->size() == container->elementCount());
    OVITO_ASSERT(!basePositions || basePositions->dataType() == DataBuffer::FloatDefault);
    OVITO_ASSERT(!basePositions || (basePositions->componentCount() == 3 && basePositions->dataType() == DataBuffer::FloatDefault));
    if(vectorProperty && ((vectorProperty->dataType() != PropertyObject::Float32 && vectorProperty->dataType() != PropertyObject::Float64) || vectorProperty->componentCount() != 3))
        vectorProperty.reset();

    const PropertyObject* vectorColorProperty = nullptr;
    if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(container))
        vectorColorProperty = particles->getProperty(ParticlesObject::VectorColorProperty);

    // Make sure we don't exceed our internal limits.
    if(vectorProperty && vectorProperty->size() > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("This version of OVITO cannot render more than %1 vector arrows.").arg(std::numeric_limits<int>::max()));
    }

    // Look for selected pseudo-coloring property.
    const PropertyObject* pseudoColorProperty = nullptr;
    int pseudoColorPropertyComponent = 0;
    PseudoColorMapping pseudoColorMapping;
    if(coloringMode() == PseudoColoring && colorMapping() && colorMapping()->sourceProperty() && !vectorColorProperty) {
        pseudoColorProperty = colorMapping()->sourceProperty().findInContainer(container);
        if(!pseudoColorProperty) {
            status = PipelineStatus(PipelineStatus::Error, tr("The particle property with the name '%1' does not exist.").arg(colorMapping()->sourceProperty().name()));
        }
        else {
            if(colorMapping()->sourceProperty().vectorComponent() >= (int)pseudoColorProperty->componentCount()) {
                status = PipelineStatus(PipelineStatus::Error, tr("The vector component is out of range. The particle property '%1' has only %2 values per data element.").arg(colorMapping()->sourceProperty().name()).arg(pseudoColorProperty->componentCount()));
                pseudoColorProperty = nullptr;
            }
            pseudoColorPropertyComponent = std::max(0, colorMapping()->sourceProperty().vectorComponent());
            pseudoColorMapping = colorMapping()->pseudoColorMapping();
        }
    }

    // The key type used for caching the rendering primitive:
    using CacheKey = RendererResourceKey<struct VectorVisCache,
        ConstDataObjectRef,     // Vector property
        ConstDataObjectRef,     // Base positions
        ShadingMode,            // Arrow shading mode
        FloatType,              // Scaling factor
        FloatType,              // Arrow width
        Color,                  // Arrow color
        GraphicsFloatType,      // Arrow transparency
        bool,                   // Reverse arrow direction
        ArrowPosition,          // Arrow position
        ConstDataObjectRef,     // Vector color property
        ConstDataObjectRef,     // Pseudo-color property
        int,                    // Pseudo-color vector component
        PseudoColorMapping      // Pseudo-color mapping
    >;

    // Determine effective color including alpha value.
    GraphicsFloatType transparency = 0;
    TimeInterval iv;
    if(transparencyController())
        transparency = transparencyController()->getFloatValue(time, iv);

    // Lookup the rendering primitive in the vis cache.
    auto& arrows = renderer->visCache().get<CylinderPrimitive>(CacheKey(
            vectorProperty,
            basePositions,
            shadingMode(),
            scalingFactor(),
            arrowWidth(),
            arrowColor(),
            transparency,
            reverseArrowDirection(),
            arrowPosition(),
            vectorColorProperty,
            pseudoColorProperty,
            pseudoColorPropertyComponent,
            pseudoColorMapping));

    // Check if we already have a valid rendering primitive that is up to date.
    if(!arrows.basePositions()) {

        // Determine number of non-zero vectors.
        int vectorCount = 0;
        BufferReadAccess<Vector_3<float>> vectorData32(vectorProperty->dataType() == DataBuffer::Float32 ? vectorProperty : nullptr);
        BufferReadAccess<Vector_3<double>> vectorData64(vectorProperty->dataType() == DataBuffer::Float64 ? vectorProperty : nullptr);
        if(basePositions) {
            if(vectorData32) {
                for(const auto& v : vectorData32) {
                    if(v != Vector_3<float>::Zero())
                        vectorCount++;
                }
            }
            else if(vectorData64) {
                for(const auto& v : vectorData64) {
                    if(v != Vector_3<double>::Zero())
                        vectorCount++;
                }
            }
        }

        // Allocate data buffers.
        BufferFactory<Point3G> arrowBasePositions(vectorCount);
        BufferFactory<Point3G> arrowHeadPositions(vectorCount);
        BufferFactory<ColorG> arrowColors = (vectorColorProperty || pseudoColorProperty) ? BufferFactory<ColorG>(vectorCount) : BufferFactory<ColorG>{};

        // Fill data buffers.
        if(vectorCount) {
            FloatType scalingFac = scalingFactor();
            if(reverseArrowDirection())
                scalingFac = -scalingFac;
            BufferReadAccess<Point3> basePositionData(basePositions);
            BufferReadAccess<ColorG> vectorColorData(vectorColorProperty);
            RawBufferReadAccess vectorPseudoColorData(pseudoColorProperty);
            size_t inIndex = 0;
            size_t outIndex = 0;
            const auto arrowPosition = this->arrowPosition();
            for(size_t inIndex = 0; inIndex < basePositionData.size(); inIndex++) {
                const Vector3G vec = vectorData32 ? vectorData32[inIndex].toDataType<GraphicsFloatType>() : vectorData64[inIndex].toDataType<GraphicsFloatType>();
                if(vec != Vector3G::Zero()) {
                    Vector3G v = vec * scalingFac;
                    Point3G base = basePositionData[inIndex].toDataType<GraphicsFloatType>();
                    if(arrowPosition == Head)
                        base -= v;
                    else if(arrowPosition == Center)
                        base -= v * GraphicsFloatType(0.5);
                    arrowBasePositions[outIndex] = base;
                    arrowHeadPositions[outIndex] = base + v;
                    if(vectorColorProperty)
                        arrowColors[outIndex] = vectorColorData[inIndex];
                    else if(pseudoColorProperty)
                        arrowColors[outIndex] = pseudoColorMapping.valueToColor(vectorPseudoColorData.get<GraphicsFloatType>(inIndex, pseudoColorPropertyComponent));
                    outIndex++;
                }
            }
            OVITO_ASSERT(outIndex == vectorCount);
        }

        // Create arrow rendering primitive.
        arrows.setShape(CylinderPrimitive::ArrowShape);
        arrows.setShadingMode(static_cast<CylinderPrimitive::ShadingMode>(shadingMode()));
        arrows.setUniformWidth(2 * arrowWidth());
        arrows.setUniformColor(arrowColor());
        arrows.setPositions(arrowBasePositions.take(), arrowHeadPositions.take());
        arrows.setColors(arrowColors.take());
        if(transparency > 0) {
            DataBufferPtr transparencyBuffer = DataBufferPtr::create(vectorCount, DataBuffer::FloatGraphics);
            transparencyBuffer->fill<GraphicsFloatType>(transparency);
            arrows.setTransparencies(std::move(transparencyBuffer));
        }
    }

    if(renderer->isPicking()) {
        renderer->beginPickObject(contextNode, OORef<VectorPickInfo>::create(this, path));
    }
    AffineTransformation oldTM = renderer->worldTransform();
    renderer->setWorldTransform(AffineTransformation::translation(offset()) * oldTM);
    renderer->renderCylinders(arrows);
    renderer->setWorldTransform(oldTM);
    if(renderer->isPicking()) {
        renderer->endPickObject();
    }

    return status;
}

/******************************************************************************
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding data element index.
******************************************************************************/
size_t VectorPickInfo::elementIndexFromSubObjectID(quint32 subobjID) const
{
    if(const PropertyObject* vectorProperty = dataPath().lastAs<PropertyObject>()) {
        size_t elementIndex = 0;
        if(vectorProperty->dataType() == DataBuffer::Float32) {
            BufferReadAccess<Vector_3<float>> vectorData(vectorProperty);
            for(const Vector_3<float>& v : vectorData) {
                if(v != Vector_3<float>::Zero()) {
                    if(subobjID == 0) return elementIndex;
                    subobjID--;
                }
                elementIndex++;
            }
        }
        else if(vectorProperty->dataType() == DataBuffer::Float64) {
            BufferReadAccess<Vector_3<double>> vectorData(vectorProperty);
            for(const Vector_3<double>& v : vectorData) {
                if(v != Vector_3<double>::Zero()) {
                    if(subobjID == 0) return elementIndex;
                    subobjID--;
                }
                elementIndex++;
            }
        }
    }
    return std::numeric_limits<size_t>::max();
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString VectorPickInfo::infoString(PipelineSceneNode* objectNode, quint32 subobjectId)
{
    size_t elementIndex = elementIndexFromSubObjectID(subobjectId);
    if(elementIndex != std::numeric_limits<size_t>::max()) {
        if(const PropertyContainer* container = dataPath().lastAs<PropertyContainer>(1))
            return container->elementInfoString(elementIndex, dataPath());
    }
    return {};
}

}   // End of namespace
