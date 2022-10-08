////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include "NucleotidesVis.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(NucleotidesVis);
DEFINE_PROPERTY_FIELD(NucleotidesVis, cylinderRadius);
SET_PROPERTY_FIELD_LABEL(NucleotidesVis, cylinderRadius, "Cylinder radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(NucleotidesVis, cylinderRadius, WorldParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
NucleotidesVis::NucleotidesVis(ObjectCreationParams params) : ParticlesVis(params),
	_cylinderRadius(0.05)
{	
	setDefaultParticleRadius(0.1);
}

/******************************************************************************
* Computes the bounding box of the visual element.
******************************************************************************/
Box3 NucleotidesVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
	const ParticlesObject* particles = path.lastAs<ParticlesObject>();
	if(!particles) return {};
	particles->verifyIntegrity();
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	const PropertyObject* nucleotideAxisProperty = particles->getProperty(ParticlesObject::NucleotideAxisProperty);

	// The key type used for caching the computed bounding box:
	using CacheKey = std::tuple<
		ConstDataObjectRef,	// Position property
		ConstDataObjectRef,	// Nucleotide axis property
		FloatType 			// Default particle radius
	>;

	// Look up the bounding box in the vis cache.
	auto& bbox = dataset()->visCache().get<Box3>(CacheKey(
			positionProperty,
			nucleotideAxisProperty,
			defaultParticleRadius()));

	// Check if the cached bounding box information is still up to date.
	if(bbox.isEmpty()) {

		// If not, recompute bounding box from particle data.
		Box3 innerBox;
		if(ConstPropertyAccess<Point3> positionArray = positionProperty) {
			innerBox.addPoints(positionArray);
			if(ConstPropertyAccess<Vector3> axisArray = nucleotideAxisProperty) {
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
const PropertyObject* NucleotidesVis::getParticleTypeColorProperty(const ParticlesObject* particles) const
{
	return particles->getProperty(ParticlesObject::DNAStrandProperty);
}

/******************************************************************************
* Returns the typed particle property used to determine the rendering radii 
* of particles (if no per-particle radii are defined).
******************************************************************************/
const PropertyObject* NucleotidesVis::getParticleTypeRadiusProperty(const ParticlesObject* particles) const
{
	return particles->getProperty(ParticlesObject::TypeProperty);
}

/******************************************************************************
* Determines the effective rendering colors for the backbone sites of the nucleotides.
******************************************************************************/
ConstPropertyPtr NucleotidesVis::backboneColors(const ParticlesObject* particles, bool highlightSelection) const
{
	return particleColors(particles, highlightSelection);
}

/******************************************************************************
* Determines the effective rendering colors for the base sites of the nucleotides.
******************************************************************************/
ConstPropertyPtr NucleotidesVis::nucleobaseColors(const ParticlesObject* particles, bool highlightSelection) const
{
	particles->verifyIntegrity();

	// Allocate output color array.
	PropertyPtr output = ParticlesObject::OOClass().createStandardProperty(dataset(), particles->elementCount(), ParticlesObject::ColorProperty);

	Color defaultColor = defaultParticleColor();
	if(const PropertyObject* baseProperty = particles->getProperty(ParticlesObject::NucleobaseTypeProperty)) {
		// Assign colors based on base type.
		// Generate a lookup map for base type colors.
		const std::map<int,Color> colorMap = baseProperty->typeColorMap();
		std::array<Color,16> colorArray;
		// Check if all type IDs are within a small, non-negative range.
		// If yes, we can use an array lookup strategy. Otherwise we have to use a dictionary lookup strategy, which is slower.
		if(std::all_of(colorMap.begin(), colorMap.end(), [&colorArray](const auto& i) { return i.first >= 0 && i.first < (int)colorArray.size(); })) {
			colorArray.fill(defaultColor);
			for(const auto& entry : colorMap)
				colorArray[entry.first] = entry.second;
			// Fill color array.
			ConstPropertyAccess<int> typeArray(baseProperty);
			const int* t = typeArray.cbegin();
			for(Color& c : PropertyAccess<Color>(output)) {
				if(*t >= 0 && *t < (int)colorArray.size())
					c = colorArray[*t];
				else
					c = defaultColor;
				++t;
			}
		}
		else {
			// Fill color array.
			ConstPropertyAccess<int> typeArray(baseProperty);
			const int* t = typeArray.cbegin();
			for(Color& c : PropertyAccess<Color>(output)) {
				auto it = colorMap.find(*t);
				if(it != colorMap.end())
					c = it->second;
				else
					c = defaultColor;
				++t;
			}
		}
	}
	else {
		// Assign a uniform color to all base sites.
		output->fill(defaultColor);
	}

	// Highlight selected sites.
	if(const PropertyObject* selectionProperty = highlightSelection ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr)
		output->fillSelected(selectionParticleColor(), *selectionProperty);

	return output;
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus NucleotidesVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	if(renderer->isBoundingBoxPass()) {
		TimeInterval validityInterval;
		renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, validityInterval));
		return {};
	}

	// Get input data.
	const ParticlesObject* particles = path.lastAs<ParticlesObject>();
	if(!particles) return {};
	particles->verifyIntegrity();
	const PropertyObject* positionProperty = particles->getProperty(ParticlesObject::PositionProperty);
	if(!positionProperty) return {};
	const PropertyObject* colorProperty = particles->getProperty(ParticlesObject::ColorProperty);
	const PropertyObject* baseProperty = particles->getProperty(ParticlesObject::NucleobaseTypeProperty);
	const PropertyObject* strandProperty = particles->getProperty(ParticlesObject::DNAStrandProperty);
	const PropertyObject* selectionProperty = renderer->isInteractive() ? particles->getProperty(ParticlesObject::SelectionProperty) : nullptr;
	const PropertyObject* transparencyProperty = particles->getProperty(ParticlesObject::TransparencyProperty);
	const PropertyObject* nucleotideAxisProperty = particles->getProperty(ParticlesObject::NucleotideAxisProperty);
	const PropertyObject* nucleotideNormalProperty = particles->getProperty(ParticlesObject::NucleotideNormalProperty);

	// Make sure we don't exceed our internal limits.
	if(particles->elementCount() > (size_t)std::numeric_limits<int>::max()) {
		throwException(tr("Cannot render more than %1 nucleotides.").arg(std::numeric_limits<int>::max()));
	}

	// The type of lookup key used for caching the rendering primitives:
	using NucleotidesCacheKey = RendererResourceKey<struct NucleotidesVisCache,
		QPointer<PipelineSceneNode>,// Pipeline scene node
		ConstDataObjectRef,			// Position property
		ConstDataObjectRef,			// Color property
		ConstDataObjectRef,			// Strand property
		ConstDataObjectRef,			// Transparency property
		ConstDataObjectRef,			// Selection property
		ConstDataObjectRef,			// Nucleotide axis property
		ConstDataObjectRef,			// Nucleotide normal property
		FloatType,					// Default particle radius
		FloatType					// Cylinder radius
	>;
	
	// The data structure stored in the vis cache.
	struct NucleotidesCacheValue {
		ParticlePrimitive backbonePrimitive;
		CylinderPrimitive connectionPrimitive;
		ParticlePrimitive basePrimitive;
		OORef<ParticlePickInfo> pickInfo;
	};

	// Look up the rendering primitives in the vis cache.
	auto& visCache = dataset()->visCache().get<NucleotidesCacheValue>(NucleotidesCacheKey(
		const_cast<PipelineSceneNode*>(contextNode),
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
			DataBufferAccessAndRef<Point3> baseSites = DataBufferPtr::create(dataset(), particles->elementCount(), DataBuffer::Float, 3);
			ConstPropertyAccess<Point3> positionsArray(positionProperty);
			ConstPropertyAccess<Vector3> nucleotideAxisArray(nucleotideAxisProperty);
			for(size_t i = 0; i < baseSites.size(); i++)
				baseSites[i] = positionsArray[i] + (0.8 * nucleotideAxisArray[i]);
			visCache.basePrimitive.setPositions(baseSites.take());

			// Fill in base color data.
			visCache.basePrimitive.setColors(nucleobaseColors(particles, renderer->isInteractive()));

			// Fill in aspherical shape values.
			DataBufferPtr asphericalShapes = DataBufferPtr::create(dataset(), particles->elementCount(), DataBuffer::Float, 3);
			asphericalShapes->fill(cylinderRadius() * Vector3(2.0, 3.0, 1.0));
			visCache.basePrimitive.setAsphericalShapes(std::move(asphericalShapes));

			// Fill in base orientations.
			if(ConstPropertyAccess<Vector3> nucleotideNormalArray = nucleotideNormalProperty) {
				PropertyAccessAndRef<Quaternion> orientations = ParticlesObject::OOClass().createStandardProperty(dataset(), particles->elementCount(), ParticlesObject::OrientationProperty);
				for(size_t i = 0; i < orientations.size(); i++) {
					if(nucleotideNormalArray[i] != Vector3::Zero() && nucleotideAxisArray[i] != Vector3::Zero()) {
						// Build an orthonomal basis from the two direction vectors of a nucleotide.
						Matrix3 tm;
						tm.column(2) = nucleotideNormalArray[i];
						tm.column(1) = nucleotideAxisArray[i];
						tm.column(0) = tm.column(1).cross(tm.column(2));
						if(!tm.column(0).isZero()) {
							tm.orthonormalize();
							orientations[i] = Quaternion(tm);
						}
						else orientations[i] = Quaternion::Identity();
					}
					else {
						orientations[i] = Quaternion::Identity();
					}
				}
				visCache.basePrimitive.setOrientations(orientations.take());	
			}

			// Create the rendering primitive for the connections between backbone and base sites.
			visCache.connectionPrimitive.setShape(CylinderPrimitive::CylinderShape);
			visCache.connectionPrimitive.setShadingMode(CylinderPrimitive::NormalShading);
			visCache.connectionPrimitive.setUniformWidth(2 * cylinderRadius());
			visCache.connectionPrimitive.setColors(colors);
			DataBufferAccessAndRef<Point3> headPositions = DataBufferPtr::create(dataset(), particles->elementCount(), DataBuffer::Float, 3);
			for(size_t i = 0; i < positionsArray.size(); i++)
				headPositions[i] = positionsArray[i] + 0.8 * nucleotideAxisArray[i];
			visCache.connectionPrimitive.setPositions(positionProperty, headPositions.take());
		}
		else {
			visCache.connectionPrimitive = CylinderPrimitive();
			visCache.basePrimitive = ParticlePrimitive();
		}

		// Create pick info record.
		DataBufferAccessAndRef<int> subobjectToParticleMapping = DataBufferPtr::create(dataset(), nucleotideAxisProperty ? (particles->elementCount() * 3) : particles->elementCount(), DataBuffer::Int);
		std::iota(subobjectToParticleMapping.begin(), subobjectToParticleMapping.begin() + particles->elementCount(), 0);
		if(nucleotideAxisProperty) {
			std::iota(subobjectToParticleMapping.begin() +     particles->elementCount(), subobjectToParticleMapping.begin() + 2 * particles->elementCount(), 0);
			std::iota(subobjectToParticleMapping.begin() + 2 * particles->elementCount(), subobjectToParticleMapping.begin() + 3 * particles->elementCount(), 0);
		}
		visCache.pickInfo = new ParticlePickInfo(this, particles, subobjectToParticleMapping.take());
	}
	else {
		// Update the pipeline state stored in te picking object info.
		visCache.pickInfo->setParticles(particles);
	}

	if(renderer->isPicking())
		renderer->beginPickObject(contextNode, visCache.pickInfo);

	renderer->renderParticles(visCache.backbonePrimitive);
	if(visCache.connectionPrimitive.basePositions())
		renderer->renderCylinders(visCache.connectionPrimitive);
	if(visCache.basePrimitive.positions())
		renderer->renderParticles(visCache.basePrimitive);

	if(renderer->isPicking())
		renderer->endPickObject();

	return {};
}

}	// End of namespace
