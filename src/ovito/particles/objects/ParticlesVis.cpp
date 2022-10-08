////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/DataObjectAccess.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "ParticlesVis.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParticlesVis);
IMPLEMENT_OVITO_CLASS(ParticlePickInfo);
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
* Constructor.
******************************************************************************/
ParticlesVis::ParticlesVis(ObjectCreationParams params) : DataVis(params),
	_defaultParticleRadius(1.2),
	_radiusScaleFactor(1.0),
	_renderingQuality(ParticlePrimitive::AutoQuality),
	_particleShape(Sphere)
{
}

/******************************************************************************
* Computes the bounding box of the visual element.
******************************************************************************/
Box3 ParticlesVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
	const ParticlesObject* particles = path.lastAs<ParticlesObject>();
	if(!particles) return {};
	particles->verifyIntegrity();
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	const PropertyObject* radiusProperty = particles->getProperty(ParticlesObject::RadiusProperty);
	const PropertyObject* typeProperty = particles->getProperty(ParticlesObject::TypeProperty);
	const PropertyObject* shapeProperty = particles->getProperty(ParticlesObject::AsphericalShapeProperty);

	// The key type used for caching the computed bounding box:
	using CacheKey = RendererResourceKey<struct ParticlesVisBoundingBoxCache,
		ConstDataObjectRef,	// Position property
		ConstDataObjectRef,	// Radius property
		ConstDataObjectRef,	// Type property
		ConstDataObjectRef,	// Aspherical shape property
		FloatType, 			// Default particle radius
		FloatType, 			// Uniform scaling factor
		ParticleShape		// Standard particle shape
	>;

	// Look up the bounding box in the vis cache.
	auto& bbox = dataset()->visCache().get<Box3>(CacheKey(
			positionProperty,
			radiusProperty,
			typeProperty,
			shapeProperty,
			defaultParticleRadius(),
			radiusScaleFactor(),
			particleShape()));

	// Check if the cached bounding box information is still up to date.
	if(bbox.isEmpty()) {
		// If not, recompute bounding box from particle data.
		bbox = particleBoundingBox(positionProperty, typeProperty, radiusProperty, shapeProperty, true);
	}
	return bbox;
}

/******************************************************************************
* Computes the bounding box of the particles.
******************************************************************************/
Box3 ParticlesVis::particleBoundingBox(ConstPropertyAccess<Point3> positionProperty, const PropertyObject* typeProperty, ConstPropertyAccess<FloatType> radiusProperty, ConstPropertyAccess<Vector3> shapeProperty, bool includeParticleRadius) const
{
	OVITO_ASSERT(typeProperty == nullptr || typeProperty->type() == ParticlesObject::TypeProperty);
	if(particleShape() != Sphere && particleShape() != Box && particleShape() != Cylinder && particleShape() != Spherocylinder)
		shapeProperty = nullptr;

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
			for(const Vector3& s : shapeProperty)
				maxAtomRadius = std::max(maxAtomRadius, std::max(s.x(), std::max(s.y(), s.z())));
			if(particleShape() == Spherocylinder)
				maxAtomRadius *= 2;
		}
		if(radiusProperty && radiusProperty.size() != 0) {
			auto minmax = std::minmax_element(radiusProperty.cbegin(), radiusProperty.cend());
			if(*minmax.first <= 0)
				maxAtomRadius = std::max(maxAtomRadius, *minmax.second);
			else
				maxAtomRadius = *minmax.second;
		}
	}
	else {
		// Non-standard case - at least one user-defined particle shape assigned:
		std::map<int,FloatType> typeRadiusMap = ParticleType::typeRadiusMap(typeProperty);
		if(radiusProperty && radiusProperty.size() == typeProperty->size()) {
			const FloatType* r = radiusProperty.cbegin();
			ConstPropertyAccess<int> typeData(typeProperty);
			for(int t : typeData) {
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
const PropertyObject* ParticlesVis::getParticleTypeColorProperty(const ParticlesObject* particles) const
{
	return particles->getProperty(ParticlesObject::TypeProperty);
}

/******************************************************************************
* Determines the display particle colors.
******************************************************************************/
ConstPropertyPtr ParticlesVis::particleColors(const ParticlesObject* particles, bool highlightSelection) const
{
	OVITO_ASSERT(particles);
	particles->verifyIntegrity();

	// Take particle colors directly from the 'Color' property if available.
	DataObjectAccess<DataOORef, PropertyObject> output = particles->getProperty(ParticlesObject::ColorProperty);
	if(!output) {
		// Allocate new output color array.
		output.reset(ParticlesObject::OOClass().createStandardProperty(dataset(), particles->elementCount(), ParticlesObject::ColorProperty));

		const Color defaultColor = defaultParticleColor();
		if(const PropertyObject* typeProperty = getParticleTypeColorProperty(particles)) {
			OVITO_ASSERT(typeProperty->size() == output->size());
			// Assign colors based on particle types.
			// Generate a lookup map for particle type colors.
			const std::map<int,Color> colorMap = typeProperty->typeColorMap();
			std::array<Color,16> colorArray;
			// Check if all type IDs are within a small, non-negative range.
			// If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
			if(boost::algorithm::all_of(colorMap, [&colorArray](const std::map<int,Color>::value_type& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
				colorArray.fill(defaultColor);
				for(const auto& entry : colorMap)
					colorArray[entry.first] = entry.second;
				// Fill color array.
				ConstPropertyAccess<int> typeData(typeProperty);
				const int* t = typeData.cbegin();
				for(Color& c : PropertyAccess<Color>(output.makeMutable())) {
					if(*t >= 0 && *t < (int)colorArray.size())
						c = colorArray[*t];
					else
						c = defaultColor;
					++t;
				}
			}
			else {
				// Fill color array.
				ConstPropertyAccess<int> typeData(typeProperty);
				const int* t = typeData.cbegin();
				for(Color& c : PropertyAccess<Color>(output.makeMutable())) {
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
			output.makeMutable()->fill(defaultColor);
		}
	}

	// Highlight selected particles with a special color.
	if(const PropertyObject* selectionProperty = highlightSelection ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr)
		output.makeMutable()->fillSelected(selectionParticleColor(), *selectionProperty);

	return output.take();
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering radii 
* of particles (if no per-particle radii are defined).
******************************************************************************/
const PropertyObject* ParticlesVis::getParticleTypeRadiusProperty(const ParticlesObject* particles) const
{
	return particles->getProperty(ParticlesObject::TypeProperty);
}

/******************************************************************************
* Determines the display particle radii.
******************************************************************************/
ConstPropertyPtr ParticlesVis::particleRadii(const ParticlesObject* particles, bool includeGlobalScaleFactor) const
{
	particles->verifyIntegrity();
	FloatType defaultRadius = defaultParticleRadius();
	if(includeGlobalScaleFactor)
		defaultRadius *= radiusScaleFactor();

	// Take particle radii directly from the 'Radius' property if available.
	DataObjectAccess<DataOORef, PropertyObject> output = particles->getProperty(ParticlesObject::RadiusProperty);
	if(output) {
		// Check if the radius array contains any zero entries.
		ConstPropertyAccess<FloatType> radiusArray(output);
		if(boost::find(radiusArray, FloatType(0)) != radiusArray.end()) {	
			radiusArray.reset();

			// Copy per-type radii to those particles whose "Radius" property value is zero.
			if(const PropertyObject* typeProperty = getParticleTypeRadiusProperty(particles)) {
				// Build a lookup map for particle type radii.
				std::map<int,FloatType> radiusMap = ParticleType::typeRadiusMap(typeProperty);
				// Skip the following loop if all per-type radii are zero. 
				if(boost::algorithm::any_of(radiusMap, [](const std::pair<int,FloatType>& it) { return it.second != 0; })) {
					// Fill radius array.
					ConstPropertyAccess<int> typeArray(typeProperty);
					const int* type = typeArray.cbegin();
					for(FloatType& radius : PropertyAccess<FloatType>(output.makeMutable())) {
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
			boost::replace(PropertyAccess<FloatType>(output.makeMutable()), FloatType(0), defaultParticleRadius());
		}
		// Apply global scaling factor.
		if(includeGlobalScaleFactor && radiusScaleFactor() != 1.0) {
			for(FloatType& r : PropertyAccess<FloatType>(output.makeMutable()))
				r *= radiusScaleFactor();
		}
	}
	else {
		// Allocate output array.
		output.reset(ParticlesObject::OOClass().createStandardProperty(dataset(), particles->elementCount(), ParticlesObject::RadiusProperty));

		if(const PropertyObject* typeProperty = getParticleTypeRadiusProperty(particles)) {
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
				ConstPropertyAccess<int> typeData(typeProperty);
				PropertyAccess<FloatType> radiusArray(output.makeMutable());
				boost::transform(typeData, radiusArray.begin(), [&](int t) {
					// Set particle radius only if the type's radius is non-zero.
					if(auto it = radiusMap.find(t); it != radiusMap.end() && it->second != 0)
						return it->second;
					else
						return defaultRadius;
				});
			}
			else {
				// Assign the uniform default radius to all particles.
				output.makeMutable()->fill(defaultRadius);
			}
		}
		else {
			// Assign the uniform default radius to all particles.
			output.makeMutable()->fill(defaultRadius);
		}
	}

	return output.take();
}

/******************************************************************************
* Determines the display radius of a single particle.
******************************************************************************/
FloatType ParticlesVis::particleRadius(size_t particleIndex, ConstPropertyAccess<FloatType> radiusProperty, const PropertyObject* typeProperty) const
{
	OVITO_ASSERT(typeProperty == nullptr || typeProperty->type() == ParticlesObject::TypeProperty);

	if(radiusProperty && radiusProperty.size() > particleIndex) {
		// Take particle radius directly from the radius property.
		FloatType r = radiusProperty[particleIndex];
		if(r > 0) return r * radiusScaleFactor();
	}
	else if(typeProperty && typeProperty->size() > particleIndex) {
		// Assign radius based on particle types.
		ConstPropertyAccess<int> typeData(typeProperty);
		const ParticleType* ptype = static_object_cast<ParticleType>(typeProperty->elementType(typeData[particleIndex]));
		if(ptype && ptype->radius() > 0)
			return ptype->radius() * radiusScaleFactor();
	}

	return defaultParticleRadius() * radiusScaleFactor();
}

/******************************************************************************
* Determines the display color of a single particle.
******************************************************************************/
Color ParticlesVis::particleColor(size_t particleIndex, ConstPropertyAccess<Color> colorProperty, const PropertyObject* typeProperty, ConstPropertyAccess<int> selectionProperty) const
{
	// Check if particle is selected.
	if(selectionProperty && selectionProperty.size() > particleIndex) {
		if(selectionProperty[particleIndex])
			return selectionParticleColor();
	}

	Color c = defaultParticleColor();
	if(colorProperty && colorProperty.size() > particleIndex) {
		// Take particle color directly from the color property.
		c = colorProperty[particleIndex];
	}
	else if(typeProperty && typeProperty->size() > particleIndex) {
		// Return color based on particle types.
		ConstPropertyAccess<int> typeData(typeProperty);
		const ElementType* ptype = typeProperty->elementType(typeData[particleIndex]);
		if(ptype)
			c = ptype->color();
	}

	return c;
}

/******************************************************************************
* Returns the actual rendering quality used to render the particles.
******************************************************************************/
ParticlePrimitive::RenderingQuality ParticlesVis::effectiveRenderingQuality(SceneRenderer* renderer, const ParticlesObject* particles) const
{
	ParticlePrimitive::RenderingQuality renderQuality = renderingQuality();
	if(renderQuality == ParticlePrimitive::AutoQuality) {
		if(!particles) return ParticlePrimitive::HighQuality;
		size_t particleCount = particles->elementCount();
		if(particleCount < 4000 || renderer->isInteractive() == false)
			renderQuality = ParticlePrimitive::HighQuality;
		else if(particleCount < 400000)
			renderQuality = ParticlePrimitive::MediumQuality;
		else
			renderQuality = ParticlePrimitive::LowQuality;
	}
	return renderQuality;
}

/******************************************************************************
* Returns the effective primtive shape for rendering the particles.
******************************************************************************/
ParticlePrimitive::ParticleShape ParticlesVis::effectiveParticleShape(ParticleShape shape, const PropertyObject* shapeProperty, const PropertyObject* orientationProperty, const PropertyObject* roundnessProperty)
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
PipelineStatus ParticlesVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	// Handle bounding-box computation in a separate method.
	if(renderer->isBoundingBoxPass()) {
		TimeInterval validityInterval;
		renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, validityInterval));
		return {};
	}

	// Get input particle data.
	const ParticlesObject* particles = path.lastAs<ParticlesObject>();
	if(!particles) return {};
	particles->verifyIntegrity();

	// Make sure we don't exceed the internal limits. Rendering of more than 2 billion particles is not yet supported by OVITO.
	size_t particleCount = particles->elementCount();
	if(particleCount > (size_t)std::numeric_limits<int>::max()) {
		throwException(tr("This version of OVITO doesn't support rendering of more than %1 particles.").arg(std::numeric_limits<int>::max()));
	}

	// Render all mesh-based particle types.
	renderMeshBasedParticles(particles, renderer, contextNode);

	// Render all primitive particle types.
	renderPrimitiveParticles(particles, renderer, contextNode);

	// Render all (sphero-)cylindric particle types.
	renderCylindricParticles(particles, renderer, contextNode);

	return {};
}

/******************************************************************************
* Renders particle types that have a mesh-based shape assigned.
******************************************************************************/
void ParticlesVis::renderMeshBasedParticles(const ParticlesObject* particles, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	// Get input particle data.
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	const PropertyObject* radiusProperty = particles->getProperty(ParticlesObject::RadiusProperty);
	const PropertyObject* colorProperty = particles->getProperty(ParticlesObject::ColorProperty);
	const PropertyObject* typeProperty = particles->getProperty(ParticlesObject::TypeProperty);
	const PropertyObject* selectionProperty = renderer->isInteractive() ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr;
	const PropertyObject* transparencyProperty = particles->getProperty(ParticlesObject::TransparencyProperty);
	const PropertyObject* orientationProperty = particles->getProperty(ParticlesObject::OrientationProperty);
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
		ConstDataObjectRef,			// Particle type property
		FloatType,					// Default particle radius
		FloatType,					// Global radius scaling factor
		ConstDataObjectRef,			// Position property
		ConstDataObjectRef,			// Orientation property
		ConstDataObjectRef,			// Color property
		ConstDataObjectRef,			// Selection property
		ConstDataObjectRef,			// Transparency property
		ConstDataObjectRef			// Radius property
	>;
	// The data structure created for each mesh-based particle type.
	struct MeshParticleType {
		MeshPrimitive meshPrimitive;
		OORef<ParticlePickInfo> pickInfo;
		bool useMeshColors; ///< Controls the use of the original face colors from the mesh instead of the per-particle colors.
	};
	// The data structure stored in the vis cache for the mesh-based particle shapes.
	using ShapeMeshCacheValue = std::vector<MeshParticleType>;

	// Look up the rendering primitives for mesh-based particle types in the vis cache.
	ShapeMeshCacheValue& meshVisCache = dataset()->visCache().get<ShapeMeshCacheValue>(ShapeMeshCacheKey{
		typeProperty,
		defaultParticleRadius(),
		radiusScaleFactor(),
		positionProperty,
		orientationProperty,
		colorProperty,
		selectionProperty,
		transparencyProperty,
		radiusProperty});

	// Check if we already have valid rendering primitives that are up to date.
	if(meshVisCache.empty()) {
		meshVisCache.clear();

		// This data structure stores temporary per-particle instance data, separated by mesh-based particle type.
		struct MeshTypePerInstanceData {
			MeshTypePerInstanceData(DataBufferPtr tm, DataBufferPtr colors, DataBufferPtr indices) : 
				particleTMs(std::move(tm)), particleColors(std::move(colors)), particleIndices(std::move(indices)) {}
			DataBufferAccessAndRef<AffineTransformation> particleTMs; 	/// AffineTransformation of each particle to be rendered.
			DataBufferAccessAndRef<ColorA> particleColors;	/// Color of each particle to be rendered.				
			DataBufferAccessAndRef<int> particleIndices;	/// Index of each particle to be rendered in the original particles list.
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

			perInstanceData.emplace_back(
				DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 12),	// <AffineTransformation> (particle orientations)
				DataBufferPtr::create(dataset(), 0, DataBuffer::Float,  4),	// <ColorA> (particle colors)
				DataBufferPtr::create(dataset(), 0, DataBuffer::Int)		// <int> (particle indices)
			);
		}

		// Compile the per-instance particle data (positions, orientations, colors, etc) for each mesh-based particle type.
		ConstPropertyAccessAndRef<Color> colors = particleColors(particles, renderer->isInteractive());
		ConstPropertyAccessAndRef<FloatType> radii = particleRadii(particles, true);
		ConstPropertyAccess<int> types(typeProperty);
		ConstPropertyAccess<Point3> positions(positionProperty);
		ConstPropertyAccess<Quaternion> orientations(orientationProperty);
		ConstPropertyAccess<FloatType> transparencies(transparencyProperty);
		size_t particleCount = particles->elementCount();
		for(size_t i = 0; i < particleCount; i++) {
			if(radii[i] <= 0) continue;
			auto iter = boost::find(shapeMeshParticleTypes, types[i]);
			if(iter == shapeMeshParticleTypes.end()) continue;
			size_t typeIndex = std::distance(shapeMeshParticleTypes.begin(), iter);
			AffineTransformation tm = AffineTransformation::scaling(radii[i]);
			if(positions)
				tm.translation() = positions[i] - Point3::Origin();
			if(orientations) {
				Quaternion quat = orientations[i];
				// Normalize quaternion.
				FloatType c = sqrt(quat.dot(quat));
				if(c <= FLOATTYPE_EPSILON)
					quat.setIdentity();
				else
					quat /= c;
				tm = tm * Matrix3::rotation(quat);
			}
			perInstanceData[typeIndex].particleTMs.push_back(tm);
			perInstanceData[typeIndex].particleColors.push_back(ColorA(colors[i], transparencies ? qBound(0.0, 1.0 - transparencies[i], 1.0) : 1.0));
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
			meshVisCache[typeIndex].pickInfo = new ParticlePickInfo(this, particles, perInstanceData[typeIndex].particleIndices.take());
		}
	}
	OVITO_ASSERT(meshVisCache.size() == shapeMeshParticleTypes.size());

	// Render the instanced mesh primitives, one for each particle type with a mesh-based shape.
	for(MeshParticleType& t : meshVisCache) {

		// Update the pick info record with the latest particle data.
		t.pickInfo->setParticles(particles);

		if(renderer->isPicking())
			renderer->beginPickObject(contextNode, t.pickInfo);
		renderer->renderMesh(t.meshPrimitive);
		if(renderer->isPicking())
			renderer->endPickObject();
	}
}

/******************************************************************************
* Renders all particles with a primitive shape (spherical, box, (super)quadrics).
******************************************************************************/
void ParticlesVis::renderPrimitiveParticles(const ParticlesObject* particles, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	// Determine whether all particle types use the same uniform shape or not.
	ParticlesVis::ParticleShape uniformShape = particleShape();
	OVITO_ASSERT(uniformShape != ParticleShape::Default);
	if(uniformShape == ParticleShape::Default)
		return;
	const PropertyObject* typeProperty = particles->getProperty(ParticlesObject::TypeProperty);
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
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	const PropertyObject* radiusProperty = particles->getProperty(ParticlesObject::RadiusProperty);
	const PropertyObject* colorProperty = particles->getProperty(ParticlesObject::ColorProperty);
	const PropertyObject* typeColorProperty = getParticleTypeColorProperty(particles);
	const PropertyObject* typeRadiusProperty = getParticleTypeRadiusProperty(particles);
	const PropertyObject* selectionProperty = renderer->isInteractive() ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr;
	const PropertyObject* transparencyProperty = particles->getProperty(ParticlesObject::TransparencyProperty);
	const PropertyObject* asphericalShapeProperty = particles->getProperty(ParticlesObject::AsphericalShapeProperty);
	const PropertyObject* orientationProperty = particles->getProperty(ParticlesObject::OrientationProperty);
	const PropertyObject* roundnessProperty = particles->getProperty(ParticlesObject::SuperquadricRoundnessProperty);
	if(!positionProperty)
		return;

	// Prepare the particle rendering primitive.
	ParticlePrimitive primitive;

	// Pick render quality level adaptively based on current number of particles.
	ParticlePrimitive::RenderingQuality primitiveRenderQuality = effectiveRenderingQuality(renderer, particles);
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
			ConstDataObjectRef,		// Aspherical shape property
			FloatType				// Scaling factor
		>;
		// Look up the scaled aspherical shape array in the vis cache, which have been multipled with the uniform scaling factor.
		ConstDataBufferPtr& scaledShapes = dataset()->visCache().get<ConstDataBufferPtr>(ParticleShapeCacheKey(asphericalShapeProperty, radiusScaleFactor()));
		if(!scaledShapes) {
			// Make a copy of the original aspherical shape array and multiple all vectors with the scaling factor.
			DataBufferAccessAndRef<Vector3> values = ConstDataBufferPtr::makeCopy(asphericalShapeProperty);
			for(Vector3& s : values) s *= radiusScaleFactor();
			scaledShapes = values.take();
		}
		primitive.setAsphericalShapes(scaledShapes);
	}

	// Create separate rendering primitives for the different shapes supported by the method.
	for(ParticlesVis::ParticleShape shape : {ParticleShape::Sphere, ParticleShape::Box, ParticleShape::Circle, ParticleShape::Square}) {

		// Skip this shape if all particles are known to have another shape.
		if(uniformShape != ParticleShape::Default && uniformShape != shape) 
			continue;

		// The lookup key for the cached particle indices for the current shape type:
		using ParticleCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveCache,
			ConstDataObjectRef,					// Particle type property
			ParticlesVis::ParticleShape,		// Current particle shape
			ParticlesVis::ParticleShape,		// Global particle shape
			size_t								// Particle count
		>;

		// Look up the particle indices in the vis cache.
		ConstDataBufferPtr& indices = dataset()->visCache().get<ConstDataBufferPtr>(ParticleCacheKey(
			typeProperty,
			shape,
			uniformShape,
			particles->elementCount()));

		if(!indices) {
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
				DataBufferAccessAndRef<int> activeParticleIndices = DataBufferPtr::create(dataset(), 0, DataBuffer::Int);
				size_t index = 0;
				for(int t : ConstPropertyAccess<int>(typeProperty)) {
					if(boost::find(activeParticleTypes, t) != activeParticleTypes.cend())
						activeParticleIndices.push_back(index);
					index++;
				}
				indices = activeParticleIndices.take();
			}
		}

		if(indices && indices->size() == 0)
			continue;	// No particles to be rendered using the current primitive shape.

		// Enable/disable indexed rendering of particle primitives.
		primitive.setIndices(indices);

		if(!primitive.radii()) {
			// The type of lookup key used for caching the particle radii:
			using RadiiCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveRadiusCache,
				FloatType,							// Default particle radius
				FloatType,							// Global radius scaling factor
				ConstDataObjectRef,					// Radius property
				ConstDataObjectRef,					// Type property
				size_t								// Particle count
			>;
			ConstPropertyPtr& radiusBuffer = dataset()->visCache().get<ConstPropertyPtr>(RadiiCacheKey(
				defaultParticleRadius(),
				radiusScaleFactor(),
				radiusProperty,
				typeRadiusProperty,
				particles->elementCount()));
			if(!radiusBuffer)
				radiusBuffer = particleRadii(particles, true);
			primitive.setRadii(radiusBuffer);
		}

		if(!primitive.colors()) {
			// The type of lookup key used for caching the particle colors:
			using ColorCacheKey = RendererResourceKey<struct ParticlesVisPrimitiveColorCache,
				ConstDataObjectRef,					// Type property
				ConstDataObjectRef,					// Color property
				size_t								// Particle count
			>;
			ConstPropertyPtr& colorBuffer = dataset()->visCache().get<ConstPropertyPtr>(ColorCacheKey(
				typeProperty,
				colorProperty,
				particles->elementCount()));
			if(!colorBuffer)
				colorBuffer = particleColors(particles, false);
			primitive.setColors(colorBuffer);
		}

		// Configure rendering shape and shading style.
		ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, asphericalShapeProperty, orientationProperty, roundnessProperty);
		ParticlePrimitive::ShadingMode primitiveShadingMode = (shape == Circle || shape == Square) ? ParticlePrimitive::FlatShading : ParticlePrimitive::NormalShading;
		primitive.setParticleShape(primitiveParticleShape);
		primitive.setShadingMode(primitiveShadingMode);

		if(renderer->isPicking()) {
			// Look up or create the pick info record with the latest particle data.
			OORef<ParticlePickInfo>& pickingInfo = dataset()->visCache().get<OORef<ParticlePickInfo>>(ConstDataObjectRef(particles));
			if(!pickingInfo) pickingInfo = new ParticlePickInfo(this, particles);
			renderer->beginPickObject(contextNode, pickingInfo);
		}
		// Render the particle primitive.
		renderer->renderParticles(primitive);
		if(renderer->isPicking()) renderer->endPickObject();
	}
}

/******************************************************************************
* Renders all particles with a (sphero-)cylindrical shape.
******************************************************************************/
void ParticlesVis::renderCylindricParticles(const ParticlesObject* particles, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	// Determine whether all particle types use the same uniform shape or not.
	ParticlesVis::ParticleShape uniformShape = particleShape();
	OVITO_ASSERT(uniformShape != ParticleShape::Default);
	if(uniformShape == ParticleShape::Default)
		return;
	const PropertyObject* typeProperty = particles->getProperty(ParticlesObject::TypeProperty);
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
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	const PropertyObject* radiusProperty = particles->getProperty(ParticlesObject::RadiusProperty);
	const PropertyObject* colorProperty = particles->getProperty(ParticlesObject::ColorProperty);
	const PropertyObject* selectionProperty = renderer->isInteractive() ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr;
	const PropertyObject* transparencyProperty = particles->getProperty(ParticlesObject::TransparencyProperty);
	const PropertyObject* asphericalShapeProperty = particles->getProperty(ParticlesObject::AsphericalShapeProperty);
	const PropertyObject* orientationProperty = particles->getProperty(ParticlesObject::OrientationProperty);
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
			ConstDataObjectRef,					// Position property
			ConstDataObjectRef,					// Type property
			ConstDataObjectRef,					// Selection property
			ConstDataObjectRef,					// Color property
			ConstDataObjectRef,					// Transparency property
			ConstDataObjectRef,					// Apherical shape property
			ConstDataObjectRef,					// Orientation property
			ConstDataObjectRef,					// Radius property
			FloatType,							// Default particle radius
			FloatType,							// Global radius scaling factor
			ParticlesVis::ParticleShape,		// Global particle shape
			ParticlesVis::ParticleShape			// Local particle shape
		>;
		// The data structure stored in the vis cache.
		struct ParticleCacheValue {
			CylinderPrimitive cylinderPrimitive;
			ParticlePrimitive spheresPrimitives[2];
			OORef<ParticlePickInfo> pickInfo;
			bool isCreated = false;
		};

		// Look up the rendering primitive in the vis cache.
		auto& visCache = dataset()->visCache().get<ParticleCacheValue>(ParticleCacheKey(
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
			shape));

		// Check if the rendering primitive needs to be recreated from scratch.
		if(!visCache.isCreated) {
			visCache.isCreated = true;

			// Determine the set of particles to be rendered using the current shape.
			DataBufferAccessAndRef<int> activeParticleIndices;
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
				activeParticleIndices = DataBufferPtr::create(dataset(), 0, DataBuffer::Int);
				size_t index = 0;
				for(int t : ConstPropertyAccess<int>(typeProperty)) {
					if(boost::find(activeParticleTypes, t) != activeParticleTypes.cend())
						activeParticleIndices.push_back(index);
					index++;
				}

				if(activeParticleIndices.size() == 0) {
					visCache.cylinderPrimitive = CylinderPrimitive();
					visCache.spheresPrimitives[0] = ParticlePrimitive();
					visCache.spheresPrimitives[1] = ParticlePrimitive();
					visCache.pickInfo.reset();
					continue;	// No particles to be rendered using the current primitive shape.
				}
			}
			int effectiveParticleCount = activeParticleIndices ? activeParticleIndices.size() : particles->elementCount();

			// Create the rendering primitive for the cylinders.
			visCache.cylinderPrimitive.setShape(CylinderPrimitive::CylinderShape);
			visCache.cylinderPrimitive.setShadingMode(CylinderPrimitive::NormalShading);

			// Determine cylinder colors.
			if(!colorBuffer)
				colorBuffer = particleColors(particles, renderer->isInteractive());
			
			// Determine cylinder radii (only needed if aspherical shape property is not present).
			if(!radiusBuffer && !asphericalShapeProperty)
				radiusBuffer = particleRadii(particles, false);

			// Allocate cylinder data buffers.
			DataBufferAccessAndRef<Point3> cylinderBasePositions = DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float, 3);
			DataBufferAccessAndRef<Point3> cylinderHeadPositions = DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float, 3);
			DataBufferAccessAndRef<FloatType> cylinderWidths = DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float);
			DataBufferAccessAndRef<Color> cylinderColors = DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float, 3);
			DataBufferAccessAndRef<FloatType> cylinderTransparencies = transparencyProperty ? DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float) : nullptr;
			DataBufferAccessAndRef<FloatType> sphereRadii = (shape == ParticleShape::Spherocylinder) ? DataBufferPtr::create(dataset(), effectiveParticleCount, DataBuffer::Float) : nullptr;

			// Fill data buffers.
			ConstPropertyAccess<Point3> positionArray(positionProperty);
			ConstPropertyAccess<Vector3> asphericalShapeArray(asphericalShapeProperty);
			ConstPropertyAccess<Quaternion> orientationArray(orientationProperty);
			ConstPropertyAccess<Color> colorsArray(colorBuffer);
			ConstPropertyAccess<FloatType> radiiArray(radiusBuffer);
			ConstPropertyAccess<FloatType> transparencies(transparencyProperty);
			const FloatType scalingFactor = radiusScaleFactor();
			for(int index = 0; index < effectiveParticleCount; index++) {
				int effectiveParticleIndex = activeParticleIndices ? activeParticleIndices[index] : index;
				const Point3& center = positionArray[effectiveParticleIndex];
				FloatType radius, length;
				if(asphericalShapeArray) {
					radius = std::abs(asphericalShapeArray[effectiveParticleIndex].x()) * scalingFactor;
					length = asphericalShapeArray[effectiveParticleIndex].z() * scalingFactor;
				}
				else {
					radius = radiiArray[effectiveParticleIndex] * scalingFactor;
					length = radius * 2;
				}
				Vector3 dir = Vector3(0, 0, length);
				if(orientationArray) {
					dir = orientationArray[effectiveParticleIndex] * dir;
				}
				Point3 p = center - (dir * FloatType(0.5));
				cylinderBasePositions[index] = p;
				cylinderHeadPositions[index] = p + dir;
				cylinderWidths[index] = 2 * radius;
				cylinderColors[index] = colorsArray[effectiveParticleIndex];
				if(cylinderTransparencies)
					cylinderTransparencies[index] = transparencies[effectiveParticleIndex];
				if(sphereRadii)
					sphereRadii[index] = radius;
			}
			visCache.cylinderPrimitive.setPositions(cylinderBasePositions.take(), cylinderHeadPositions.take());
			visCache.cylinderPrimitive.setWidths(cylinderWidths.take());
			visCache.cylinderPrimitive.setColors(cylinderColors.take());
			visCache.cylinderPrimitive.setTransparencies(cylinderTransparencies.take());

			// Create the rendering primitive for the spheres.
			if(shape == ParticleShape::Spherocylinder) {
				visCache.spheresPrimitives[0].setParticleShape(ParticlePrimitive::SphericalShape);
				visCache.spheresPrimitives[0].setShadingMode(ParticlePrimitive::NormalShading);
				visCache.spheresPrimitives[0].setRenderingQuality(ParticlePrimitive::HighQuality);
				visCache.spheresPrimitives[0].setPositions(visCache.cylinderPrimitive.basePositions());
				visCache.spheresPrimitives[0].setRadii(sphereRadii.take());
				visCache.spheresPrimitives[0].setColors(visCache.cylinderPrimitive.colors());
				visCache.spheresPrimitives[0].setTransparencies(visCache.cylinderPrimitive.transparencies());
				visCache.spheresPrimitives[1].setParticleShape(ParticlePrimitive::SphericalShape);
				visCache.spheresPrimitives[1].setShadingMode(ParticlePrimitive::NormalShading);
				visCache.spheresPrimitives[1].setRenderingQuality(ParticlePrimitive::HighQuality);
				visCache.spheresPrimitives[1].setPositions(visCache.cylinderPrimitive.headPositions());
				visCache.spheresPrimitives[1].setRadii(visCache.spheresPrimitives[0].radii());
				visCache.spheresPrimitives[1].setColors(visCache.cylinderPrimitive.colors());
				visCache.spheresPrimitives[1].setTransparencies(visCache.cylinderPrimitive.transparencies());
			}

			// Also create the corresponding picking record.
			visCache.pickInfo = new ParticlePickInfo(this, particles, activeParticleIndices.take());
		}
		if(!visCache.pickInfo)
			continue;

		// Update the pick info record with the latest particle data.
		visCache.pickInfo->setParticles(particles);

		// Render the particle primitive.
		if(renderer->isPicking()) renderer->beginPickObject(contextNode, visCache.pickInfo);
		renderer->renderCylinders(visCache.cylinderPrimitive);
		if(renderer->isPicking()) renderer->endPickObject();
		if(visCache.spheresPrimitives[0].positions()) {
			if(renderer->isPicking()) renderer->beginPickObject(contextNode, visCache.pickInfo);
			renderer->renderParticles(visCache.spheresPrimitives[0]);
			if(renderer->isPicking()) renderer->endPickObject();
			if(renderer->isPicking()) renderer->beginPickObject(contextNode, visCache.pickInfo);
			renderer->renderParticles(visCache.spheresPrimitives[1]);
			if(renderer->isPicking()) renderer->endPickObject();
		}
	}
}

/******************************************************************************
* Render a marker around a particle to highlight it in the viewports.
******************************************************************************/
void ParticlesVis::highlightParticle(size_t particleIndex, const ParticlesObject* particles, SceneRenderer* renderer) const
{
	if(!renderer->isBoundingBoxPass()) {

		// Fetch properties of selected particle which are needed to render the overlay.
		const PropertyObject* posProperty = nullptr;
		const PropertyObject* radiusProperty = nullptr;
		const PropertyObject* colorProperty = nullptr;
		const PropertyObject* selectionProperty = nullptr;
		const PropertyObject* shapeProperty = nullptr;
		const PropertyObject* orientationProperty = nullptr;
		const PropertyObject* roundnessProperty = nullptr;
		const PropertyObject* typeProperty = nullptr;
		for(const PropertyObject* property : particles->properties()) {
			if(property->type() == ParticlesObject::PositionProperty && property->size() >= particleIndex)
				posProperty = property;
			else if(property->type() == ParticlesObject::RadiusProperty && property->size() >= particleIndex)
				radiusProperty = property;
			else if(property->type() == ParticlesObject::TypeProperty && property->size() >= particleIndex)
				typeProperty = property;
			else if(property->type() == ParticlesObject::ColorProperty && property->size() >= particleIndex)
				colorProperty = property;
			else if(property->type() == ParticlesObject::SelectionProperty && property->size() >= particleIndex)
				selectionProperty = property;
			else if(property->type() == ParticlesObject::AsphericalShapeProperty && property->size() >= particleIndex)
				shapeProperty = property;
			else if(property->type() == ParticlesObject::OrientationProperty && property->size() >= particleIndex)
				orientationProperty = property;
			else if(property->type() == ParticlesObject::SuperquadricRoundnessProperty && property->size() >= particleIndex)
				roundnessProperty = property;
		}
		if(!posProperty || particleIndex >= posProperty->size())
			return;

		// Get the particle type.
		const ParticleType* ptype = nullptr;
		if(typeProperty && particleIndex < typeProperty->size()) {
			ConstPropertyAccess<int> typeArray(typeProperty);
			ptype = dynamic_object_cast<ParticleType>(typeProperty->elementType(typeArray[particleIndex]));
		}

		// Check if the particle must be rendered using a custom shape.
		if(ptype && ptype->shape() == ParticleShape::Mesh && ptype->shapeMesh())
			return;	// Note: Highlighting of particles with user-defined shapes is not implemented yet.

		// The rendering shape of the highlighted particle.
		ParticleShape shape = particleShape();
		if(ptype && ptype->shape() != ParticleShape::Default)
			shape = ptype->shape();

		// Determine position of the selected particle.
		Point3 pos = ConstPropertyAccess<Point3>(posProperty)[particleIndex];

		// Determine radius of selected particle.
		FloatType radius = particleRadius(particleIndex, radiusProperty, typeProperty);

		// Determine the display color of selected particle.
		Color color = particleColor(particleIndex, colorProperty, typeProperty, selectionProperty);
		Color highlightColor = selectionParticleColor();
		color = color * FloatType(0.5) + highlightColor * FloatType(0.5);

		// Determine rendering quality used to render the particles.
		ParticlePrimitive::RenderingQuality renderQuality = effectiveRenderingQuality(renderer, particles);

		ParticlePrimitive particleBuffer;
		ParticlePrimitive highlightParticleBuffer;
		CylinderPrimitive cylinderBuffer;
		CylinderPrimitive highlightCylinderBuffer;
		if(shape != Cylinder && shape != Spherocylinder) {
			// Determine effective particle shape and shading mode.
			ParticlePrimitive::ParticleShape primitiveParticleShape = effectiveParticleShape(shape, shapeProperty, orientationProperty, roundnessProperty);
			ParticlePrimitive::ShadingMode primitiveShadingMode = ParticlePrimitive::NormalShading;
			if(shape == ParticlesVis::Circle || shape == ParticlesVis::Square)
				primitiveShadingMode = ParticlePrimitive::FlatShading;

			// Prepare data buffers.
			DataBufferAccessAndRef<Point3> positionBuffer = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 3);
			positionBuffer[0] = pos;
			DataBufferAccessAndRef<Vector3> asphericalShapeBuffer;
			DataBufferAccessAndRef<Vector3> asphericalShapeBufferHighlight;
			if(shapeProperty) {
				asphericalShapeBuffer = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 3);
				asphericalShapeBufferHighlight = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 3);
				asphericalShapeBufferHighlight[0] = asphericalShapeBuffer[0] = ConstPropertyAccess<Vector3>(shapeProperty)[particleIndex];
				asphericalShapeBufferHighlight[0] += Vector3(renderer->viewport()->nonScalingSize(renderer->worldTransform() * pos) * FloatType(1e-1));
			}
			DataBufferAccessAndRef<Quaternion> orientationBuffer;
			if(orientationProperty) {
				orientationBuffer = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 4);
				orientationBuffer[0] = ConstPropertyAccess<Quaternion>(orientationProperty)[particleIndex];
			}
			DataBufferAccessAndRef<Vector2> roundnessBuffer;
			if(roundnessProperty) {
				roundnessBuffer = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 2);
				roundnessBuffer[0] = ConstPropertyAccess<Vector2>(roundnessProperty)[particleIndex];
			}

			particleBuffer.setParticleShape(primitiveParticleShape);
			particleBuffer.setShadingMode(primitiveShadingMode);
			particleBuffer.setRenderingQuality(renderQuality);
			particleBuffer.setUniformColor(color);
			particleBuffer.setPositions(positionBuffer.take());
			particleBuffer.setUniformRadius(radius);
			particleBuffer.setAsphericalShapes(asphericalShapeBuffer.take());
			particleBuffer.setOrientations(orientationBuffer.take());
			particleBuffer.setRoundness(roundnessBuffer.take());

			// Prepare marker geometry buffer.
			highlightParticleBuffer.setParticleShape(primitiveParticleShape);
			highlightParticleBuffer.setShadingMode(primitiveShadingMode);
			highlightParticleBuffer.setRenderingQuality(renderQuality);
			highlightParticleBuffer.setUniformColor(highlightColor);
			highlightParticleBuffer.setPositions(particleBuffer.positions());
			highlightParticleBuffer.setUniformRadius(radius + renderer->viewport()->nonScalingSize(renderer->worldTransform() * pos) * FloatType(1e-1));
			highlightParticleBuffer.setAsphericalShapes(asphericalShapeBufferHighlight.take());
			highlightParticleBuffer.setOrientations(particleBuffer.orientations());
			highlightParticleBuffer.setRoundness(particleBuffer.roundness());
		}
		else if(shape == Cylinder || shape == Spherocylinder) {
			FloatType radius, length;
			if(shapeProperty) {
				Vector3 shape = ConstPropertyAccess<Vector3>(shapeProperty)[particleIndex];
				radius = std::abs(shape.x());
				length = shape.z();
			}
			else {
				radius = defaultParticleRadius();
				length = radius * 2;
			}
			Vector3 dir = Vector3(0, 0, length);
			if(orientationProperty) {
				Quaternion q = ConstPropertyAccess<Quaternion>(orientationProperty)[particleIndex];
				dir = q * dir;
			}
			DataBufferAccessAndRef<Point3> positionBuffer1 = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 3);
			DataBufferAccessAndRef<Point3> positionBuffer2 = DataBufferPtr::create(dataset(), 1, DataBuffer::Float, 3);
			DataBufferAccessAndRef<Point3> positionBufferSpheres = DataBufferPtr::create(dataset(), 2, DataBuffer::Float, 3);
			positionBufferSpheres[0] = positionBuffer1[0] = pos - (dir * FloatType(0.5));
			positionBufferSpheres[1] = positionBuffer2[0] = pos + (dir * FloatType(0.5));
			cylinderBuffer.setShape(CylinderPrimitive::CylinderShape);
			cylinderBuffer.setShadingMode(CylinderPrimitive::NormalShading);
			highlightCylinderBuffer.setShape(CylinderPrimitive::CylinderShape);
			highlightCylinderBuffer.setShadingMode(CylinderPrimitive::NormalShading);
			cylinderBuffer.setUniformColor(color);
			cylinderBuffer.setUniformWidth(2 * radius);
			cylinderBuffer.setPositions(positionBuffer1.take(), positionBuffer2.take());
			FloatType padding = renderer->viewport()->nonScalingSize(renderer->worldTransform() * pos) * FloatType(1e-1);
			highlightCylinderBuffer.setUniformColor(highlightColor);
			highlightCylinderBuffer.setUniformWidth(2 * (radius + padding));
			highlightCylinderBuffer.setPositions(cylinderBuffer.basePositions(), cylinderBuffer.headPositions());
			if(shape == Spherocylinder) {
				particleBuffer.setParticleShape(ParticlePrimitive::SphericalShape);
				particleBuffer.setShadingMode(ParticlePrimitive::NormalShading);
				particleBuffer.setRenderingQuality(ParticlePrimitive::HighQuality);
				highlightParticleBuffer.setParticleShape(ParticlePrimitive::SphericalShape);
				highlightParticleBuffer.setShadingMode(ParticlePrimitive::NormalShading);
				highlightParticleBuffer.setRenderingQuality(ParticlePrimitive::HighQuality);
				particleBuffer.setPositions(positionBufferSpheres.take());
				particleBuffer.setUniformRadius(radius);
				particleBuffer.setUniformColor(color);
				highlightParticleBuffer.setPositions(particleBuffer.positions());
				highlightParticleBuffer.setUniformRadius(radius + padding);
				highlightParticleBuffer.setUniformColor(highlightColor);
			}
		}

		renderer->setHighlightMode(1);
		if(particleBuffer.positions())
			renderer->renderParticles(particleBuffer);
		if(cylinderBuffer.basePositions())
			renderer->renderCylinders(cylinderBuffer);
		renderer->setHighlightMode(2);
		if(highlightParticleBuffer.positions())
			renderer->renderParticles(highlightParticleBuffer);
		if(highlightCylinderBuffer.basePositions())
			renderer->renderCylinders(highlightCylinderBuffer);
		renderer->setHighlightMode(0);
	}
	else {
		// Fetch properties of selected particle needed to compute the bounding box.
		const PropertyObject* posProperty = nullptr;
		const PropertyObject* radiusProperty = nullptr;
		const PropertyObject* shapeProperty = nullptr;
		const PropertyObject* typeProperty = nullptr;
		for(const PropertyObject* property : particles->properties()) {
			if(property->type() == ParticlesObject::PositionProperty && property->size() >= particleIndex)
				posProperty = property;
			else if(property->type() == ParticlesObject::RadiusProperty && property->size() >= particleIndex)
				radiusProperty = property;
			else if(property->type() == ParticlesObject::AsphericalShapeProperty && property->size() >= particleIndex)
				shapeProperty = property;
			else if(property->type() == ParticlesObject::TypeProperty && property->size() >= particleIndex)
				typeProperty = property;
		}
		if(!posProperty)
			return;

		// Determine position of selected particle.
		Point3 pos = ConstPropertyAccess<Point3>(posProperty)[particleIndex];

		// Determine radius of selected particle.
		FloatType radius = particleRadius(particleIndex, radiusProperty, typeProperty);
		if(shapeProperty) {
			Vector3 shape = ConstPropertyAccess<Vector3>(shapeProperty)[particleIndex];
			radius = std::max(radius, shape.x());
			radius = std::max(radius, shape.y());
			radius = std::max(radius, shape.z());
			radius *= 2;
		}

		if(radius <= 0 || !renderer->viewport())
			return;

		const AffineTransformation& tm = renderer->worldTransform();
		renderer->addToLocalBoundingBox(Box3(pos, radius + renderer->viewport()->nonScalingSize(tm * pos) * FloatType(1e-1)));
	}
}

/******************************************************************************
* Given an sub-object ID returned by the Viewport::pick() method, looks up the
* corresponding particle index.
******************************************************************************/
size_t ParticlePickInfo::particleIndexFromSubObjectID(quint32 subobjID) const
{
	if(_subobjectToParticleMapping && subobjID < _subobjectToParticleMapping->size())
		return ConstDataBufferAccess<int>(_subobjectToParticleMapping)[subobjID];
	return subobjID;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString ParticlePickInfo::infoString(PipelineSceneNode* objectNode, quint32 subobjectId)
{
	size_t particleIndex = particleIndexFromSubObjectID(subobjectId);
	return particles()->elementInfoString(particleIndex);
}

}	// End of namespace
