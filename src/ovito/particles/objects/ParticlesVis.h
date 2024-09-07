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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering particles.
 */
class OVITO_PARTICLES_EXPORT ParticlesVis : public DataVis
{
    OVITO_CLASS(ParticlesVis)

public:

    /// The standard shapes supported by the particles visualization element.
    enum ParticleShape {
        Sphere,             // Includes ellipsoids and superquadrics
        Box,                // Includes cubes and non-cubic boxes
        Circle,
        Square,
        Cylinder,
        Spherocylinder,
        Mesh,
        Default
    };
    Q_ENUM(ParticleShape);

public:

    /// Renders the visual element.
    virtual std::variant<PipelineStatus, Future<PipelineStatus>> render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the visual element.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Returns the default display color for particles.
    Color defaultParticleColor() const { return Color(1,1,1); }

    /// Returns the display color used for selected particles.
    Color selectionParticleColor() const { return Color(1,0,0); }

    /// Returns the actual particle shape used to render the particles.
    static ParticlePrimitive::ParticleShape effectiveParticleShape(ParticleShape shape, const Property* shapeProperty, const Property* orientationProperty, const Property* roundnessProperty);

    /// Returns the actual rendering quality used to render the particles.
    ParticlePrimitive::RenderingQuality effectiveRenderingQuality(bool isInteractiveRenderer, const Particles* particles) const;

    /// Determines the color of each particle to be used for rendering.
    ConstPropertyPtr particleColors(const Particles* particles, bool highlightSelection) const;

    /// Determines the particle radii used for rendering.
    ConstPropertyPtr particleRadii(const Particles* particles, bool includeGlobalScaleFactor) const;

    /// Determines the display radius of a single particle.
    GraphicsFloatType particleRadius(size_t particleIndex, BufferReadAccess<GraphicsFloatType> radiusProperty, const Property* typeProperty) const;

    /// Returns the display color of a single particle.
    ColorG particleColor(size_t particleIndex, BufferReadAccess<ColorG> colorProperty, const Property* typeProperty, BufferReadAccess<SelectionIntType> selectionProperty) const;

    /// Computes the bounding box of the particles.
    Box3 particleBoundingBox(BufferReadAccess<Point3> positionProperty, const Property* typeProperty, BufferReadAccess<GraphicsFloatType> radiusProperty, BufferReadAccess<Vector3G> shapeProperty, bool includeParticleRadius) const;

    /// Render a marker around a particle to highlight it in the viewports.
    void highlightParticle(size_t particleIndex, const Particles* particles, FrameGraph& frameGraph, const Pipeline* pipeline) const;

    /// Returns the typed particle property used to determine the rendering colors of particles (if no per-particle colors are defined).
    virtual const Property* getParticleTypeColorProperty(const Particles* particles) const;

    /// Returns the typed particle property used to determine the rendering radii of particles (if no per-particle radii are defined).
    virtual const Property* getParticleTypeRadiusProperty(const Particles* particles) const;

public:

    Q_PROPERTY(Ovito::ParticlePrimitive::RenderingQuality renderingQuality READ renderingQuality WRITE setRenderingQuality)
    Q_PROPERTY(Ovito::ParticlesVis::ParticleShape particleShape READ particleShape WRITE setParticleShape)

private:

    /// Renders particle types that have a mesh-based shape assigned.
    void renderMeshBasedParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const Pipeline* pipeline, const AffineTransformation& tm) const;

    /// Renders all particles with a primitive shape (spherical, box, (super)quadrics).
    void renderPrimitiveParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const Pipeline* pipeline, const AffineTransformation& tm) const;

    /// Renders all particles with a (sphero-)cylindrical shape.
    void renderCylindricParticles(const Particles* particles, FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const Pipeline* pipeline, const AffineTransformation& tm) const;

private:

    /// Controls the default display radius of particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.2}, defaultParticleRadius, setDefaultParticleRadius, PROPERTY_FIELD_MEMORIZE);

    /// Controls the global scaling factor, which is applied to all rendered particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{1.0}, radiusScaleFactor, setRadiusScaleFactor);

    /// Controls the rendering quality mode for particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticlePrimitive::RenderingQuality{ParticlePrimitive::AutoQuality}, renderingQuality, setRenderingQuality);

    /// Controls the display shape of particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticleShape{Sphere}, particleShape, setParticleShape);
};

/**
 * \brief This information record is attached to the particles by the ParticlesVis when rendering
 * them in the viewports. It facilitates the picking of particles with the mouse.
 */
class OVITO_PARTICLES_EXPORT ParticlePickInfo : public ObjectPickInfo
{
    OVITO_CLASS(ParticlePickInfo)

public:

    /// Constructor.
    void initializeObject(const ParticlesVis* visElement, DataOORef<const Particles> particles, ConstDataBufferPtr subobjectToParticleMapping = {}) {
        ObjectPickInfo::initializeObject();
        _visElement = visElement;
        _particles = std::move(particles);
        _subobjectToParticleMapping = std::move(subobjectToParticleMapping);
    }

    /// Returns the particles object.
    const DataOORef<const Particles>& particles() const { OVITO_ASSERT(_particles); return _particles; }

    /// Updates the reference to the particles object.
    void setParticles(DataOORef<const Particles> particles) { _particles = std::move(particles); }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) override;

    /// Given an sub-object ID returned by the Viewport::pick() method, looks up the
    /// corresponding particle index.
    size_t particleIndexFromSubObjectID(uint32_t subobjID) const;

private:

    /// The vis element that rendered the particles.
    OORef<const ParticlesVis> _visElement;

    /// The particles object.
    DataOORef<const Particles> _particles;

    /// Stores the indices of the particles associated with the rendering primitives.
    ConstDataBufferPtr _subobjectToParticleMapping;
};

}   // End of namespace
