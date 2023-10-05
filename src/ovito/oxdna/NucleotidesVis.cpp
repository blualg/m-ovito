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
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "NucleotidesVis.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(NucleotidesVis);
DEFINE_PROPERTY_FIELD(NucleotidesVis, cylinderRadius);
SET_PROPERTY_FIELD_LABEL(NucleotidesVis, cylinderRadius, "Cylinder radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(NucleotidesVis, cylinderRadius, WorldParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
NucleotidesVis::NucleotidesVis(ObjectInitializationFlags flags) : ParticlesVis(flags),
    _cylinderRadius(0.05)
{
    setDefaultParticleRadius(0.1);
}

/******************************************************************************
* Computes the bounding box of the visual element.
******************************************************************************/
Box3 NucleotidesVis::boundingBox(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval)
{
    const Particles* particles = path.lastAs<Particles>();
    if(!particles) return {};
    particles->verifyIntegrity();
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    const Property* nucleotideAxisProperty = particles->getProperty(Particles::NucleotideAxisProperty);

    // The key type used for caching the computed bounding box:
    using CacheKey = std::tuple<
        ConstDataObjectRef, // Position property
        ConstDataObjectRef, // Nucleotide axis property
        FloatType           // Default particle radius
    >;

    // Look up the bounding box in the vis cache.
    auto& bbox = visCache.get<Box3>(CacheKey(
            positionProperty,
            nucleotideAxisProperty,
            defaultParticleRadius()));

    // Check if the cached bounding box information is still up to date.
    if(bbox.isEmpty()) {

        // If not, recompute bounding box from particle data.
        Box3 innerBox;
        if(BufferReadAccess<Point3> positionArray = positionProperty) {
            innerBox.addPoints(positionArray);
            if(BufferReadAccess<Vector3> axisArray = nucleotideAxisProperty) {
                const Vector3* axis = axisArray.cbegin();
                for(const Point3& p : positionArray) {
                    innerBox.addPoint(p + (*axis++));
                }
            }
        }

        // Extend box to account for radii/shape of particles.
        FloatType maxAtomRadius = defaultParticleRadius();

        // Extend the bounding box by the largest particle radius.
        bbox = innerBox.padBox(std::max(maxAtomRadius * sqrt(FloatType(3)), FloatType(0)));
    }
    return bbox;
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering colors
* of particles (if no per-particle colors are defined).
******************************************************************************/
const Property* NucleotidesVis::getParticleTypeColorProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::DNAStrandProperty);
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering radii
* of particles (if no per-particle radii are defined).
******************************************************************************/
const Property* NucleotidesVis::getParticleTypeRadiusProperty(const Particles* particles) const
{
    return particles->getProperty(Particles::TypeProperty);
}

/******************************************************************************
* Determines the effective rendering colors for the backbone sites of the nucleotides.
******************************************************************************/
ConstPropertyPtr NucleotidesVis::backboneColors(const Particles* particles, bool highlightSelection) const
{
    return particleColors(particles, highlightSelection);
}

/******************************************************************************
* Determines the effective rendering colors for the base sites of the nucleotides.
******************************************************************************/
ConstPropertyPtr NucleotidesVis::nucleobaseColors(const Particles* particles, bool highlightSelection) const
{
    particles->verifyIntegrity();

    // Allocate output color array.
    PropertyPtr output = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::ColorProperty);

    ColorG defaultColor = defaultParticleColor().toDataType<GraphicsFloatType>();
    if(const Property* baseProperty = particles->getProperty(Particles::NucleobaseTypeProperty)) {
        // Assign colors based on base type.
        // Generate a lookup map for base type colors.
        const std::map<int,Color> colorMap = baseProperty->typeColorMap();
        std::array<ColorG,16> colorArray;
        // Check if all type IDs are within a small, non-negative range.
        // If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
        if(std::all_of(colorMap.begin(), colorMap.end(), [&colorArray](const auto& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
            colorArray.fill(defaultColor);
            for(const auto& entry : colorMap)
                colorArray[entry.first] = entry.second.toDataType<GraphicsFloatType>();
            // Fill color array.
            BufferReadAccess<int32_t> typeArray(baseProperty);
            const auto* t = typeArray.cbegin();
            for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output)) {
                if(*t >= 0 && *t < (int)colorArray.size())
                    c = colorArray[*t];
                else
                    c = defaultColor;
                ++t;
            }
        }
        else {
            // Fill color array.
            BufferReadAccess<int32_t> typeArray(baseProperty);
            const auto* t = typeArray.cbegin();
            for(auto& c : BufferWriteAccess<ColorG, access_mode::discard_write>(output)) {
                auto it = colorMap.find(*t);
                if(it != colorMap.end())
                    c = it->second.toDataType<GraphicsFloatType>();
                else
                    c = defaultColor;
                ++t;
            }
        }
    }
    else {
        // Assign a uniform color to all base sites.
        output->fill<ColorG>(defaultColor);
    }

    // Highlight selected sites.
    if(const Property* selectionProperty = highlightSelection ? particles->getProperty(Particles::SelectionProperty) : nullptr)
        output->fillSelected<ColorG>(selectionParticleColor().toDataType<GraphicsFloatType>(), *selectionProperty);

    return output;
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus NucleotidesVis::render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const Pipeline* pipeline)
{
    if(renderer->isBoundingBoxPass()) {
        TimeInterval validityInterval;
        renderer->addToLocalBoundingBox(boundingBox(time, path, pipeline, flowState, renderer->visCache(), validityInterval));
        return {};
    }

    // Get input data.
    const Particles* particles = path.lastAs<Particles>();
    if(!particles) return {};
    particles->verifyIntegrity();
    const Property* positionProperty = particles->getProperty(Particles::PositionProperty);
    if(!positionProperty) return {};
    const Property* colorProperty = particles->getProperty(Particles::ColorProperty);
    const Property* baseProperty = particles->getProperty(Particles::NucleobaseTypeProperty);
    const Property* strandProperty = particles->getProperty(Particles::DNAStrandProperty);
    const Property* selectionProperty = renderer->isInteractive() ? particles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* transparencyProperty = particles->getProperty(Particles::TransparencyProperty);
    const Property* nucleotideAxisProperty = particles->getProperty(Particles::NucleotideAxisProperty);
    const Property* nucleotideNormalProperty = particles->getProperty(Particles::NucleotideNormalProperty);

    // Make sure we don't exceed our internal limits.
    if(particles->elementCount() > (size_t)std::numeric_limits<int>::max()) {
        throw Exception(tr("Cannot render more than %1 nucleotides.").arg(std::numeric_limits<int>::max()));
    }

    // The type of lookup key used for caching the rendering primitives:
    using NucleotidesCacheKey = RendererResourceKey<struct NucleotidesVisCache,
        QPointer<Pipeline>,         // Pipeline scene node
        ConstDataObjectRef,         // Position property
        ConstDataObjectRef,         // Color property
        ConstDataObjectRef,         // Strand property
        ConstDataObjectRef,         // Transparency property
        ConstDataObjectRef,         // Selection property
        ConstDataObjectRef,         // Nucleotide axis property
        ConstDataObjectRef,         // Nucleotide normal property
        FloatType,                  // Default particle radius
        FloatType                   // Cylinder radius
    >;

    // The data structure stored in the vis cache.
    struct NucleotidesCacheValue {
        ParticlePrimitive backbonePrimitive;
        CylinderPrimitive connectionPrimitive;
        ParticlePrimitive basePrimitive;
        OORef<ParticlePickInfo> pickInfo;
    };

    // Look up the rendering primitives in the vis cache.
    auto& visCache = renderer->visCache().get<NucleotidesCacheValue>(NucleotidesCacheKey(
        const_cast<Pipeline*>(pipeline),
        positionProperty,
        colorProperty,
        strandProperty,
        transparencyProperty,
        selectionProperty,
        nucleotideAxisProperty,
        nucleotideNormalProperty,
        defaultParticleRadius(),
        cylinderRadius()));

    // Check if we already have valid rendering primitives that are up to date.
    if(!visCache.backbonePrimitive.positions()) {

        // Create the rendering primitive for the backbone sites.
        visCache.backbonePrimitive.setShadingMode(ParticlePrimitive::NormalShading);
        visCache.backbonePrimitive.setRenderingQuality(ParticlePrimitive::MediumQuality);

        // Fill in the position data.
        visCache.backbonePrimitive.setPositions(positionProperty);

        // Fill in the transparency data.
        visCache.backbonePrimitive.setTransparencies(transparencyProperty);

        // Compute the effective color of each particle.
        ConstPropertyPtr colors = backboneColors(particles, renderer->isInteractive());

        // Fill in backbone color data.
        visCache.backbonePrimitive.setColors(colors);

        // Assign a uniform radius to all particles.
        visCache.backbonePrimitive.setUniformRadius(defaultParticleRadius());

        if(nucleotideAxisProperty) {
            // Create the rendering primitive for the base sites.
            visCache.basePrimitive.setParticleShape(ParticlePrimitive::EllipsoidShape);
            visCache.basePrimitive.setShadingMode(ParticlePrimitive::NormalShading);
            visCache.basePrimitive.setRenderingQuality(ParticlePrimitive::MediumQuality);

            // Fill in the position data for the base sites.
            BufferFactory<Point3G> baseSites(particles->elementCount());
            BufferReadAccess<Point3> positionsArray(positionProperty);
            BufferReadAccess<Vector3> nucleotideAxisArray(nucleotideAxisProperty);
            for(size_t i = 0; i < baseSites.size(); i++)
                baseSites[i] = (positionsArray[i] + (0.8 * nucleotideAxisArray[i])).toDataType<GraphicsFloatType>();
            visCache.basePrimitive.setPositions(baseSites.take());

            // Fill in base color data.
            visCache.basePrimitive.setColors(nucleobaseColors(particles, renderer->isInteractive()));

            // Fill in aspherical shape values.
            DataBufferPtr asphericalShapes = DataBufferPtr::create(particles->elementCount(), DataBuffer::FloatGraphics, 3);
            asphericalShapes->fill<Vector3G>(static_cast<GraphicsFloatType>(cylinderRadius()) * Vector3G(2.0f, 3.0f, 1.0f));
            visCache.basePrimitive.setAsphericalShapes(std::move(asphericalShapes));

            // Fill in base orientations.
            if(BufferReadAccess<Vector3> nucleotideNormalArray = nucleotideNormalProperty) {
                PropertyPtr orientations = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::OrientationProperty);
                BufferWriteAccess<QuaternionG, access_mode::discard_write> orientationsAccess(orientations);
                for(size_t i = 0; i < orientations->size(); i++) {
                    if(nucleotideNormalArray[i] != Vector3::Zero() && nucleotideAxisArray[i] != Vector3::Zero()) {
                        // Build an orthonomal basis from the two direction vectors of a nucleotide.
                        Matrix3 tm;
                        tm.column(2) = nucleotideNormalArray[i];
                        tm.column(1) = nucleotideAxisArray[i];
                        tm.column(0) = tm.column(1).cross(tm.column(2));
                        if(!tm.column(0).isZero()) {
                            tm.orthonormalize();
                            orientationsAccess[i] = Quaternion(tm).toDataType<GraphicsFloatType>();
                        }
                        else orientationsAccess[i] = QuaternionG::Identity();
                    }
                    else {
                        orientationsAccess[i] = QuaternionG::Identity();
                    }
                }
                visCache.basePrimitive.setOrientations(std::move(orientations));
            }

            // Create the rendering primitive for the connections between backbone and base sites.
            visCache.connectionPrimitive.setShape(CylinderPrimitive::CylinderShape);
            visCache.connectionPrimitive.setShadingMode(CylinderPrimitive::NormalShading);
            visCache.connectionPrimitive.setUniformWidth(2 * cylinderRadius());
            visCache.connectionPrimitive.setColors(colors);
            BufferFactory<Point3G> headPositions(particles->elementCount());
            for(size_t i = 0; i < positionsArray.size(); i++)
                headPositions[i] = (positionsArray[i] + 0.8 * nucleotideAxisArray[i]).toDataType<GraphicsFloatType>();
            visCache.connectionPrimitive.setPositions(positionProperty, headPositions.take());
        }
        else {
            visCache.connectionPrimitive = CylinderPrimitive();
            visCache.basePrimitive = ParticlePrimitive();
        }

        // Create pick info record.
        BufferFactory<int32_t> subobjectToParticleMapping(nucleotideAxisProperty ? (particles->elementCount() * 3) : particles->elementCount());
        std::iota(subobjectToParticleMapping.begin(), subobjectToParticleMapping.begin() + particles->elementCount(), 0);
        if(nucleotideAxisProperty) {
            std::iota(subobjectToParticleMapping.begin() +     particles->elementCount(), subobjectToParticleMapping.begin() + 2 * particles->elementCount(), 0);
            std::iota(subobjectToParticleMapping.begin() + 2 * particles->elementCount(), subobjectToParticleMapping.begin() + 3 * particles->elementCount(), 0);
        }
        visCache.pickInfo = OORef<ParticlePickInfo>::create(this, particles, subobjectToParticleMapping.take());
    }
    else {
        // Update the pipeline state stored in te picking object info.
        visCache.pickInfo->setParticles(particles);
    }

    renderer->beginPickObject(pipeline, visCache.pickInfo);

    renderer->renderParticles(visCache.backbonePrimitive);
    if(visCache.connectionPrimitive.basePositions())
        renderer->renderCylinders(visCache.connectionPrimitive);
    if(visCache.basePrimitive.positions())
        renderer->renderParticles(visCache.basePrimitive);

    renderer->endPickObject();

    return {};
}

}   // End of namespace
