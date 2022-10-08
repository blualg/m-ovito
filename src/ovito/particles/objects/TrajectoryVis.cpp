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
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include "TrajectoryVis.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(TrajectoryVis);
DEFINE_PROPERTY_FIELD(TrajectoryVis, lineWidth);
DEFINE_PROPERTY_FIELD(TrajectoryVis, lineColor);
DEFINE_PROPERTY_FIELD(TrajectoryVis, shadingMode);
DEFINE_PROPERTY_FIELD(TrajectoryVis, showUpToCurrentTime);
DEFINE_PROPERTY_FIELD(TrajectoryVis, wrappedLines);
DEFINE_PROPERTY_FIELD(TrajectoryVis, coloringMode);
DEFINE_REFERENCE_FIELD(TrajectoryVis, colorMapping);
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, lineWidth, "Line width");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, lineColor, "Line color");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, showUpToCurrentTime, "Show up to current time only");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, wrappedLines, "Wrap trajectory lines around");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, coloringMode, "Coloring mode");
SET_PROPERTY_FIELD_LABEL(TrajectoryVis, colorMapping, "Color mapping");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(TrajectoryVis, lineWidth, WorldParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
TrajectoryVis::TrajectoryVis(ObjectCreationParams params) : DataVis(params),
	_lineWidth(0.2),
	_lineColor(0.6, 0.6, 0.6),
	_shadingMode(FlatShading),
	_showUpToCurrentTime(false),
	_wrappedLines(false),
	_coloringMode(UniformColoring)
{
	if(params.createSubObjects()) {
		// Create a color mapping object for pseudo-color visualization of a trajectory property.
		setColorMapping(OORef<PropertyColorMapping>::create(params));
	}
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void TrajectoryVis::loadFromStreamComplete(ObjectLoadStream& stream)
{
	DataVis::loadFromStreamComplete(stream);

	// For backward compatibility with OVITO 3.5.4.
	// Create a color mapping sub-object if it wasn't loaded from the state file.
	if(!colorMapping())
		setColorMapping(OORef<PropertyColorMapping>::create(dataset()));
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TrajectoryVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
	const TrajectoryObject* trajObj = path.lastAs<TrajectoryObject>();

	// Get the simulation cell.
	const SimulationCellObject* simulationCell = wrappedLines() ? flowState.getObject<SimulationCellObject>() : nullptr;

	// The key type used for caching the computed bounding box:
	using CacheKey = RendererResourceKey<struct TrajectoryVisBoundBoxCache,
		ConstDataObjectRef,		// Trajectory object
		FloatType,				// Line width
		ConstDataObjectRef		// Simulation cell
	>;

	// Look up the bounding box in the vis cache.
	auto& bbox = dataset()->visCache().get<Box3>(CacheKey(trajObj, lineWidth(), simulationCell));

	// Check if the cached bounding box information is still up to date.
	if(bbox.isEmpty()) {
		// If not, recompute bounding box from trajectory data.
		if(trajObj) {
			if(!simulationCell) {
				if(ConstPropertyAccess<Point3> posProperty = trajObj->getProperty(TrajectoryObject::PositionProperty)) {
					bbox.addPoints(posProperty);
				}
			}
			else {
				bbox = Box3(Point3(0,0,0), Point3(1,1,1)).transformed(simulationCell->cellMatrix());
			}
			bbox = bbox.padBox(lineWidth() / 2);
		}
	}
	return bbox;
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus TrajectoryVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	PipelineStatus status;

	if(renderer->isBoundingBoxPass()) {
		TimeInterval validityInterval;
		renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, validityInterval));
		return status;
	}

	const TrajectoryObject* trajObj = path.lastAs<TrajectoryObject>();

	// Get the simulation cell.
	const SimulationCellObject* simulationCell = wrappedLines() ? flowState.getObject<SimulationCellObject>() : nullptr;

	// Look for selected pseudo-coloring property.
	const PropertyObject* pseudoColorProperty = nullptr;
	int pseudoColorPropertyComponent = 0;
	if(coloringMode() == PseudoColoring && colorMapping() && colorMapping()->sourceProperty() && !trajObj->getProperty(TrajectoryObject::ColorProperty)) {
		pseudoColorProperty = colorMapping()->sourceProperty().findInContainer(trajObj);
		if(!pseudoColorProperty) {
			status = PipelineStatus(PipelineStatus::Error, tr("The property with the name '%1' does not exist.").arg(colorMapping()->sourceProperty().name()));
		}
		else {
			if(colorMapping()->sourceProperty().vectorComponent() >= (int)pseudoColorProperty->componentCount()) {
				status = PipelineStatus(PipelineStatus::Error, tr("The vector component is out of range. The property '%1' has only %2 values per data element.").arg(colorMapping()->sourceProperty().name()).arg(pseudoColorProperty->componentCount()));
				pseudoColorProperty = nullptr;
			}
			pseudoColorPropertyComponent = std::max(0, colorMapping()->sourceProperty().vectorComponent());
		}
	}

	// The key type used for caching the rendering primitive:
	using CacheKey = RendererResourceKey<struct TrajectoryVisCache,
		ConstDataObjectRef,		// Trajectory data object
		FloatType,				// Line width
		Color,					// Line color,
		ShadingMode,			// Shading mode
		FloatType,				// End frame
		ConstDataObjectRef,		// Simulation cell
		ConstDataObjectRef,		// Pseudo-color property
		int						// Pseudo-color vector component
	>;

	// The data structure stored in the vis cache.
	struct CacheValue {
		CylinderPrimitive segments;
		ParticlePrimitive corners;
		ConstDataBufferPtr cornerPseudoColors;
	};

	FloatType endFrame = showUpToCurrentTime() ? dataset()->animationSettings()->timeToFrame(time) : std::numeric_limits<FloatType>::max();

	// Look up the rendering primitives in the vis cache.
	auto& visCache = dataset()->visCache().get<CacheValue>(CacheKey(
			trajObj,
			lineWidth(),
			lineColor(),
			shadingMode(),
			endFrame,
			simulationCell,
			pseudoColorProperty, 
			pseudoColorPropertyComponent));

	// The shading mode for corner spheres.
	ParticlePrimitive::ShadingMode cornerShadingMode = (shadingMode() == ShadingMode::NormalShading)
			? ParticlePrimitive::NormalShading : ParticlePrimitive::FlatShading;

	// Check if we already have valid rendering primitives that are up to date.
	if(!visCache.segments.basePositions() || !visCache.corners.positions()) {

		// Update the rendering primitives.
		visCache.segments.setPositions(nullptr, nullptr);
		visCache.corners.setPositions(nullptr);
		visCache.cornerPseudoColors.reset();

		FloatType lineDiameter = lineWidth();
		if(trajObj && lineDiameter > 0) {
			trajObj->verifyIntegrity();

			// Retrieve the line data stored in the TrajectoryObject.
			ConstPropertyAccess<Point3> posProperty = trajObj->getProperty(TrajectoryObject::PositionProperty);
			ConstPropertyAccess<int> timeProperty = trajObj->getProperty(TrajectoryObject::SampleTimeProperty);
			ConstPropertyAccess<qlonglong> idProperty = trajObj->getProperty(TrajectoryObject::ParticleIdentifierProperty);
			ConstPropertyAccess<Color> colorProperty = trajObj->getProperty(TrajectoryObject::ColorProperty);
			ConstPropertyAccess<void,true> pseudoColorArray = pseudoColorProperty;
			if(posProperty && timeProperty && idProperty && posProperty.size() >= 2) {

				// Determine the number of line segments and corner points to render.
				DataBufferAccessAndRef<Point3> cornerPoints = DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 3);
				DataBufferAccessAndRef<Point3> baseSegmentPoints = DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 3);
				DataBufferAccessAndRef<Point3> headSegmentPoints = DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 3);
				DataBufferAccessAndRef<Color> cornerColors = colorProperty ? DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 3) : nullptr;
				DataBufferAccessAndRef<Color> segmentColors = colorProperty ? DataBufferPtr::create(dataset(), 0, DataBuffer::Float, 3) : nullptr;
				DataBufferAccessAndRef<FloatType> cornerPseudoColors = pseudoColorArray ? DataBufferPtr::create(dataset(), 0, DataBuffer::Float) : nullptr;
				DataBufferAccessAndRef<FloatType> segmentPseudoColors = pseudoColorArray ? DataBufferPtr::create(dataset(), 0, DataBuffer::Float) : nullptr;
				const Point3* pos = posProperty.cbegin();
				const int* sampleTime = timeProperty.cbegin();
				const qlonglong* id = idProperty.cbegin();
				const Color* color = colorProperty ? colorProperty.cbegin() : nullptr;
				if(!simulationCell) {
					for(auto pos_end = pos + posProperty.size() - 1; pos != pos_end; ++pos, ++sampleTime, ++id) {
						if(id[0] == id[1] && sampleTime[1] <= endFrame) {
							baseSegmentPoints.push_back(pos[0]);
							headSegmentPoints.push_back(pos[1]);
							if(color) {
								segmentColors.push_back(color[0]);
								segmentColors.push_back(color[1]);
							}
							else if(pseudoColorArray) {
								segmentPseudoColors.push_back(pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 0, pseudoColorPropertyComponent));
								segmentPseudoColors.push_back(pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 1, pseudoColorPropertyComponent));
							}
							if(pos + 1 != pos_end && id[1] == id[2] && sampleTime[2] <= endFrame) {
								cornerPoints.push_back(pos[1]);
								if(color) cornerColors.push_back(color[1]);
								else if(pseudoColorArray) cornerPseudoColors.push_back(pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 1, pseudoColorPropertyComponent));
							}
						}
						if(color) ++color;
					}
				}
				else {
					for(auto pos_end = pos + posProperty.size() - 1; pos != pos_end; ++pos, ++sampleTime, ++id) {
						if(id[0] == id[1] && sampleTime[1] <= endFrame) {
							clipTrajectoryLine(pos[0], pos[1], simulationCell, [&](const Point3& p1, const Point3& p2, FloatType t1, FloatType t2) {
								baseSegmentPoints.push_back(p1);
								headSegmentPoints.push_back(p2);
								if(color) {
									segmentColors.push_back((FloatType(1) - t1) * color[0] + t1 * color[1]);
									segmentColors.push_back((FloatType(1) - t2) * color[0] + t2 * color[1]);
								}
								else if(pseudoColorArray) {
									FloatType ps1 = pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 0, pseudoColorPropertyComponent);
									FloatType ps2 = pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 1, pseudoColorPropertyComponent);
									segmentPseudoColors.push_back((FloatType(1) - t1) * ps1 + t1 * ps2);
									segmentPseudoColors.push_back((FloatType(1) - t2) * ps1 + t2 * ps2);
								}
							});
							if(pos + 1 != pos_end && id[1] == id[2] && sampleTime[2] <= endFrame) {
								cornerPoints.push_back(simulationCell->wrapPoint(pos[1]));
								if(color) cornerColors.push_back(color[1]);
								else if(pseudoColorArray) cornerPseudoColors.push_back(pseudoColorArray.get<FloatType>(pos - posProperty.cbegin() + 1, pseudoColorPropertyComponent));
							}
						}
						if(color) ++color;
					}
				}

				// Create rendering primitive for the line segments.
				visCache.segments.setShape(CylinderPrimitive::CylinderShape);
				visCache.segments.setShadingMode(static_cast<CylinderPrimitive::ShadingMode>(shadingMode()));
				visCache.segments.setColors(segmentColors ? segmentColors.take() : segmentPseudoColors.take());
				visCache.segments.setUniformColor(lineColor());
				visCache.segments.setUniformWidth(lineDiameter);
				visCache.segments.setPositions(baseSegmentPoints.take(), headSegmentPoints.take());

				// Create rendering primitive for the corner points.
				visCache.corners.setParticleShape(ParticlePrimitive::SphericalShape);
				visCache.corners.setShadingMode(cornerShadingMode);
				visCache.corners.setRenderingQuality(ParticlePrimitive::HighQuality);
				visCache.corners.setPositions(cornerPoints.take());
				visCache.corners.setUniformColor(lineColor());
				visCache.corners.setColors(cornerColors.take());
				visCache.corners.setUniformRadius(0.5 * lineDiameter);

				// Save the pseudo-colors of the corner spheres. They will be converted to RGB colors below.
				visCache.cornerPseudoColors = cornerPseudoColors.take();
			}
		}
	}

	if(!visCache.segments.basePositions())
		return status;

	// Update the color mapping.
	visCache.segments.setPseudoColorMapping(colorMapping()->pseudoColorMapping());

	// Convert the pseudocolors of the corner spheres to RGB colors if necessary.
	if(visCache.cornerPseudoColors) {
		// Perform a cache lookup to check if latest pseudocolors have already been mapped to RGB colors.
		auto& cornerColorsUpToDate = dataset()->visCache().get<bool>(std::make_pair(visCache.cornerPseudoColors, visCache.segments.pseudoColorMapping()));
		if(!cornerColorsUpToDate) {
			// Create an RGB color array, which will be filled and then assigned to the ParticlesPrimitive.
			DataBufferAccessAndRef<Color> cornerColorsArray = DataBufferPtr::create(dataset(), visCache.cornerPseudoColors->size(), DataBuffer::Float, 3);
			boost::transform(ConstDataBufferAccess<FloatType>(visCache.cornerPseudoColors), cornerColorsArray.begin(), [&](FloatType v) { return visCache.segments.pseudoColorMapping().valueToColor(v); });
			visCache.corners.setColors(cornerColorsArray.take());
			cornerColorsUpToDate = true;
		}
	}

	renderer->beginPickObject(contextNode);
	renderer->renderCylinders(visCache.segments);
	renderer->renderParticles(visCache.corners);
	renderer->endPickObject();

	return status;
}

/******************************************************************************
* Clips a trajectory line at the periodic box boundaries.
******************************************************************************/
void TrajectoryVis::clipTrajectoryLine(const Point3& v1, const Point3& v2, const SimulationCellObject* simulationCell, const std::function<void(const Point3&, const Point3&, FloatType, FloatType)>& segmentCallback)
{
	OVITO_ASSERT(simulationCell);

	Point3 rp1 = simulationCell->absoluteToReduced(v1);
	Vector3 shiftVector = Vector3::Zero();
	for(size_t dim = 0; dim < 3; dim++) {
		if(simulationCell->hasPbcCorrected(dim)) {
			while(rp1[dim] >= 1) { rp1[dim] -= 1; shiftVector[dim] -= 1; }
			while(rp1[dim] < 0) { rp1[dim] += 1; shiftVector[dim] += 1; }
		}
	}
	Point3 rp2 = simulationCell->absoluteToReduced(v2) + shiftVector;
	FloatType t1 = 0;
	FloatType smallestT;
	bool clippedDimensions[3] = { false, false, false };
	do {
		size_t crossDim;
		FloatType crossDir;
		smallestT = FLOATTYPE_MAX;
		for(size_t dim = 0; dim < 3; dim++) {
			if(simulationCell->hasPbcCorrected(dim) && !clippedDimensions[dim]) {
				int d = (int)std::floor(rp2[dim]) - (int)std::floor(rp1[dim]);
				if(d == 0) continue;
				FloatType t;
				if(d > 0)
					t = (std::ceil(rp1[dim]) - rp1[dim]) / (rp2[dim] - rp1[dim]);
				else
					t = (std::floor(rp1[dim]) - rp1[dim]) / (rp2[dim] - rp1[dim]);
				if(t >= 0 && t < smallestT) {
					smallestT = t;
					crossDim = dim;
					crossDir = (d > 0) ? 1 : -1;
				}
			}
		}
		if(smallestT != FLOATTYPE_MAX) {
			clippedDimensions[crossDim] = true;
			Point3 intersection = rp1 + smallestT * (rp2 - rp1);
			intersection[crossDim] = std::round(intersection[crossDim]);
			FloatType t2 = (FloatType(1) - smallestT) * t1 + smallestT;
			Point3 rp1abs = simulationCell->reducedToAbsolute(rp1);
			Point3 intabs = simulationCell->reducedToAbsolute(intersection);
			if(!intabs.equals(rp1abs)) {
				OVITO_ASSERT(t2 <= FloatType(1) + FLOATTYPE_EPSILON);
				segmentCallback(rp1abs, intabs, t1, t2);
			}
			shiftVector[crossDim] -= crossDir;
			rp1 = intersection;
			rp1[crossDim] -= crossDir;
			rp2[crossDim] -= crossDir;
			t1 = t2;
		}
	}
	while(smallestT != FLOATTYPE_MAX);

	segmentCallback(simulationCell->reducedToAbsolute(rp1), simulationCell->reducedToAbsolute(rp2), t1, 1);
}

}	// End of namespace
