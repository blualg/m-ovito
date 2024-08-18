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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include "DislocationVis.h"
#include "RenderableDislocationLines.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationVis);
OVITO_CLASSINFO(DislocationVis, "DisplayName", "Dislocations");
DEFINE_PROPERTY_FIELD(DislocationVis, lineWidth);
DEFINE_PROPERTY_FIELD(DislocationVis, shadingMode);
DEFINE_PROPERTY_FIELD(DislocationVis, burgersVectorWidth);
DEFINE_PROPERTY_FIELD(DislocationVis, burgersVectorScaling);
DEFINE_PROPERTY_FIELD(DislocationVis, burgersVectorColor);
DEFINE_PROPERTY_FIELD(DislocationVis, showBurgersVectors);
DEFINE_PROPERTY_FIELD(DislocationVis, showLineDirections);
DEFINE_PROPERTY_FIELD(DislocationVis, lineColoringMode);
SET_PROPERTY_FIELD_LABEL(DislocationVis, lineWidth, "Line width");
SET_PROPERTY_FIELD_LABEL(DislocationVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(DislocationVis, burgersVectorWidth, "Burgers vector width");
SET_PROPERTY_FIELD_LABEL(DislocationVis, burgersVectorScaling, "Burgers vector scaling");
SET_PROPERTY_FIELD_LABEL(DislocationVis, burgersVectorColor, "Burgers vector color");
SET_PROPERTY_FIELD_LABEL(DislocationVis, showBurgersVectors, "Show Burgers vectors");
SET_PROPERTY_FIELD_LABEL(DislocationVis, showLineDirections, "Indicate line directions");
SET_PROPERTY_FIELD_LABEL(DislocationVis, lineColoringMode, "Line coloring");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationVis, lineWidth, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationVis, burgersVectorWidth, WorldParameterUnit, 0);

IMPLEMENT_ABSTRACT_OVITO_CLASS(DislocationPickInfo);

/******************************************************************************
* Transforms the DislocationNetwork into a renderable set of lines.
******************************************************************************/
Future<std::shared_ptr<const RenderableDislocationLines>> DislocationVis::transformDislocations(const DislocationNetwork* dislocations)
{
    // The actual work can be performed in a separate thread.
    return asyncLaunch([dislocations = DataOORef<const DislocationNetwork>(dislocations)]() {

        // Get the simulation cell (must be 3D).
        const SimulationCell* cellObject = dislocations->domain();
        if(!cellObject || cellObject->is2D())
            throw Exception(tr("Display of the dislocation line network requires a 3D simulation cell."));

        // Generate the list of clipped line segments.
        std::vector<RenderableDislocationLines::Segment> outputSegments;
        const ClusterGraph* clusterGraph = dislocations->clusterGraph();

        // Convert the dislocations object.
        int segmentIndex = 0;
        for(const DislocationSegment* segment : dislocations->segments()) {
            const ClusterVector& b = segment->burgersVector;
            // Determine the Burgers vector family the dislocation segment belongs to.
            if(const MicrostructurePhase* phase = dislocations->structureById(b.cluster()->structure)) {
                const BurgersVectorFamily* family = phase->defaultBurgersVectorFamily();
                for(const BurgersVectorFamily* f : phase->burgersVectorFamilies()) {
                    if(f->isMember(b.localVec(), phase)) {
                        family = f;
                        break;
                    }
                }
                // Don't render dislocation segment if the Burgers vector family has been disabled.
                if(family && !family->enabled()) {
                    segmentIndex++;
                    continue;
                }
            }
            clipDislocationLine(segment->line, *cellObject, dislocations->cuttingPlanes(), [segmentIndex, &outputSegments, &b](const Point3& p1, const Point3& p2, bool isInitialSegment) {
                outputSegments.push_back({ { p1, p2 }, b.localVec(), b.cluster()->id, segmentIndex });
            });
            segmentIndex++;
        }

        // Create output RenderableDislocationLines object.
        return std::make_shared<const RenderableDislocationLines>(std::move(outputSegments), std::move(clusterGraph));
    });
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 DislocationVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const DislocationNetwork* dislocations = path.lastAs<DislocationNetwork>();
    if(!dislocations)
        return {};
    const SimulationCell* cellObject = dislocations->domain();
    if(!cellObject || cellObject->is2D())
        return {};

    // Compute bounding box from dislocation data.
    Box3 bb = Box3(Point3(0,0,0), Point3(1,1,1)).transformed(cellObject->cellMatrix());
    FloatType padding = std::max(lineWidth(), FloatType(0));

    if(showBurgersVectors()) {
        padding = std::max(padding, burgersVectorWidth() * FloatType(2));
        for(const DislocationSegment* segment : dislocations->segments()) {
            Point3 center = cellObject->wrapPoint(segment->getPointOnLine(FloatType(0.5)));
            Vector3 dir = burgersVectorScaling() * segment->burgersVector.toSpatialVector();
            bb.addPoint(center + dir);
        }
    }

    return bb.padBox(padding * FloatType(0.5));
}

/******************************************************************************
* Lets the vis element render a data object.
******************************************************************************/
PipelineStatus DislocationVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Get the dislocation network to be rendered.
    const DislocationNetwork* dislocations = path.lastAs<DislocationNetwork>();
    if(!dislocations)
        return {};

    // Get the simulation cell.
    const SimulationCell* cellObject = dislocations->domain();
    if(!cellObject)
        return {};

    // Look up the asynchronous task that generates the renderable lines.
    const auto& renderableLinesFuture = frameGraph.visCache().lookup<SharedFuture<std::shared_ptr<const RenderableDislocationLines>>>(
        RendererResourceKey<struct RenderableLinesCache, ConstDataObjectRef>{ path.back() },
        [&](SharedFuture<std::shared_ptr<const RenderableDislocationLines>>& renderableLinesFuture) {
            // Start generating the renderable lines.
            renderableLinesFuture = transformDislocations(dislocations);
            registerActiveFuture(renderableLinesFuture);
        });

    // Wait for the renderable lines to be generated.
    std::shared_ptr<const RenderableDislocationLines> renderableLines = renderableLinesFuture.result();

    // Make sure we don't exceed our internal limits.
    if(renderableLines->lineSegments().size() > (size_t)std::numeric_limits<int>::max())
        throw Exception(tr("Cannot render more than %1 dislocation segments.").arg(std::numeric_limits<int>::max()));

    // The key type used for caching the rendering primitives:
    using CacheKey = RendererResourceKey<struct DislocationVisCache,
        std::shared_ptr<const RenderableDislocationLines>,     // Renderable object
        ConstDataObjectRef,     // Simulation cell geometry
        FloatType,              // Line width
        bool,                   // Burgers vector display
        FloatType,              // Burgers vectors scaling
        FloatType,              // Burgers vector width
        Color,                  // Burgers vector color
        bool,                   // Indicate line directions
        LineColoringMode,       // Way to color lines
        CylinderPrimitive::ShadingMode  // Line shading mode
    >;

    // The values stored in the vis cache.
    struct CacheValue {
        CylinderPrimitive segments;
        ParticlePrimitive corners;
        CylinderPrimitive burgersArrows;
        OORef<DislocationPickInfo> pickInfo;
    };

    // Lookup the rendering primitives in the vis cache.
    const auto& primitives = frameGraph.visCache().lookup<CacheValue>(
        CacheKey(
            renderableLines,
            cellObject,
            lineWidth(),
            showBurgersVectors(),
            burgersVectorScaling(),
            burgersVectorWidth(),
            burgersVectorColor(),
            showLineDirections(),
            lineColoringMode(),
            shadingMode()),
        [&](CacheValue& primitives) {

            // First determine number of corner vertices/segments that are going to be rendered.
            int lineSegmentCount = renderableLines->lineSegments().size();
            int cornerCount = 0;
            for(size_t i = 1; i < renderableLines->lineSegments().size(); i++) {
                const auto& s1 = renderableLines->lineSegments()[i-1];
                const auto& s2 = renderableLines->lineSegments()[i];
                if(s1.verts[1].equals(s2.verts[0])) cornerCount++;
            }
            // Allocate rendering data buffers.
            std::vector<int> subobjToSegmentMap(lineSegmentCount + cornerCount);
            FloatType lineDiameter = std::max(lineWidth(), FloatType(0));
            BufferFactory<Point3G> cornerPoints(cornerCount);
            BufferFactory<ColorG> cornerColors(cornerCount);
            BufferFactory<Point3G> baseSegmentPoints(lineSegmentCount);
            BufferFactory<Point3G> headSegmentPoints(lineSegmentCount);
            BufferFactory<ColorG> segmentColors(lineSegmentCount);

            // Build list of line segments.
            auto cornerPointsIter = cornerPoints.begin();
            auto cornerColorsIter = cornerColors.begin();
            ColorG lineColor;
            Vector3 normalizedBurgersVector;
            Vector3 lastBurgersVector = Vector3::Zero();
            int lastRegion = -1;
            int lastDislocationIndex = -1;
            const DislocationSegment* lastInputDislocationSegment = nullptr;
            for(size_t lineSegmentIndex = 0; lineSegmentIndex < renderableLines->lineSegments().size(); lineSegmentIndex++) {
                const auto& lineSegment = renderableLines->lineSegments()[lineSegmentIndex];
                if(lineSegment.burgersVector != lastBurgersVector || lineSegment.region != lastRegion) {
                    lastBurgersVector = lineSegment.burgersVector;
                    lastRegion = lineSegment.region;
                    lineColor = ColorG(0.8f, 0.8f, 0.8f);
                    const MicrostructurePhase* phase = nullptr;
                    if(dislocations && renderableLines->clusterGraph()) {
                        Cluster* cluster = renderableLines->clusterGraph()->findCluster(lineSegment.region);
                        OVITO_ASSERT(cluster != nullptr);
                        phase = dislocations->structureById(cluster->structure);
                        normalizedBurgersVector = ClusterVector(lineSegment.burgersVector, cluster).toSpatialVector();
                        normalizedBurgersVector.normalizeSafely();
                    }
                    if(phase) {
                        if(lineColoringMode() == ColorByDislocationType) {
                            const BurgersVectorFamily* family = phase->defaultBurgersVectorFamily();
                            for(const BurgersVectorFamily* f : phase->burgersVectorFamilies()) {
                                if(f->isMember(lineSegment.burgersVector, phase)) {
                                    family = f;
                                    break;
                                }
                            }
                            if(family)
                                lineColor = family->color().toDataType<GraphicsFloatType>();
                        }
                        else if(lineColoringMode() == ColorByBurgersVector) {
                            lineColor = MicrostructurePhase::getBurgersVectorColor(phase->name(), lineSegment.burgersVector).toDataType<GraphicsFloatType>();
                        }
                    }
                }
                subobjToSegmentMap[lineSegmentIndex] = lineSegment.dislocationIndex;
                ColorG segmentColor = lineColor;
                if(lineColoringMode() == ColorByCharacter) {
                    Vector3 delta = lineSegment.verts[1] - lineSegment.verts[0];
                    FloatType dot = std::abs(delta.dot(normalizedBurgersVector));
                    if(dot != 0) dot /= delta.length();
                    if(dot > 1) dot = 1;
                    FloatType angle = std::acos(dot) / (FLOATTYPE_PI/2);
                    if(angle <= FloatType(0.5))
                        segmentColor = ColorG(1, angle * 2, angle * 2);
                    else
                        segmentColor = ColorG((FloatType(1)-angle) * 2, (FloatType(1)-angle) * 2, 1);
                }
                if(dislocations) {
                    if(lastDislocationIndex != lineSegment.dislocationIndex) {
                        lastDislocationIndex = lineSegment.dislocationIndex;
                        const auto& segmentList = dislocations->segments();
                        lastInputDislocationSegment = (lastDislocationIndex >= 0 && lastDislocationIndex < segmentList.size()) ?
                            segmentList[lastDislocationIndex] : nullptr;
                    }
                    if(lastInputDislocationSegment) {
                        if(lastInputDislocationSegment->customColor.r() >= 0 && lastInputDislocationSegment->customColor.g() >= 0 && lastInputDislocationSegment->customColor.b() >= 0) {
                            segmentColor = lastInputDislocationSegment->customColor.toDataType<GraphicsFloatType>();
                        }
                    }
                }
                baseSegmentPoints[lineSegmentIndex] = lineSegment.verts[0].toDataType<GraphicsFloatType>();
                headSegmentPoints[lineSegmentIndex] = lineSegment.verts[1].toDataType<GraphicsFloatType>();
                segmentColors[lineSegmentIndex] = segmentColor;
                if(lineSegmentIndex != 0 && lineSegment.verts[0].equals(renderableLines->lineSegments()[lineSegmentIndex-1].verts[1])) {
                    subobjToSegmentMap[(cornerPointsIter - cornerPoints.begin()) + lineSegmentCount] = lineSegment.dislocationIndex;
                    *cornerPointsIter++ = lineSegment.verts[0].toDataType<GraphicsFloatType>();
                    *cornerColorsIter++ = segmentColor;
                }
            }
            OVITO_ASSERT(cornerPointsIter == cornerPoints.end());

            // Create rendering primitive for the line segments.
            primitives.segments.setShape(showLineDirections() ? CylinderPrimitive::ArrowShape : CylinderPrimitive::CylinderShape);
            primitives.segments.setShadingMode(shadingMode());
            primitives.segments.setUniformWidth(lineDiameter);
            primitives.segments.setPositions(baseSegmentPoints.take(), headSegmentPoints.take());
            primitives.segments.setColors(segmentColors.take());

            // Create rendering primitive for the line corner points.
            primitives.corners.setParticleShape(ParticlePrimitive::SphericalShape);
            primitives.corners.setShadingMode((shadingMode() == CylinderPrimitive::NormalShading) ? ParticlePrimitive::NormalShading : ParticlePrimitive::FlatShading);
            primitives.corners.setRenderingQuality(ParticlePrimitive::HighQuality);
            primitives.corners.setPositions(cornerPoints.take());
            primitives.corners.setColors(cornerColors.take());
            primitives.corners.setUniformRadius(0.5 * lineDiameter);

            if(dislocations) {
                if(showBurgersVectors()) {
                    BufferFactory<Point3G> baseArrowPoints(dislocations->segments().size());
                    BufferFactory<Point3G> headArrowPoints(dislocations->segments().size());
                    subobjToSegmentMap.reserve(subobjToSegmentMap.size() + dislocations->segments().size());
                    int arrowIndex = 0;
                    for(const DislocationSegment* segment : dislocations->segments()) {
                        subobjToSegmentMap.push_back(arrowIndex);
                        Point3 center = cellObject->wrapPoint(segment->getPointOnLine(FloatType(0.5)));
                        Vector3 dir = burgersVectorScaling() * segment->burgersVector.toSpatialVector();
                        // Check if arrow is clipped away by cutting planes.
                        if(dislocations->isPointCulled(center))
                            dir.setZero(); // Hide arrow by setting length to zero.
                        baseArrowPoints[arrowIndex] = center.toDataType<GraphicsFloatType>();
                        headArrowPoints[arrowIndex] = baseArrowPoints[arrowIndex] + dir.toDataType<GraphicsFloatType>();
                        arrowIndex++;
                    }
                    // Create rendering primitive for the Burgers vector arrows.
                    primitives.burgersArrows.setShape(CylinderPrimitive::ArrowShape);
                    primitives.burgersArrows.setShadingMode(shadingMode());
                    primitives.burgersArrows.setUniformWidth(std::max(burgersVectorWidth(), FloatType(0)));
                    primitives.burgersArrows.setUniformColor(burgersVectorColor());
                    primitives.burgersArrows.setPositions(baseArrowPoints.take(), headArrowPoints.take());
                }
                primitives.pickInfo = OORef<DislocationPickInfo>::create(this, dislocations, std::move(subobjToSegmentMap));
            }
        });

    // Take simulation cell as bounding box of dislocation lines.
    Box3 bb = Box3(Point3(0), Point3(1)).transformed(cellObject->cellMatrix()).padBox(std::max(lineWidth(), FloatType(0)) * FloatType(0.5));

    auto pickingGroup = frameGraph.addPickingGroup(pipeline, primitives.pickInfo);

    // Render dislocation segments.
    frameGraph.addPrimitive(std::make_unique<CylinderPrimitive>(primitives.segments), pipeline, pickingGroup, bb);

    // Render segment vertices.
    frameGraph.addPrimitive(std::make_unique<ParticlePrimitive>(primitives.corners), pipeline, pickingGroup, bb);

    // Render Burgers vectors.
    if(showBurgersVectors() && primitives.burgersArrows.basePositions())
        frameGraph.addPrimitive(std::make_unique<CylinderPrimitive>(primitives.burgersArrows), pipeline, pickingGroup);

    return {};
}

/******************************************************************************
* Renders an overlay marker for a single dislocation segment.
******************************************************************************/
void DislocationVis::renderOverlayMarker(const DataObject* dataObject, const PipelineFlowState& flowState, int segmentIndex, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Get the dislocations.
    const DislocationNetwork* dislocations = dynamic_object_cast<DislocationNetwork>(dataObject);
    if(!dislocations)
        return;

    // Get the simulation cell.
    const SimulationCell* cellObject = dislocations->domain();
    if(!cellObject)
        return;

    if(segmentIndex < 0 || segmentIndex >= dislocations->segments().size())
        return;

    const DislocationSegment* segment = dislocations->segments()[segmentIndex];

    // Generate the polyline segments to render.
    BufferFactory<Point3G> baseSegmentPoints(0);
    BufferFactory<Point3G> headSegmentPoints(0);
    BufferFactory<Point3G> cornerVertices(0);
    clipDislocationLine(segment->line, *cellObject, dislocations->cuttingPlanes(), [&](const Point3& v1, const Point3& v2, bool isInitialSegment) {
        baseSegmentPoints.push_back(v1.toDataType<GraphicsFloatType>());
        headSegmentPoints.push_back(v2.toDataType<GraphicsFloatType>());
        if(!isInitialSegment)
            cornerVertices.push_back(v1.toDataType<GraphicsFloatType>());
    });

    FloatType lineDiameter = std::max(lineWidth() / 2, FloatType(0));
    FloatType headRadius = lineDiameter * (3.0/2.0);

    auto previousRenderLayer = frameGraph.setCurrentRenderLayer(FrameGraph::RenderLayer::OverLayer);

    std::unique_ptr<CylinderPrimitive> segmentBuffer = std::make_unique<CylinderPrimitive>();
    segmentBuffer->setShape(CylinderPrimitive::CylinderShape);
    segmentBuffer->setShadingMode(CylinderPrimitive::FlatShading);
    segmentBuffer->setUniformWidth(lineDiameter);
    segmentBuffer->setPositions(baseSegmentPoints.take(), headSegmentPoints.take());
    segmentBuffer->setUniformColor(Color(1,1,1));
    frameGraph.addPrimitive(std::move(segmentBuffer), pipeline);

    std::unique_ptr<ParticlePrimitive> cornerBuffer = std::make_unique<ParticlePrimitive>();
    cornerBuffer->setParticleShape(ParticlePrimitive::SphericalShape);
    cornerBuffer->setShadingMode(ParticlePrimitive::FlatShading);
    cornerBuffer->setRenderingQuality(ParticlePrimitive::HighQuality);
    cornerBuffer->setPositions(cornerVertices.take());
    cornerBuffer->setUniformColor(Color(1,1,1));
    cornerBuffer->setUniformRadius(0.5 * lineDiameter);
    frameGraph.addPrimitive(std::move(cornerBuffer), pipeline);

    if(!segment->line.empty()) {
        BufferFactory<Point3G> wrappedHeadPos(1);
        wrappedHeadPos[0] = cellObject->wrapPoint(segment->line.front()).toDataType<GraphicsFloatType>();
        std::unique_ptr<ParticlePrimitive> headBuffer = std::make_unique<ParticlePrimitive>();
        headBuffer->setShadingMode(ParticlePrimitive::FlatShading);
        headBuffer->setRenderingQuality(ParticlePrimitive::HighQuality);
        headBuffer->setPositions(wrappedHeadPos.take());
        headBuffer->setUniformColor(Color(1,1,1));
        headBuffer->setUniformRadius(headRadius);
        frameGraph.addPrimitive(std::move(headBuffer), pipeline);
    }

    frameGraph.setCurrentRenderLayer(previousRenderLayer);
}

/******************************************************************************
* Clips a dislocation line at the periodic box boundaries.
******************************************************************************/
void DislocationVis::clipDislocationLine(const std::deque<Point3>& line, const SimulationCell& simulationCell, const QVector<Plane3>& clippingPlanes, const std::function<void(const Point3&, const Point3&, bool)>& segmentCallback)
{
    bool isInitialSegment = true;
    auto clippingFunction = [&clippingPlanes, &segmentCallback, &isInitialSegment](Point3 p1, Point3 p2) {
        bool isClipped = false;
        for(const Plane3& plane : clippingPlanes) {
            FloatType c1 = plane.pointDistance(p1);
            FloatType c2 = plane.pointDistance(p2);
            if(c1 >= 0 && c2 >= 0.0) {
                isClipped = true;
                break;
            }
            else if(c1 > FLOATTYPE_EPSILON && c2 < -FLOATTYPE_EPSILON) {
                p1 += (p2 - p1) * (c1 / (c1 - c2));
            }
            else if(c1 < -FLOATTYPE_EPSILON && c2 > FLOATTYPE_EPSILON) {
                p2 += (p1 - p2) * (c2 / (c2 - c1));
            }
        }
        if(!isClipped) {
            segmentCallback(p1, p2, isInitialSegment);
            isInitialSegment = false;
        }
    };

    auto v1 = line.cbegin();
    Point3 rp1 = simulationCell.absoluteToReduced(*v1);
    Vector3 shiftVector = Vector3::Zero();
    for(size_t dim = 0; dim < 3; dim++) {
        if(simulationCell.hasPbc(dim)) {
            while(rp1[dim] > 0) { rp1[dim] -= 1; shiftVector[dim] -= 1; }
            while(rp1[dim] < 0) { rp1[dim] += 1; shiftVector[dim] += 1; }
        }
    }
    for(auto v2 = v1 + 1; v2 != line.cend(); v1 = v2, ++v2) {
        Point3 rp2 = simulationCell.absoluteToReduced(*v2) + shiftVector;
        FloatType smallestT;
        bool clippedDimensions[3] = { false, false, false };
        do {
            size_t crossDim;
            FloatType crossDir;
            smallestT = FLOATTYPE_MAX;
            for(size_t dim = 0; dim < 3; dim++) {
                if(simulationCell.hasPbc(dim) && !clippedDimensions[dim]) {
                    int d = (int)floor(rp2[dim]) - (int)floor(rp1[dim]);
                    if(d == 0) continue;
                    FloatType t;
                    if(d > 0)
                        t = (ceil(rp1[dim]) - rp1[dim]) / (rp2[dim] - rp1[dim]);
                    else
                        t = (floor(rp1[dim]) - rp1[dim]) / (rp2[dim] - rp1[dim]);
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
                intersection[crossDim] = floor(intersection[crossDim] + FloatType(0.5));
                Point3 rp1abs = simulationCell.reducedToAbsolute(rp1);
                Point3 intabs = simulationCell.reducedToAbsolute(intersection);
                if(!intabs.equals(rp1abs)) {
                    clippingFunction(rp1abs, intabs);
                }
                shiftVector[crossDim] -= crossDir;
                rp1 = intersection;
                rp1[crossDim] -= crossDir;
                rp2[crossDim] -= crossDir;
                isInitialSegment = true;
            }
        }
        while(smallestT != FLOATTYPE_MAX);

        clippingFunction(simulationCell.reducedToAbsolute(rp1), simulationCell.reducedToAbsolute(rp2));
        rp1 = rp2;
    }
}

/******************************************************************************
* Checks if the given floating point number is integer.
******************************************************************************/
static bool isInteger(FloatType v, int& intPart)
{
    static const FloatType epsilon = FloatType(1e-2);
    FloatType ip;
    FloatType frac = std::modf(v, &ip);
    if(frac >= -epsilon && frac <= epsilon) intPart = (int)ip;
    else if(frac >= FloatType(1)-epsilon) intPart = (int)ip + 1;
    else if(frac <= FloatType(-1)+epsilon) intPart = (int)ip - 1;
    else return false;
    return true;
}

/******************************************************************************
* Generates a pretty string representation of the Burgers vector.
******************************************************************************/
QString DislocationVis::formatBurgersVector(const Vector3& b, const MicrostructurePhase* structure)
{
    if(structure) {
        if(structure->crystalSymmetryClass() == MicrostructurePhase::CrystalSymmetryClass::CubicSymmetry) {
            if(b.isZero())
                return QStringLiteral("[0 0 0]");
            FloatType smallestCompnt = FLOATTYPE_MAX;
            for(int i = 0; i < 3; i++) {
                FloatType c = std::abs(b[i]);
                if(c < smallestCompnt && c > FloatType(1e-3))
                    smallestCompnt = c;
            }
            if(smallestCompnt != FLOATTYPE_MAX) {
                FloatType m = FloatType(1) / smallestCompnt;
                for(int f = 1; f <= 11; f++) {
                    int multiplier;
                    if(!isInteger(m*f, multiplier)) continue;
                    if(multiplier < 80) {
                        Vector3 bm = b * (FloatType)multiplier;
                        Vector3I bmi;
                        if(isInteger(bm.x(),bmi.x()) && isInteger(bm.y(),bmi.y()) && isInteger(bm.z(),bmi.z())) {
                            if(multiplier != 1)
                                return QString("1/%1[%2 %3 %4]")
                                        .arg(multiplier)
                                        .arg(bmi.x()).arg(bmi.y()).arg(bmi.z());
                            else
                                return QString("[%1 %2 %3]")
                                        .arg(bmi.x()).arg(bmi.y()).arg(bmi.z());
                        }
                    }
                }
            }
        }
        else if(structure->crystalSymmetryClass() == MicrostructurePhase::CrystalSymmetryClass::HexagonalSymmetry) {
            if(b.isZero())
                return QStringLiteral("[0 0 0 0]");
            // Determine vector components U, V, and W, with b = U*a1 + V*a2 + W*c.
            FloatType U = sqrt(2.0)*b.x() - sqrt(2.0/3.0)*b.y();
            FloatType V = sqrt(2.0)*b.x() + sqrt(2.0/3.0)*b.y();
            FloatType W = sqrt(3.0/4.0)*b.z();
            Vector4 uvwt((2*U-V)/3, (2*V-U)/3, -(U+V)/3, W);
            FloatType smallestCompnt = FLOATTYPE_MAX;
            for(int i = 0; i < 4; i++) {
                FloatType c = std::abs(uvwt[i]);
                if(c < smallestCompnt && c > FloatType(1e-3))
                    smallestCompnt = c;
            }
            if(smallestCompnt != FLOATTYPE_MAX) {
                FloatType m = FloatType(1) / smallestCompnt;
                for(int f = 1; f <= 11; f++) {
                    int multiplier;
                    if(!isInteger(m*f, multiplier)) continue;
                    if(multiplier < 80) {
                        Vector4 bm = uvwt * (FloatType)multiplier;
                        int bmi[4];
                        if(isInteger(bm.x(),bmi[0]) && isInteger(bm.y(),bmi[1]) && isInteger(bm.z(),bmi[2]) && isInteger(bm.w(),bmi[3])) {
                            if(multiplier != 1)
                                return QString("1/%1[%2 %3 %4 %5]")
                                        .arg(multiplier)
                                        .arg(bmi[0]).arg(bmi[1]).arg(bmi[2]).arg(bmi[3]);
                            else
                                return QString("[%1 %2 %3 %4]")
                                        .arg(bmi[0]).arg(bmi[1]).arg(bmi[2]).arg(bmi[3]);
                        }
                    }
                }
            }
            return QString("[%1 %2 %3 %4]")
                    .arg(QLocale::c().toString(uvwt.x(), 'f'), 7)
                    .arg(QLocale::c().toString(uvwt.y(), 'f'), 7)
                    .arg(QLocale::c().toString(uvwt.z(), 'f'), 7)
                    .arg(QLocale::c().toString(uvwt.w(), 'f'), 7);
        }
    }

    if(b.isZero())
        return QStringLiteral("0 0 0");

    return QString("%1 %2 %3")
            .arg(QLocale::c().toString(b.x(), 'f'), 7)
            .arg(QLocale::c().toString(b.y(), 'f'), 7)
            .arg(QLocale::c().toString(b.z(), 'f'), 7);
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString DislocationPickInfo::infoString(Pipeline* pipeline, quint32 subobjectId)
{
    QString str;

    int segmentIndex = segmentIndexFromSubObjectID(subobjectId);
    if(dislocationObj()) {
        if(segmentIndex >= 0 && segmentIndex < dislocationObj()->segments().size()) {
            DislocationSegment* segment = dislocationObj()->segments()[segmentIndex];
            const MicrostructurePhase* structure = dislocationObj()->structureById(segment->burgersVector.cluster()->structure);
            QString formattedBurgersVector = DislocationVis::formatBurgersVector(segment->burgersVector.localVec(), structure);
            str = tr("<key>True Burgers vector:</key> <val>%1</val>").arg(formattedBurgersVector);
            Vector3 transformedVector = segment->burgersVector.toSpatialVector();
            str += tr("<sep><key>Spatial Burgers vector:</key> <val>[%1 %2 %3]</val>")
                    .arg(QLocale::c().toString(transformedVector.x(), 'f', 4), 7)
                    .arg(QLocale::c().toString(transformedVector.y(), 'f', 4), 7)
                    .arg(QLocale::c().toString(transformedVector.z(), 'f', 4), 7);
            str += tr("<sep><key>Cluster Id:</key> <val>%1</val>").arg(segment->burgersVector.cluster()->id);
            str += tr("<sep><key>Dislocation Id:</key> <val>%1</val>").arg(segment->id);
            if(structure) {
                str += tr("<sep><key>Crystal structure:</key> <val>%1</val>").arg(structure->name());
            }
        }
    }
    return str;
}

}   // End of namespace
