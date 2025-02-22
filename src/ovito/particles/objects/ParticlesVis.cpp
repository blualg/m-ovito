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
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "ParticlesVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesVis);
OVITO_CLASSINFO(ParticlesVis, "DisplayName", "Particles");
IMPLEMENT_ABSTRACT_OVITO_CLASS(ParticlePickInfo);
DEFINE_PROPERTY_FIELD(ParticlesVis, defaultParticleRadius);
DEFINE_PROPERTY_FIELD(ParticlesVis, radiusScaleFactor);
DEFINE_PROPERTY_FIELD(ParticlesVis, renderingQuality);
DEFINE_PROPERTY_FIELD(ParticlesVis, particleShape);
SET_PROPERTY_FIELD_LABEL(ParticlesVis, defaultParticleRadius, "Standard radius");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, radiusScaleFactor, "Radius scaling factor");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, renderingQuality, "Rendering quality");
SET_PROPERTY_FIELD_LABEL(ParticlesVis, particleShape, "Standard shape");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticlesVis, defaultParticleRadius, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticlesVis, radiusScaleFactor, PercentParameterUnit, 0);

/******************************************************************************
* Computes the bounding box of the visual element.
******************************************************************************/
Box3 ParticlesVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const Particles* particles = path.lastAs<Particles>();
    if(!particles)
        return {};

    particles->verifyIntegrity();
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    const Property* shapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);

    // Compute bounding box from particle data.
    return particleBoundingBox(positionProperty, typeProperty, radiusProperty, shapeProperty, true);
}

/******************************************************************************
* Computes the bounding box of the particles.
******************************************************************************/
Box3 ParticlesVis::particleBoundingBox(BufferReadAccess<Point3> positionProperty, const Property* typeProperty, BufferReadAccess<GraphicsFloatType> radiusProperty, BufferReadAccess<Vector3G> shapeProperty, bool includeParticleRadius) const
{
    OVITO_ASSERT(typeProperty == nullptr || typeProperty->typeId() == Particles::TypeProperty);
    if(particleShape() != Sphere && particleShape() != Box && particleShape() != Cylinder && particleShape() != Spherocylinder)
        shapeProperty.reset();

    Box3 bbox;
    if(positionProperty) {
        bbox.addPoints(positionProperty);
    }
    if(!includeParticleRadius)
        return bbox;

    // Check if any of the particle types have a user-defined mesh geometry assigned.
    std::vector<std::pair<int,FloatType>> userShapeParticleTypes;
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                if(ptype->shapeMesh() && ptype->shapeMesh()->faceCount() != 0) {
                    // Compute the maximum extent of the user-defined shape mesh.
                    const Box3& bbox = ptype->shapeMesh()->boundingBox();
                    FloatType extent = std::max((bbox.minc - Point3::Origin()).length(), (bbox.maxc - Point3::Origin()).length());
                    userShapeParticleTypes.emplace_back(ptype->numericId(), extent);
                }
            }
        }
    }

    // Extend box to account for radii/shape of particles.
    FloatType maxAtomRadius = 0;

    if(userShapeParticleTypes.empty()) {
        // Standard case - no user-defined particle shapes assigned:
        if(typeProperty) {
            for(const auto& it : ParticleType::typeRadiusMap(typeProperty)) {
                maxAtomRadius = std::max(maxAtomRadius, (it.second != 0) ? it.second : defaultParticleRadius());
            }
        }
        if(maxAtomRadius == 0)
            maxAtomRadius = defaultParticleRadius();
        if(shapeProperty) {
            bool shapeSetForAllParticles = true;
            FloatType maxShapeSize = 0;
            for(const Vector3G& s : shapeProperty) {
                if(s.x() <= 0 && s.y() <= 0 && s.z() <= 0)
                    shapeSetForAllParticles = false;
                else
                    maxShapeSize = std::max(maxShapeSize, static_cast<FloatType>(std::max(s.x(), std::max(s.y(), s.z()))));
            }
            if(particleShape() == Spherocylinder)
                maxShapeSize *= 2;
            if(shapeSetForAllParticles)
                maxAtomRadius = maxShapeSize;
            else
                maxAtomRadius = std::max(maxAtomRadius, maxShapeSize);
        }
        if(radiusProperty && radiusProperty.size() != 0) {
            auto minmax = std::minmax_element(radiusProperty.cbegin(), radiusProperty.cend());
            if(*minmax.first <= 0)
                maxAtomRadius = std::max(maxAtomRadius, static_cast<FloatType>(*minmax.second));
            else
                maxAtomRadius = *minmax.second;
        }
    }
    else {
        // Non-standard case - at least one user-defined particle shape assigned:
        std::map<int,FloatType> typeRadiusMap = ParticleType::typeRadiusMap(typeProperty);
        if(radiusProperty && radiusProperty.size() == typeProperty->size()) {
            const auto* r = radiusProperty.cbegin();
            BufferReadAccess<int32_t> typeData(typeProperty);
            for(int32_t t : typeData) {
                // Determine effective radius of the current particle.
                FloatType radius = *r++;
                if(radius <= 0) radius = typeRadiusMap[t];
                if(radius <= 0) radius = defaultParticleRadius();
                // Effective radius is multiplied with the extent of the user-defined shape mesh.
                bool foundMeshExtent = false;
                for(const auto& entry : userShapeParticleTypes) {
                    if(entry.first == t) {
                        maxAtomRadius = std::max(maxAtomRadius, radius * entry.second);
                        foundMeshExtent = true;
                        break;
                    }
                }
                // If this particle type has no user-defined shape assigned, simply use radius.
                if(!foundMeshExtent)
                    maxAtomRadius = std::max(maxAtomRadius, radius);
            }
        }
        else {
            for(const auto& it : typeRadiusMap) {
                FloatType typeRadius = (it.second != 0) ? it.second : defaultParticleRadius();
                bool foundMeshExtent = false;
                for(const auto& entry : userShapeParticleTypes) {
                    if(entry.first == it.first) {
                        maxAtomRadius = std::max(maxAtomRadius, typeRadius * entry.second);
                        foundMeshExtent = true;
                        break;
                    }
                }
                // If this particle type has no user-defined shape assigned, simply use radius.
                if(!foundMeshExtent)
                    maxAtomRadius = std::max(maxAtomRadius, typeRadius);
            }
        }
    }

    // Extend the bounding box by the largest particle radius.
    return bbox.padBox(std::max(radiusScaleFactor() * maxAtomRadius * std::sqrt(FloatType(3)), FloatType(0)));
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering colors
* of particles (if no per-particle colors are defined).
******************************************************************************/
const Property* ParticlesVis::getParticleTypeColorProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::TypeProperty);
}

/******************************************************************************
* Determines the display particle colors.
******************************************************************************/
ConstPropertyPtr ParticlesVis::particleColors(const Particles* particles, bool highlightSelection) const
{
    OVITO_ASSERT(particles);
    particles->verifyIntegrity();

    // Take particle colors directly from the 'Color' property if available.
    ConstPropertyPtr output = particles->getProperty(Particles::ColorProperty);
    if(!output) {
        // Allocate new output color array.
        output.reset(Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::ColorProperty));

        const ColorG defaultColor = defaultParticleColor().toDataType<GraphicsFloatType>();
        if(const Property* typeProperty = getParticleTypeColorProperty(particles)) {
            OVITO_ASSERT(typeProperty->size() == output->size());
            // Assign colors based on particle types.
            // Generate a lookup map for particle type colors.
            const std::map<int, ColorG> colorMap = typeProperty->typeColorMap();
            std::array<ColorG, 16> colorArray;
            // Check if all type IDs are within a small, non-negative range.
            // If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
            if(boost::algorithm::all_of(colorMap, [&colorArray](const auto& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
                colorArray.fill(defaultColor);
                for(const auto& entry : colorMap)
                    colorArray[entry.first] = entry.second;
                // Fill color array.
                BufferReadAccess<int32_t> typeData(typeProperty);
                const auto* t = typeData.cbegin();
                for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output.makeMutableInplace())) {
                    if(*t >= 0 && (size_t)*t < colorArray.size())
                        c = colorArray[*t];
                    else
                        c = defaultColor;
                    ++t;
                }
            }
            else {
                // Fill color array.
                BufferReadAccess<int32_t> typeData(typeProperty);
                const auto* t = typeData.cbegin();
                for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output.makeMutableInplace())) {
                    if(auto it = colorMap.find(*t); it != colorMap.end())
                        c = it->second;
                    else
                        c = defaultColor;
                    ++t;
                }
            }
        }
        else {
            // Assign a uniform color to all particles.
            output.makeMutableInplace()->fill<ColorG>(defaultColor);
        }
    }

    // Highlight selected particles with a special color.
    if(const Property* selectionProperty = highlightSelection ? particles->getProperty(Particles::SelectionProperty) : nullptr)
        output.makeMutableInplace()->fillSelected<ColorG>(selectionParticleColor().toDataType<GraphicsFloatType>(), *selectionProperty);

    return output;
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering radii
* of particles (if no per-particle radii are defined).
******************************************************************************/
const Property* ParticlesVis::getParticleTypeRadiusProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::TypeProperty);
}

/******************************************************************************
* Determines the display particle radii.
******************************************************************************/
ConstPropertyPtr ParticlesVis::particleRadii(const Particles* particles, bool includeGlobalScaleFactor) const
{
    particles->verifyIntegrity();
    GraphicsFloatType defaultRadius = defaultParticleRadius();
    if(includeGlobalScaleFactor)
        defaultRadius *= radiusScaleFactor();

    // Take particle radii directly from the 'Radius' property if available.
    ConstPropertyPtr output = particles->getProperty(Particles::RadiusProperty);
    if(output) {
        // Check if the radius array contains any zero entries.
        BufferReadAccess<GraphicsFloatType> radiusArray(output);
        if(boost::find(radiusArray, GraphicsFloatType(0)) != radiusArray.end()) {
            radiusArray.reset();

            // Copy per-type radii to those particles whose "Radius" property value is zero.
            if(const Property* typeProperty = getParticleTypeRadiusProperty(particles)) {
                // Build a lookup map for particle type radii.
                std::map<int,FloatType> radiusMap = ParticleType::typeRadiusMap(typeProperty);
                // Skip the following loop if all per-type radii are zero.
                if(boost::algorithm::any_of(radiusMap, [](const std::pair<int,FloatType>& it) { return it.second != 0; })) {
                    // Fill radius array.
                    BufferReadAccess<int32_t> typeArray(typeProperty);
                    const auto* type = typeArray.cbegin();
                    for(auto& radius : BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace())) {
                        if(radius <= 0) {
                            auto it = radiusMap.find(*type);
                            if(it != radiusMap.end())
                                radius = it->second;
                        }
                        ++type;
                    }
                }
            }

            // Replace remaining zero entries in the "Radius" array with the uniform default radius.
            boost::replace(BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace()), GraphicsFloatType(0), static_cast<GraphicsFloatType>(defaultParticleRadius()));
        }
        // Apply global scaling factor.
        if(includeGlobalScaleFactor && radiusScaleFactor() != 1.0) {
            for(auto& r : BufferWriteAccess<GraphicsFloatType, access_mode::read_write>(output.makeMutableInplace()))
                r *= radiusScaleFactor();
        }
    }
    else {
        // Allocate output array.
        output.reset(Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::RadiusProperty));

        if(const Property* typeProperty = getParticleTypeRadiusProperty(particles)) {
            OVITO_ASSERT(typeProperty->size() == output->size());

            // Assign radii based on particle types.
            // Build a lookup map for particle type radii.
            std::map<int,FloatType> radiusMap = ParticleType::typeRadiusMap(typeProperty);
            // Skip the following loop if all per-type radii are zero. In this case, simply use the default radius for all particles.
            if(boost::algorithm::any_of(radiusMap, [](const std::pair<int,FloatType>& it) { return it.second != 0; })) {
                // Apply global scaling factor.
                if(includeGlobalScaleFactor && radiusScaleFactor() != 1.0) {
                    for(auto& p : radiusMap)
                        p.second *= radiusScaleFactor();
                }
                // Fill radius array.
                BufferReadAccess<int32_t> typeData(typeProperty);
                BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> radiusArray(output.makeMutableInplace());
                boost::transform(typeData, radiusArray.begin(), [&](auto t) {
                    // Set particle radius only if the type's radius is non-zero.
                    if(auto it = radiusMap.find(t); it != radiusMap.end() && it->second != 0)
                        return static_cast<GraphicsFloatType>(it->second);
                    else
                        return defaultRadius;
                });
            }
            else {
                // Assign the uniform default radius to all particles.
                output.makeMutableInplace()->fill<GraphicsFloatType>(defaultRadius);
            }
        }
        else {
            // Assign the uniform default radius to all particles.
            output.makeMutableInplace()->fill<GraphicsFloatType>(defaultRadius);
        }
    }

    return output;
}

/******************************************************************************
* Determines the display radius of a single particle.
******************************************************************************/
GraphicsFloatType ParticlesVis::particleRadius(size_t particleIndex, BufferReadAccess<GraphicsFloatType> radiusProperty, const Property* typeProperty) const
{
    OVITO_ASSERT(typeProperty == nullptr || typeProperty->typeId() == Particles::TypeProperty);

    if(radiusProperty && radiusProperty.size() > particleIndex) {
        // Take particle radius directly from the radius property.
        GraphicsFloatType r = radiusProperty[particleIndex];
        if(r > 0)
            return r * radiusScaleFactor();
    }
    else if(typeProperty && typeProperty->size() > particleIndex) {
        // Assign radius based on particle types.
        BufferReadAccess<int32_t> typeData(typeProperty);
        const ParticleType* ptype = static_object_cast<ParticleType>(typeProperty->elementType(typeData[particleIndex]));
        if(ptype && ptype->radius() > 0)
            return ptype->radius() * radiusScaleFactor();
    }

    return defaultParticleRadius() * radiusScaleFactor();
}

/******************************************************************************
* Determines the display color of a single particle.
******************************************************************************/
ColorG ParticlesVis::particleColor(size_t particleIndex, BufferReadAccess<ColorG> colorProperty, const Property* typeProperty, BufferReadAccess<SelectionIntType> selectionProperty) const
{
    // Check if particle is selected.
    if(selectionProperty && selectionProperty.size() > particleIndex) {
        if(selectionProperty[particleIndex])
            return selectionParticleColor().toDataType<GraphicsFloatType>();
    }

    ColorG c = defaultParticleColor().toDataType<GraphicsFloatType>();
    if(colorProperty && colorProperty.size() > particleIndex) {
        // Take particle color directly from the color property.
        c = colorProperty[particleIndex];
    }
    else if(typeProperty && typeProperty->size() > particleIndex) {
        // Return color based on particle types.
        BufferReadAccess<int32_t> typeData(typeProperty);
        const ElementType* ptype = typeProperty->elementType(typeData[particleIndex]);
        if(ptype)
            c = ptype->color().toDataType<GraphicsFloatType>();
    }

    return c;
}

/******************************************************************************
* Returns the actual rendering quality used to render the particles.
******************************************************************************/
ParticlePrimitive::RenderingQuality ParticlesVis::effectiveRenderingQuality(bool isInteractiveRenderer, const Particles* particles) const
{
    ParticlePrimitive::RenderingQuality renderQuality = renderingQuality();
    if(renderQuality == ParticlePrimitive::AutoQuality) {
        if(!particles) return ParticlePrimitive::HighQuality;
        size_t particleCount = particles->elementCount();
        if(particleCount < 4000 || !isInteractiveRenderer)
            renderQuality = ParticlePrimitive::HighQuality;
        else if(particleCount < 400000)
            renderQuality = ParticlePrimitive::MediumQuality;
        else
            renderQuality = ParticlePrimitive::LowQuality;
    }
    return renderQuality;
}

/******************************************************************************
* Returns the effective primitive shape for rendering the particles.
******************************************************************************/
ParticlePrimitive::ParticleShape ParticlesVis::effectiveParticleShape(ParticleShape shape, const Property* shapeProperty, const Property* orientationProperty, const Property* roundnessProperty)
{
    if(shape == Sphere) {
        if(roundnessProperty != nullptr) return ParticlePrimitive::SuperquadricShape;
        if(shapeProperty != nullptr) return ParticlePrimitive::EllipsoidShape;
        else return ParticlePrimitive::SphericalShape;
    }
    else if(shape == Box) {
        if(shapeProperty != nullptr || orientationProperty != nullptr) return ParticlePrimitive::BoxShape;
        else return ParticlePrimitive::SquareCubicShape;
    }
    else if(shape == Circle) {
        return ParticlePrimitive::SphericalShape;
    }
    else if(shape == Square) {
        return ParticlePrimitive::SquareCubicShape;
    }
    else {
        OVITO_ASSERT(false);
        return ParticlePrimitive::SphericalShape;
    }
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> ParticlesVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode)
{
    // Get input particle data.
    DataOORef<const Particles> particles = path.lastAs<Particles>();
    if(!particles)
        return {};
    particles->verifyIntegrity();

    // Make sure the 'Position' property is present.
    if(!particles->getProperty(Particles::PositionProperty))
        throw Exception(tr("Cannot display particles because the 'Position' property is not present."));

    // Make sure we don't exceed the internal limits. Rendering of more than 2 billion particles is not yet supported by OVITO.
    size_t particleCount = particles->elementCount();
    if(particleCount > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("This version of OVITO doesn't support rendering of more than %1 particles.").arg(std::numeric_limits<int>::max()));
    }

    // Perform rendering in a background thread to not block the GUI for too long.
    return asyncLaunch([
            this,
            self=OORef<ParticlesVis>(this), // To keep this vis element alive while rendering.
            &commandGroup=frameGraph.addCommandGroup(FrameGraph::SceneLayer),
            frameGraph=OORef<FrameGraph>(&frameGraph),
            sceneNode=OORef<const SceneNode>(sceneNode),
            particles=std::move(particles),
            tm=sceneNode->getWorldTransform(frameGraph.time())
        ]() -> PipelineStatus {

        // Render all mesh-based particle types.
        renderMeshBasedParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

        // Render all primitive particle types.
        renderPrimitiveParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

        // Render all (sphero-)cylindric particle types.
        renderCylindricParticles(particles, *frameGraph, commandGroup, sceneNode, tm);

        return {};
    });
}

/******************************************************************************
* Renders particle types that have a mesh-based shape assigned.
******************************************************************************/
void ParticlesVis::renderMeshBasedParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    if(!positionProperty || !typeProperty)
        return;

    // Compile list of particle types that have a mesh geometry assigned.
    QVarLengthArray<int, 10> shapeMeshParticleTypes;
    for(const ElementType* etype : typeProperty->elementTypes()) {
        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
            if(ptype->shape() == ParticlesVis::ParticleShape::Mesh && ptype->shapeMesh() && ptype->shapeMesh()->faceCount() != 0) {
                shapeMeshParticleTypes.push_back(ptype->numericId());
            }
        }
    }
    if(shapeMeshParticleTypes.empty())
        return;

    // The type of lookup key used for caching the mesh rendering primitives:
    using ShapeMeshCacheKey = RendererResourceKey<struct ParticlesVisMeshCache,
        ConstDataObjectRef,         // Particle type property
        FloatType,                  // Default particle radius
        FloatType,                  // Global radius scaling factor
        ConstDataObjectRef,         // Position property
        ConstDataObjectRef,         // Orientation property
        ConstDataObjectRef,         // Color property
        ConstDataObjectRef,         // Selection property
        ConstDataObjectRef,         // Transparency property
        ConstDataObjectRef          // Radius property
    >;

    // The data structure created for each mesh-based particle type.
    struct MeshParticleType {
        MeshPrimitive meshPrimitive;
        OORef<ParticlePickInfo> pickInfo;
        bool useMeshColors; ///< Controls the use of the original face colors from the mesh instead of the per-particle colors.
    };

    // Look up the rendering primitives for mesh-based particle types in the vis cache.
    const std::vector<MeshParticleType>& meshVisCache = frameGraph.visCache().lookup<std::vector<MeshParticleType>>(
        ShapeMeshCacheKey{
            typeProperty,
            defaultParticleRadius(),
            radiusScaleFactor(),
            positionProperty,
            orientationProperty,
            colorProperty,
            selectionProperty,
            transparencyProperty,
            radiusProperty},
        [&](std::vector<MeshParticleType>& meshVisCache) {

            // This data structure stores temporary per-particle instance data, separated by mesh-based particle type.
            struct MeshTypePerInstanceData {
                BufferFactory<AffineTransformationG> particleTMs{0};   /// AffineTransformation of each particle to be rendered.
                BufferFactory<ColorAG> particleColors{0};  /// Color of each particle to be rendered.
                BufferFactory<int32_t> particleIndices{0}; /// Index of each particle to be rendered in the original particles list.
            };
            std::vector<MeshTypePerInstanceData> perInstanceData;

            meshVisCache.reserve(shapeMeshParticleTypes.size());
            perInstanceData.reserve(shapeMeshParticleTypes.size());

            // Create one instanced mesh primitive for each mesh-based particle type.
            for(int typeId : shapeMeshParticleTypes) {
                // Create a new instanced mesh primitive for the particle type.
                const ParticleType* ptype = static_object_cast<ParticleType>(typeProperty->elementType(typeId));
                OVITO_ASSERT(ptype->shapeMesh());
                MeshParticleType meshType;
                meshType.meshPrimitive.setEmphasizeEdges(ptype->highlightShapeEdges());
                meshType.meshPrimitive.setCullFaces(ptype->shapeBackfaceCullingEnabled());
                meshType.meshPrimitive.setMesh(ptype->shapeMesh());
                meshType.useMeshColors = ptype->shapeUseMeshColor();
                meshVisCache.push_back(std::move(meshType));
                perInstanceData.emplace_back();
            }

            // Compile the per-instance particle data (positions, orientations, colors, etc) for each mesh-based particle type.
            BufferReadAccessAndRef<ColorG> colors = particleColors(particles, frameGraph.isInteractive());
            BufferReadAccessAndRef<GraphicsFloatType> radii = particleRadii(particles, true);
            BufferReadAccess<int32_t> types(typeProperty);
            BufferReadAccess<Point3> positions(positionProperty);
            BufferReadAccess<QuaternionG> orientations(orientationProperty);
            BufferReadAccess<GraphicsFloatType> transparencies(transparencyProperty);
            size_t particleCount = particles->elementCount();
            for(size_t i = 0; i < particleCount; i++) {
                if(radii[i] <= 0)
                    continue;
                auto iter = boost::find(shapeMeshParticleTypes, types[i]);
                if(iter == shapeMeshParticleTypes.end())
                    continue;
                size_t typeIndex = std::distance(shapeMeshParticleTypes.begin(), iter);
                AffineTransformationG tm = AffineTransformationG::scaling(radii[i]);
                if(positions)
                    tm.translation() = positions[i].toDataType<GraphicsFloatType>() - Point3G::Origin();
                if(orientations)
                    tm = tm * Matrix_3<GraphicsFloatType>::rotation(orientations[i].safelyNormalized());
                perInstanceData[typeIndex].particleTMs.push_back(tm);
                perInstanceData[typeIndex].particleColors.push_back(ColorAG(colors[i], transparencies ? qBound<GraphicsFloatType>(0, 1 - transparencies[i], 1) : 1));
                perInstanceData[typeIndex].particleIndices.push_back(i);
            }

            // Store the per-particle data into the mesh rendering primitives.
            for(size_t typeIndex = 0; typeIndex < meshVisCache.size(); typeIndex++) {
                if(meshVisCache[typeIndex].useMeshColors)
                    perInstanceData[typeIndex].particleColors.reset();
                meshVisCache[typeIndex].meshPrimitive.setInstancedRendering(
                    perInstanceData[typeIndex].particleTMs.take(),
                    perInstanceData[typeIndex].particleColors.take());
                // Create a picking structure for this set of particles.
                meshVisCache[typeIndex].pickInfo = OORef<ParticlePickInfo>::create(this, particles, perInstanceData[typeIndex].particleIndices.take());
            }
        });

    OVITO_ASSERT(meshVisCache.size() == shapeMeshParticleTypes.size());

    // Render the instanced mesh primitives, one for each particle type with a mesh-based shape.
    for(const MeshParticleType& t : meshVisCache) {

        // Update the pick info record with the latest particle data.
        t.pickInfo->setParticles(particles);

        // Add the instanced mesh primitive to the frame graph.
        commandGroup.addPrimitive(std::make_unique<MeshPrimitive>(t.meshPrimitive), tm, t.meshPrimitive.computeBoundingBox(frameGraph.visCache()), sceneNode, t.pickInfo);
    }
}

/******************************************************************************
* Renders all particles with a primitive shape (spherical, box, (super)quadrics).
******************************************************************************/
void ParticlesVis::renderPrimitiveParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Determine whether all particle types use the same uniform shape or not.
    ParticlesVis::ParticleShape uniformShape = particleShape();
    OVITO_ASSERT(uniformShape != ParticleShape::Default);
    if(uniformShape == ParticleShape::Default)
        return;
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                ParticleShape ptypeShape = ptype->shape();
                if(ptypeShape == ParticleShape::Default)
                    ptypeShape = particleShape();
                if(ptypeShape != uniformShape) {
                    uniformShape = ParticleShape::Default; // This value indicates that particles do NOT all use one uniform shape.
                    break;
                }
            }
        }
    }

    // Quit early if all particles have a shape not handled by this method.
    if(uniformShape != ParticleShape::Default) {
        if(uniformShape != ParticleShape::Sphere &&
            uniformShape != ParticleShape::Box &&
            uniformShape != ParticleShape::Circle &&
            uniformShape != ParticleShape::Square)
            return;
    }

    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* typeRadiusProperty = getParticleTypeRadiusProperty(particles);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* asphericalShapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    const Property* roundnessProperty = particles->getProperty(Particles::SuperquadricRoundnessProperty);
    if(!positionProperty)
        return;

    // Prepare the particle rendering primitive.
    ParticlePrimitive primitive;

    // Pick render quality level adaptively based on current number of particles.
    ParticlePrimitive::RenderingQuality primitiveRenderQuality = effectiveRenderingQuality(frameGraph.isInteractive(), particles);
    primitive.setRenderingQuality(primitiveRenderQuality);

    // Fill rendering primitive with particle properties.
    primitive.setPositions(positionProperty);
    primitive.setTransparencies(transparencyProperty);
    primitive.setSelection(selectionProperty);
    primitive.setOrientations(orientationProperty);
    primitive.setRoundness(roundnessProperty);
    primitive.setSelectionColor(selectionParticleColor());

    // Aspherical shape arrays mau require extra work, because it is affected by the uniform particle scaling factor.
    if(radiusScaleFactor() == 1.0 || !asphericalShapeProperty) {
        primitive.setAsphericalShapes(asphericalShapeProperty);
    }
    else {
        // The lookup key for the cached particle indices for the current shape type:
        using ParticleShapeCacheKey = RendererResourceKey<struct ParticlesVisShapeCache,
            ConstDataObjectRef,     // Aspherical shape property
            FloatType               // Scaling factor
        >;
        // Look up the scaled aspherical shape array in the vis cache, which have been multiplied with the uniform scaling factor.
        const ConstDataBufferPtr& scaledShapes = frameGraph.visCache().lookup<ConstDataBufferPtr>(
            ParticleShapeCacheKey(asphericalShapeProperty, radiusScaleFactor()),
            [&](ConstDataBufferPtr& scaledShapes) {
                // Make a copy of the original aspherical shape array and multiply all vectors with the scaling factor.
                BufferWriteAccessAndRef<Vector3G, access_mode::read_write> values = ConstDataBufferPtr::makeCopy(asphericalShapeProperty);
                for(Vector3G& s : values)
                    s *= radiusScaleFactor();
                scaledShapes = values.take();
            });
        primitive.setAsphericalShapes(scaledShapes);
    }

    // Create separate rendering primitives for the different shapes supported by the method.
    for(ParticlesVis::ParticleShape shape : {ParticleShape::Sphere, ParticleShape::Box, ParticleShape::Circle, ParticleShape::Square}) {

        // Skip this shape if all particles are known to have another shape.
        if(uniformShape != ParticleShape::Default && uniformShape != shape)
            continue;

        // The lookup key for the cached particle indices for the current shape type:
        using ParticleCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveCache,
            ConstDataObjectRef,                 // Particle type property
            ParticlesVis::ParticleShape,        // Current particle shape
            ParticlesVis::ParticleShape,        // Global particle shape
            size_t                              // Particle count
        >;

        // Look up the particle indices in the vis cache.
        const ConstDataBufferPtr& indices = frameGraph.visCache().lookup<ConstDataBufferPtr>(
            ParticleCacheKey(
                typeProperty,
                shape,
                uniformShape,
                particles->elementCount()),
            [&](ConstDataBufferPtr& indices) {
                // Determine the set of particles to be rendered using the current primitive shape.
                if(uniformShape != shape) {
                    OVITO_ASSERT(typeProperty);

                    // Build list of type IDs that use the current shape.
                    std::vector<int> activeParticleTypes;
                    for(const ElementType* etype : typeProperty->elementTypes()) {
                        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                            if(ptype->shape() == shape || (ptype->shape() == ParticleShape::Default && shape == particleShape()) || (ptype->shape() == ParticleShape::Mesh && !ptype->shapeMesh() && shape == ParticleShape::Box))
                                activeParticleTypes.push_back(ptype->numericId());
                        }
                    }

                    // Collect indices of all particles that have an active type.
                    BufferFactory<int32_t> activeParticleIndices(0);
                    size_t index = 0;
                    for(int32_t t : BufferReadAccess<int32_t>(typeProperty)) {
                        if(boost::find(activeParticleTypes, t) != activeParticleTypes.cend())
                            activeParticleIndices.push_back(index);
                        index++;
                    }
                    indices = activeParticleIndices.take();
                }
            });

        if(indices && indices->size() == 0)
            continue;   // No particles to be rendered using the current primitive shape.

        // Enable/disable indexed rendering of particle primitives.
        primitive.setIndices(indices);

        if(!primitive.radii()) {
            // The type of lookup key used for caching the particle radii:
            using RadiiCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveRadiusCache,
                FloatType,                          // Default particle radius
                FloatType,                          // Global radius scaling factor
                ConstDataObjectRef,                 // Radius property
                ConstDataObjectRef,                 // Type property
                size_t                              // Particle count
            >;
            primitive.setRadii(frameGraph.visCache().lookup<ConstPropertyPtr>(
                RadiiCacheKey(
                    defaultParticleRadius(),
                    radiusScaleFactor(),
                    radiusProperty,
                    typeRadiusProperty,
                    particles->elementCount()),
                [&](ConstPropertyPtr& radiusBuffer) {
                    radiusBuffer = particleRadii(particles, true);
                }));
        }

        if(!primitive.colors()) {
            // The type of lookup key used for caching the particle colors:
            using ColorCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveColorCache,
                ConstDataObjectRef,                 // Type property
                ConstDataObjectRef,                 // Color property
                size_t                              // Particle count
            >;
            primitive.setColors(frameGraph.visCache().lookup<ConstPropertyPtr>(
                ColorCacheKey(
                    typeProperty,
                    colorProperty,
                    particles->elementCount()),
                [&](ConstPropertyPtr& colorBuffer) {
                    colorBuffer = particleColors(particles, false);
                }));
        }

        // Configure rendering shape and shading style.
        ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, asphericalShapeProperty, orientationProperty, roundnessProperty);
        ParticlePrimitive::ShadingMode primitiveShadingMode = (shape == Circle || shape == Square) ? ParticlePrimitive::FlatShading : ParticlePrimitive::NormalShading;
        primitive.setParticleShape(primitiveParticleShape);
        primitive.setShadingMode(primitiveShadingMode);

        // Look up or create the pick info record with the latest particle data.
        const auto& pickingInfo = frameGraph.visCache().lookup<OORef<ParticlePickInfo>>(
            ConstDataObjectRef(particles),
            [&](OORef<ParticlePickInfo>& pickingInfo) {
                pickingInfo = OORef<ParticlePickInfo>::create(this, particles);
            });

        // Add the particles primitive to the frame graph.
        commandGroup.addPrimitive(std::make_unique<ParticlePrimitive>(primitive), tm, primitive.computeBoundingBox(frameGraph.visCache()), sceneNode, pickingInfo);
    }
}

/******************************************************************************
* Renders all particles with a (sphero-)cylindrical shape.
******************************************************************************/
void ParticlesVis::renderCylindricParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const SceneNode* sceneNode, const AffineTransformation& tm) const
{
    // Determine whether all particle types use the same uniform shape or not.
    ParticlesVis::ParticleShape uniformShape = particleShape();
    OVITO_ASSERT(uniformShape != ParticleShape::Default);
    if(uniformShape == ParticleShape::Default)
        return;
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    if(typeProperty) {
        for(const ElementType* etype : typeProperty->elementTypes()) {
            if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                ParticleShape ptypeShape = ptype->shape();
                if(ptypeShape == ParticleShape::Default)
                    ptypeShape = particleShape();
                if(ptypeShape != uniformShape) {
                    uniformShape = ParticleShape::Default; // This value indicates that particles do NOT all use one uniform shape.
                    break;
                }
            }
        }
    }

    // Quit early if all particles have a shape not handled by this method.
    if(uniformShape != ParticleShape::Default) {
        if(uniformShape != ParticleShape::Cylinder &&
            uniformShape != ParticleShape::Spherocylinder)
            return;
    }

    // Get input particle data.
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* radiusProperty = particles->getProperty(Particles::RadiusProperty);
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* selectionProperty = frameGraph.isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* asphericalShapeProperty = particles->getProperty(Particles::AsphericalShapeProperty);
    const Property* orientationProperty = particles->getProperty(Particles::OrientationProperty);
    if(!positionProperty)
        return;

    ConstPropertyPtr colorBuffer;
    ConstPropertyPtr radiusBuffer;

    /// Create separate rendering primitives for the different shapes supported by the method.
    for(ParticlesVis::ParticleShape shape : {ParticleShape::Cylinder, ParticleShape::Spherocylinder}) {

        // Skip this shape if all particles are known to have a different shape.
        if(uniformShape != ParticleShape::Default && uniformShape != shape)
            continue;

        // The lookup key for the cached rendering primitive:
        using ParticleCacheKey = RendererResourceKey<struct ParticlesVisCylindersCache,
            ConstDataObjectRef,                 // Position property
            ConstDataObjectRef,                 // Type property
            ConstDataObjectRef,                 // Selection property
            ConstDataObjectRef,                 // Color property
            ConstDataObjectRef,                 // Transparency property
            ConstDataObjectRef,                 // Apherical shape property
            ConstDataObjectRef,                 // Orientation property
            ConstDataObjectRef,                 // Radius property
            FloatType,                          // Default particle radius
            FloatType,                          // Global radius scaling factor
            ParticlesVis::ParticleShape,        // Global particle shape
            ParticlesVis::ParticleShape         // Local particle shape
        >;

        // Look up the rendering primitive in the vis cache.
        const auto& [cylinderPrimitive, spheresPrimitives, pickInfo] = frameGraph.visCache().lookup<std::tuple<CylinderPrimitive, std::array<ParticlePrimitive, 2>, OORef<ParticlePickInfo>>>(
            ParticleCacheKey(
                positionProperty,
                typeProperty,
                selectionProperty,
                colorProperty,
                transparencyProperty,
                asphericalShapeProperty,
                orientationProperty,
                radiusProperty,
                defaultParticleRadius(),
                radiusScaleFactor(),
                particleShape(),
                shape),
            [&](CylinderPrimitive& cylinderPrimitive, std::array<ParticlePrimitive, 2>& spheresPrimitives, OORef<ParticlePickInfo>& pickInfo) {

                // Determine the set of particles to be rendered using the current shape.
                BufferFactory<int32_t> activeParticleIndices;
                if(uniformShape != shape) {
                    OVITO_ASSERT(typeProperty);

                    // Build list of type IDs that use the current shape.
                    std::vector<int> activeParticleTypes;
                    for(const ElementType* etype : typeProperty->elementTypes()) {
                        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                            if(ptype->shape() == shape || (ptype->shape() == ParticleShape::Default && shape == particleShape()))
                                activeParticleTypes.push_back(ptype->numericId());
                        }
                    }

                    // Collect indices of all particles that have an active type.
                    activeParticleIndices = BufferFactory<int32_t>(0);
                    size_t index = 0;
                    for(int32_t t : BufferReadAccess<int32_t>(typeProperty)) {
                        if(boost::find(activeParticleTypes, t) != activeParticleTypes.cend())
                            activeParticleIndices.push_back(index);
                        index++;
                    }

                    if(activeParticleIndices.size() == 0) {
                        return;   // No particles to be rendered using the current primitive shape.
                    }
                }
                int effectiveParticleCount = activeParticleIndices ? activeParticleIndices.size() : particles->elementCount();

                // Create the rendering primitive for the cylinders.
                cylinderPrimitive.setShape(CylinderPrimitive::CylinderShape);
                cylinderPrimitive.setShadingMode(CylinderPrimitive::NormalShading);

                // Determine cylinder colors.
                if(!colorBuffer)
                    colorBuffer = particleColors(particles, frameGraph.isInteractive());

                // Determine cylinder radii (only needed if aspherical shape property is not present).
                if(!radiusBuffer && !asphericalShapeProperty)
                    radiusBuffer = particleRadii(particles, false);

                // Allocate cylinder data buffers.
                BufferFactory<Point3G> cylinderBasePositions(effectiveParticleCount);
                BufferFactory<Point3G> cylinderHeadPositions(effectiveParticleCount);
                BufferFactory<GraphicsFloatType> cylinderWidths(effectiveParticleCount);
                BufferFactory<ColorG> cylinderColors(effectiveParticleCount);
                BufferFactory<GraphicsFloatType> cylinderTransparencies = transparencyProperty ? BufferFactory<GraphicsFloatType>(effectiveParticleCount) : BufferFactory<GraphicsFloatType>{};
                BufferFactory<GraphicsFloatType> sphereRadii = (shape == ParticleShape::Spherocylinder) ? BufferFactory<GraphicsFloatType>(effectiveParticleCount) : BufferFactory<GraphicsFloatType>{};

                // Fill data buffers.
                BufferReadAccess<Point3> positionArray(positionProperty);
                BufferReadAccess<Vector3G> asphericalShapeArray(asphericalShapeProperty);
                BufferReadAccess<QuaternionG> orientationArray(orientationProperty);
                BufferReadAccess<ColorG> colorsArray(colorBuffer);
                BufferReadAccess<GraphicsFloatType> radiiArray(radiusBuffer);
                BufferReadAccess<GraphicsFloatType> transparencies(transparencyProperty);
                const GraphicsFloatType scalingFactor = radiusScaleFactor();
                for(int index = 0; index < effectiveParticleCount; index++) {
                    int effectiveParticleIndex = activeParticleIndices ? activeParticleIndices[index] : index;
                    const Point3& center = positionArray[effectiveParticleIndex];
                    GraphicsFloatType radius, length;
                    if(asphericalShapeArray) {
                        radius = std::abs(asphericalShapeArray[effectiveParticleIndex].x()) * scalingFactor;
                        length = asphericalShapeArray[effectiveParticleIndex].z() * scalingFactor;
                    }
                    else {
                        radius = radiiArray[effectiveParticleIndex] * scalingFactor;
                        length = radius * 2;
                    }
                    Vector3G dir(0, 0, length);
                    if(orientationArray) {
                        dir = orientationArray[effectiveParticleIndex].safelyNormalized() * dir;
                    }
                    Point3G p = center.toDataType<GraphicsFloatType>() - (dir * GraphicsFloatType(0.5));
                    cylinderBasePositions[index] = p;
                    cylinderHeadPositions[index] = p + dir;
                    cylinderWidths[index] = 2 * radius;
                    cylinderColors[index] = colorsArray[effectiveParticleIndex];
                    if(cylinderTransparencies)
                        cylinderTransparencies[index] = transparencies[effectiveParticleIndex];
                    if(sphereRadii)
                        sphereRadii[index] = radius;
                }
                cylinderPrimitive.setPositions(cylinderBasePositions.take(), cylinderHeadPositions.take());
                cylinderPrimitive.setWidths(cylinderWidths.take());
                cylinderPrimitive.setColors(cylinderColors.take());
                cylinderPrimitive.setTransparencies(cylinderTransparencies.take());

                // Create the rendering primitive for the spheres.
                if(shape == ParticleShape::Spherocylinder) {
                    spheresPrimitives[0].setParticleShape(ParticlePrimitive::SphericalShape);
                    spheresPrimitives[0].setShadingMode(ParticlePrimitive::NormalShading);
                    spheresPrimitives[0].setRenderingQuality(ParticlePrimitive::HighQuality);
                    spheresPrimitives[0].setPositions(cylinderPrimitive.basePositions());
                    spheresPrimitives[0].setRadii(sphereRadii.take());
                    spheresPrimitives[0].setColors(cylinderPrimitive.colors());
                    spheresPrimitives[0].setTransparencies(cylinderPrimitive.transparencies());
                    spheresPrimitives[1].setParticleShape(ParticlePrimitive::SphericalShape);
                    spheresPrimitives[1].setShadingMode(ParticlePrimitive::NormalShading);
                    spheresPrimitives[1].setRenderingQuality(ParticlePrimitive::HighQuality);
                    spheresPrimitives[1].setPositions(cylinderPrimitive.headPositions());
                    spheresPrimitives[1].setRadii(spheresPrimitives[0].radii());
                    spheresPrimitives[1].setColors(cylinderPrimitive.colors());
                    spheresPrimitives[1].setTransparencies(cylinderPrimitive.transparencies());
                }

                // Also create the corresponding picking record.
                pickInfo = OORef<ParticlePickInfo>::create(this, particles, activeParticleIndices.take());
            });

        if(!pickInfo)
            continue;

        // Update the pick info record with the latest particle data.
        pickInfo->setParticles(particles);

        // Add the cylinders to the frame graph.
        commandGroup.addPrimitive(std::make_unique<CylinderPrimitive>(cylinderPrimitive), tm, cylinderPrimitive.computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);

        // Render the spherical caps.
        if(spheresPrimitives[0].positions()) {
            commandGroup.addPrimitive(std::make_unique<ParticlePrimitive>(spheresPrimitives[0]), tm, spheresPrimitives[0].computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);
            commandGroup.addPrimitive(std::make_unique<ParticlePrimitive>(spheresPrimitives[1]), tm, spheresPrimitives[1].computeBoundingBox(frameGraph.visCache()), sceneNode, pickInfo);
        }
    }
}

/******************************************************************************
* Render a marker around a particle to highlight it in the viewports.
******************************************************************************/
void ParticlesVis::highlightParticle(size_t particleIndex, const Particles* particles, FrameGraph& frameGraph, const SceneNode* sceneNode) const
{
    // Fetch properties of selected particle which are needed to render the overlay.
    const Property* posProperty = nullptr;
    const Property* radiusProperty = nullptr;
    const Property* colorProperty = nullptr;
    const Property* selectionProperty = nullptr;
    const Property* shapeProperty = nullptr;
    const Property* orientationProperty = nullptr;
    const Property* roundnessProperty = nullptr;
    const Property* typeProperty = nullptr;
    for(const Property* property : particles->properties()) {
        if(property->typeId() == Particles::PositionProperty && property->size() >= particleIndex)
            posProperty = property;
        else if(property->typeId() == Particles::RadiusProperty && property->size() >= particleIndex)
            radiusProperty = property;
        else if(property->typeId() == Particles::TypeProperty && property->size() >= particleIndex)
            typeProperty = property;
        else if(property->typeId() == Particles::ColorProperty && property->size() >= particleIndex)
            colorProperty = property;
        else if(property->typeId() == Particles::SelectionProperty && property->size() >= particleIndex)
            selectionProperty = property;
        else if(property->typeId() == Particles::AsphericalShapeProperty && property->size() >= particleIndex)
            shapeProperty = property;
        else if(property->typeId() == Particles::OrientationProperty && property->size() >= particleIndex)
            orientationProperty = property;
        else if(property->typeId() == Particles::SuperquadricRoundnessProperty && property->size() >= particleIndex)
            roundnessProperty = property;
    }
    if(!posProperty || particleIndex >= posProperty->size())
        return;

    // Get the particle type.
    const ParticleType* ptype = nullptr;
    if(typeProperty && particleIndex < typeProperty->size()) {
        BufferReadAccess<int32_t> typeArray(typeProperty);
        ptype = dynamic_object_cast<ParticleType>(typeProperty->elementType(typeArray[particleIndex]));
    }

    // Check if the particle must be rendered using a custom shape.
    if(ptype && ptype->shape() == ParticleShape::Mesh && ptype->shapeMesh())
        return; // Note: Highlighting of particles with user-defined shapes is not implemented yet.

    // The rendering shape of the highlighted particle.
    ParticleShape shape = particleShape();
    if(ptype && ptype->shape() != ParticleShape::Default)
        shape = ptype->shape();

    // Determine position of the selected particle.
    Point3 pos = BufferReadAccess<Point3>(posProperty)[particleIndex];

    // Determine radius of selected particle.
    GraphicsFloatType radius = particleRadius(particleIndex, radiusProperty, typeProperty);

    // Get world transformation matrix of scene node.
    const AffineTransformation& nodeTM = sceneNode->getWorldTransform(frameGraph.time());

    FloatType padding = frameGraph.nonScalingSize(nodeTM * pos) * FloatType(1e-1);

    // Determine the display color of selected particle.
    ColorG color = particleColor(particleIndex, colorProperty, typeProperty, selectionProperty);
    ColorG highlightColor = selectionParticleColor().toDataType<GraphicsFloatType>();
    color = color * GraphicsFloatType(0.5) + highlightColor * GraphicsFloatType(0.5);

    // Determine rendering quality used to render the particles.
    ParticlePrimitive::RenderingQuality renderQuality = effectiveRenderingQuality(frameGraph.isInteractive(), particles);

    std::unique_ptr<ParticlePrimitive> particleBuffer;
    std::unique_ptr<ParticlePrimitive> highlightParticleBuffer;
    std::unique_ptr<CylinderPrimitive> cylinderBuffer;
    std::unique_ptr<CylinderPrimitive> highlightCylinderBuffer;
    if(shape != Cylinder && shape != Spherocylinder) {
        // Determine effective particle shape and shading mode.
        ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, shapeProperty, orientationProperty, roundnessProperty);
        ParticlePrimitive::ShadingMode primitiveShadingMode = ParticlePrimitive::NormalShading;
        if(shape == ParticlesVis::Circle || shape == ParticlesVis::Square)
            primitiveShadingMode = ParticlePrimitive::FlatShading;

        // Prepare data buffers.
        BufferFactory<Point3> positionBuffer(1);
        positionBuffer[0] = pos;
        BufferFactory<Vector3G> asphericalShapeBuffer;
        BufferFactory<Vector3G> asphericalShapeBufferHighlight;
        if(shapeProperty) {
            asphericalShapeBuffer = BufferFactory<Vector3G>(1);
            asphericalShapeBufferHighlight = BufferFactory<Vector3G>(1);
            const Vector3G shape = BufferReadAccess<Vector3G>(shapeProperty)[particleIndex];
            asphericalShapeBuffer[0] = shape * radiusScaleFactor();
            asphericalShapeBufferHighlight[0] = shape * radiusScaleFactor() + Vector3G(padding);
        }
        BufferFactory<QuaternionG> orientationBuffer;
        if(orientationProperty) {
            orientationBuffer = BufferFactory<QuaternionG>(1);
            orientationBuffer[0] = BufferReadAccess<QuaternionG>(orientationProperty)[particleIndex];
        }
        BufferFactory<Vector_2<GraphicsFloatType>> roundnessBuffer;
        if(roundnessProperty) {
            roundnessBuffer = BufferFactory<Vector_2<GraphicsFloatType>>(1);
            roundnessBuffer[0] = BufferReadAccess<Vector_2<GraphicsFloatType>>(roundnessProperty)[particleIndex];
        }

        particleBuffer = std::make_unique<ParticlePrimitive>();
        particleBuffer->setParticleShape(primitiveParticleShape);
        particleBuffer->setShadingMode(primitiveShadingMode);
        particleBuffer->setRenderingQuality(renderQuality);
        particleBuffer->setUniformColor(color.toDataType<FloatType>());
        particleBuffer->setPositions(positionBuffer.take());
        particleBuffer->setUniformRadius(radius);
        particleBuffer->setAsphericalShapes(asphericalShapeBuffer.take());
        particleBuffer->setOrientations(orientationBuffer.take());
        particleBuffer->setRoundness(roundnessBuffer.take());

        highlightParticleBuffer = std::make_unique<ParticlePrimitive>();
        highlightParticleBuffer->setParticleShape(primitiveParticleShape);
        highlightParticleBuffer->setShadingMode(primitiveShadingMode);
        highlightParticleBuffer->setRenderingQuality(renderQuality);
        highlightParticleBuffer->setUniformColor(highlightColor.toDataType<FloatType>());
        highlightParticleBuffer->setPositions(particleBuffer->positions());
        highlightParticleBuffer->setUniformRadius(radius + padding);
        highlightParticleBuffer->setAsphericalShapes(asphericalShapeBufferHighlight.take());
        highlightParticleBuffer->setOrientations(particleBuffer->orientations());
        highlightParticleBuffer->setRoundness(particleBuffer->roundness());
    }
    else if(shape == Cylinder || shape == Spherocylinder) {
        GraphicsFloatType radius, length;
        if(shapeProperty) {
            Vector3G shape = BufferReadAccess<Vector3G>(shapeProperty)[particleIndex] * radiusScaleFactor();
            radius = std::abs(shape.x());
            length = shape.z();
        }
        else {
            radius = defaultParticleRadius() * radiusScaleFactor();
            length = radius * 2;
        }
        Vector3G dir(0, 0, length);
        if(orientationProperty) {
            QuaternionG q = BufferReadAccess<QuaternionG>(orientationProperty)[particleIndex];
            dir = q.safelyNormalized() * dir;
        }
        BufferFactory<Point3G> positionBuffer1(1);
        BufferFactory<Point3G> positionBuffer2(1);
        BufferFactory<Point3G> positionBufferSpheres(2);
        positionBufferSpheres[0] = positionBuffer1[0] = pos.toDataType<GraphicsFloatType>() - (dir * GraphicsFloatType(0.5));
        positionBufferSpheres[1] = positionBuffer2[0] = pos.toDataType<GraphicsFloatType>() + (dir * GraphicsFloatType(0.5));
        cylinderBuffer = std::make_unique<CylinderPrimitive>();
        cylinderBuffer->setShape(CylinderPrimitive::CylinderShape);
        cylinderBuffer->setShadingMode(CylinderPrimitive::NormalShading);
        highlightCylinderBuffer = std::make_unique<CylinderPrimitive>();
        highlightCylinderBuffer->setShape(CylinderPrimitive::CylinderShape);
        highlightCylinderBuffer->setShadingMode(CylinderPrimitive::NormalShading);
        cylinderBuffer->setUniformColor(color.toDataType<FloatType>());
        cylinderBuffer->setUniformWidth(2 * radius);
        cylinderBuffer->setPositions(positionBuffer1.take(), positionBuffer2.take());
        highlightCylinderBuffer->setUniformColor(highlightColor.toDataType<FloatType>());
        highlightCylinderBuffer->setUniformWidth(2 * (radius + padding));
        highlightCylinderBuffer->setPositions(cylinderBuffer->basePositions(), cylinderBuffer->headPositions());
        if(shape == Spherocylinder) {
            particleBuffer = std::make_unique<ParticlePrimitive>();
            particleBuffer->setParticleShape(ParticlePrimitive::SphericalShape);
            particleBuffer->setShadingMode(ParticlePrimitive::NormalShading);
            particleBuffer->setRenderingQuality(ParticlePrimitive::HighQuality);
            highlightParticleBuffer = std::make_unique<ParticlePrimitive>();
            highlightParticleBuffer->setParticleShape(ParticlePrimitive::SphericalShape);
            highlightParticleBuffer->setShadingMode(ParticlePrimitive::NormalShading);
            highlightParticleBuffer->setRenderingQuality(ParticlePrimitive::HighQuality);
            particleBuffer->setPositions(positionBufferSpheres.take());
            particleBuffer->setUniformRadius(radius);
            particleBuffer->setUniformColor(color.toDataType<FloatType>());
            highlightParticleBuffer->setPositions(particleBuffer->positions());
            highlightParticleBuffer->setUniformRadius(radius + padding);
            highlightParticleBuffer->setUniformColor(highlightColor.toDataType<FloatType>());
        }
    }

    FrameGraph::RenderingCommandGroup& commandGroup1 = frameGraph.addCommandGroup(FrameGraph::HighlightLayer1);
    FrameGraph::RenderingCommandGroup& commandGroup2 = frameGraph.addCommandGroup(FrameGraph::HighlightLayer2);

    if(particleBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup1, std::move(particleBuffer), sceneNode);
    if(cylinderBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup1, std::move(cylinderBuffer), sceneNode);

    if(highlightParticleBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup2, std::move(highlightParticleBuffer), sceneNode);
    if(highlightCylinderBuffer)
        frameGraph.addPrimitiveNonpickable(commandGroup2, std::move(highlightCylinderBuffer), sceneNode);
}

/******************************************************************************
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding particle index.
******************************************************************************/
size_t ParticlePickInfo::particleIndexFromSubObjectID(uint32_t subobjID) const
{
    if(_subobjectToParticleMapping && subobjID < _subobjectToParticleMapping->size())
        return BufferReadAccess<int32_t>(_subobjectToParticleMapping)[subobjID];
    return subobjID;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString ParticlePickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    size_t particleIndex = particleIndexFromSubObjectID(subobjectId);
    return particles()->elementInfoString(particleIndex);
}

}   // End of namespace
