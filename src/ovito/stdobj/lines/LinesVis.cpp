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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/stdobj/lines/Lines.h>
#include "LinesVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesVis);
OVITO_CLASSINFO(LinesVis, "DisplayName", "Lines");
OVITO_CLASSINFO(LinesVis, "ClassNameAlias", "TrajectoryVis");  // For backward compatibility with OVITO 3.9.2
IMPLEMENT_ABSTRACT_OVITO_CLASS(LinesPickInfo);
DEFINE_PROPERTY_FIELD(LinesVis, lineWidth);
DEFINE_PROPERTY_FIELD(LinesVis, lineColor);
DEFINE_PROPERTY_FIELD(LinesVis, roundedCaps);
DEFINE_PROPERTY_FIELD(LinesVis, shadingMode);
DEFINE_PROPERTY_FIELD(LinesVis, showUpToCurrentTime);
DEFINE_PROPERTY_FIELD(LinesVis, wrappedLines);
DEFINE_PROPERTY_FIELD(LinesVis, coloringMode);
DEFINE_REFERENCE_FIELD(LinesVis, colorMapping);
SET_PROPERTY_FIELD_LABEL(LinesVis, lineWidth, "Line width");
SET_PROPERTY_FIELD_LABEL(LinesVis, lineColor, "Line color");
SET_PROPERTY_FIELD_LABEL(LinesVis, roundedCaps, "Rounded line ends");
SET_PROPERTY_FIELD_LABEL(LinesVis, shadingMode, "Shading mode");
SET_PROPERTY_FIELD_LABEL(LinesVis, showUpToCurrentTime, "Show up to current time only");
SET_PROPERTY_FIELD_LABEL(LinesVis, wrappedLines, "Wrap lines around");
SET_PROPERTY_FIELD_LABEL(LinesVis, coloringMode, "Coloring mode");
SET_PROPERTY_FIELD_LABEL(LinesVis, colorMapping, "Color mapping");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(LinesVis, lineWidth, WorldParameterUnit, 0);

/******************************************************************************
 * Returns a human-readable string describing the picked object,
 * which will be displayed in the status bar by OVITO.
 ******************************************************************************/
QString LinesPickInfo::infoString(Pipeline* pipeline, quint32 subobjectId)
{
    QString str;

    if(!linesObj())
        return str;

    for(size_t i = 0; i < 2; ++i) {
        int index = segmentIndexFromSubObjectID(subobjectId) + i;
        if(index == -1) {
            return str;
        }

        if(!str.isEmpty()) str += " ";
        str += (i == 0) ? tr("<section>Head:</section> ") : tr("<sep><section>Tail:</section> ");
        str += QStringLiteral("<key>Index:</key> ");
        str += QString::number(index);
        str += QStringLiteral("</val>");

        for(const Property* property : linesObj()->properties()) {
            if(property->typeId() == Property::GenericColorProperty) continue;

            if(!str.isEmpty()) str += QStringLiteral("<sep>");

            str += QStringLiteral("<key>");
            str += property->name().toHtmlEscaped();
            str += QStringLiteral(":</key> <val>");

            if(property->dataType() == Property::Int32) {
                BufferReadAccess<int*> data(property);
                for(size_t component = 0; component < data.componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(index, component));
                }
            }
            else if(property->dataType() == Property::Int64) {
                BufferReadAccess<int64_t*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(index, component));
                }
            }
            else if(property->dataType() == Property::Float32) {
                BufferReadAccess<float*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(index, component));
                }
            }
            else if(property->dataType() == Property::Float64) {
                BufferReadAccess<double*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(index, component));
                }
            }
            else {
                str += QStringLiteral("<%1>").arg(property->dataTypeName());
            }
            str += QStringLiteral("</val>");
        }
    }
    return str;
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void LinesVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create a color mapping object for pseudo-color visualization of a local line property.
        setColorMapping(OORef<PropertyColorMapping>::create(flags));
    }
}

/******************************************************************************
 * This method is called once for this object after it has been completely
 * loaded from a stream.
 ******************************************************************************/
void LinesVis::loadFromStreamComplete(ObjectLoadStream& stream)
{
    DataVis::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.5.4.
    // Create a color mapping sub-object if it wasn't loaded from the state file.
    if(!colorMapping()) setColorMapping(OORef<PropertyColorMapping>::create());

    // For backward compatibility with OVITO 3.9.x.
    if(stream.applicationMajorVersion() == 3 && stream.applicationMinorVersion() < 10) {
        setRoundedCaps(false); // Override user default setting when loading a legacy state file.
    }
}

/******************************************************************************
 * Computes the bounding box of the object.
 ******************************************************************************/
Box3 LinesVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const Lines* lines = path.lastAs<Lines>();

    // Get the simulation cell.
    const SimulationCell* simulationCell = wrappedLines() ? flowState.getObject<SimulationCell>() : nullptr;

    // Compute bounding box from trajectory data.
    Box3 bbox;
    if(lines) {
        if(!simulationCell) {
            if(BufferReadAccess<Point3> posProperty = lines->getProperty(Lines::PositionProperty)) {
                bbox.addPoints(posProperty);
            }
        }
        else {
            bbox = Box3(Point3(0, 0, 0), Point3(1, 1, 1)).transformed(simulationCell->cellMatrix());
        }
        bbox = bbox.padBox(lineWidth() / 2);
    }
    return bbox;
}

/******************************************************************************
 * Lets the visualization element render the data object.
 ******************************************************************************/
PipelineStatus LinesVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    PipelineStatus status;

    const Lines* lines = path.lastAs<Lines>();
    if(!lines)
        return status;

    // Get the simulation cell.
    const SimulationCell* simulationCell = wrappedLines() ? flowState.getObject<SimulationCell>() : nullptr;

    // Look for selected pseudo-coloring property.
    const Property* pseudoColorProperty = nullptr;
    int pseudoColorPropertyComponent = 0;
    if(coloringMode() == PseudoColoring && colorMapping() && colorMapping()->sourceProperty() && !lines->getProperty(Lines::ColorProperty)) {
        QString errorDescr;
        std::tie(pseudoColorProperty, pseudoColorPropertyComponent) = colorMapping()->sourceProperty().findInContainerWithComponent(lines, errorDescr);
        if(!pseudoColorProperty)
            status = PipelineStatus(PipelineStatus::Error, std::move(errorDescr));
    }

    // The key type used for caching the rendering primitive:
    using CacheKey = RendererResourceKey<struct LinesVisCache,
                                         ConstDataObjectRef,  // Lines data object
                                         FloatType,           // Line width
                                         bool,                // Rounded line end caps
                                         Color,               // Line color,
                                         ShadingMode,         // Shading mode
                                         int,                 // End frame
                                         ConstDataObjectRef,  // Simulation cell
                                         ConstDataObjectRef,  // Pseudo-color property
                                         int                  // Pseudo-color vector component
                                         >;

    // The data structure stored in the vis cache.
    struct CacheValue {
        CylinderPrimitive segments;
        ParticlePrimitive corners;
        ConstDataBufferPtr cornerPseudoColors;
        OORef<LinesPickInfo> pickInfo;
    };

    int endFrame = showUpToCurrentTime() ? frameGraph.time().frame() : std::numeric_limits<int>::max();

    // Look up the rendering primitives in the vis cache.
    auto& visCache = frameGraph.visCache().lookup<CacheValue>(CacheKey(lines, lineWidth(), roundedCaps(), lineColor(), shadingMode(), endFrame,
                                                                   simulationCell, pseudoColorProperty, pseudoColorPropertyComponent));

    // The shading mode for corner spheres.
    ParticlePrimitive::ShadingMode cornerShadingMode =
        (shadingMode() == ShadingMode::NormalShading) ? ParticlePrimitive::NormalShading : ParticlePrimitive::FlatShading;

    // Check if we already have valid rendering primitives that are up to date.
    if(!visCache.segments.basePositions() || !visCache.corners.positions()) {
        // Update the rendering primitives.
        visCache.segments.setPositions(nullptr, nullptr);
        visCache.corners.setPositions(nullptr);
        visCache.cornerPseudoColors.reset();

        // Map from viewport object ids to line segments
        std::vector<int> subobjToSegmentMap;

        FloatType lineDiameter = lineWidth();
        if(lines && lineDiameter > 0) {
            lines->verifyIntegrity();

            // Retrieve the line position data stored in the Lines.
            BufferReadAccess<Point3> posProperty = lines->getProperty(Lines::PositionProperty);
            BufferReadAccess<int64_t> secProperty = lines->getProperty(Lines::SectionProperty);
            BufferReadAccess<int32_t> timeProperty = lines->getProperty(Lines::SampleTimeProperty);

            BufferReadAccess<ColorG> colorProperty = lines->getProperty(Lines::ColorProperty);
            RawBufferReadAccess pseudoColorArray(pseudoColorProperty);
            if(posProperty.valid() && posProperty.size() >= 2) {
                subobjToSegmentMap.reserve(posProperty.size());
                // Determine the number of line segments and corner points to render.
                BufferFactory<Point3G> cornerPoints(0);
                BufferFactory<Point3G> baseSegmentPoints(0);
                BufferFactory<Point3G> headSegmentPoints(0);
                BufferFactory<ColorG> cornerColors = colorProperty ? BufferFactory<ColorG>(0) : BufferFactory<ColorG>{};
                BufferFactory<ColorG> segmentColors = colorProperty ? BufferFactory<ColorG>(0) : BufferFactory<ColorG>{};
                BufferFactory<GraphicsFloatType> cornerPseudoColors =
                    pseudoColorArray ? BufferFactory<GraphicsFloatType>(0) : BufferFactory<GraphicsFloatType>{};
                BufferFactory<GraphicsFloatType> segmentPseudoColors =
                    pseudoColorArray ? BufferFactory<GraphicsFloatType>(0) : BufferFactory<GraphicsFloatType>{};
                const Point3* pos = posProperty.cbegin();
                // Lines does not have sample time. It's only valid for TrajectoryLines
                const int32_t* sampleTime = (timeProperty) ? timeProperty.cbegin() : nullptr;
                const int64_t* id = (secProperty) ? secProperty.cbegin() : nullptr;
                size_t inputColorIndex = 0;

                // subObject index map to allow picking in the viewport
                int subobjIndex = 0;

                // segment callback used by the "clipLines" function
                // TODO: this can be a templated lambda with (colorOffset) in c++20
                const auto clipPointCallback = [&](const Point3& p1, int colorOffset) {
                    OVITO_ASSERT(colorOffset < 2);
                    cornerPoints.push_back(p1.toDataType<GraphicsFloatType>());
                    if(colorProperty) {
                        cornerColors.push_back(colorProperty[inputColorIndex + colorOffset]);
                    }
                    else if(pseudoColorArray) {
                        cornerPseudoColors.push_back(pseudoColorArray.get<GraphicsFloatType>(inputColorIndex + colorOffset, pseudoColorPropertyComponent));
                    }
                };

                // Don't increment sampleTime if timeProperty is not present
                for(auto pos_end = pos + posProperty.size() - 1; pos != pos_end;
                    ++pos, (sampleTime) ? ++sampleTime : nullptr, (id) ? ++id : nullptr) {
                    // Use short circuit to avoid dereferencing nullptr
                    if((!id || id[0] == id[1]) && (!sampleTime || sampleTime[1] <= endFrame)) {
                        clipLine(pos[0], pos[1], simulationCell, lines->cuttingPlanes(),
                                    [&](const Point3& p1, const Point3& p2, GraphicsFloatType t1, GraphicsFloatType t2) {
                                        baseSegmentPoints.push_back(p1.toDataType<GraphicsFloatType>());
                                        headSegmentPoints.push_back(p2.toDataType<GraphicsFloatType>());
                                        if(colorProperty) {
                                            segmentColors.push_back((GraphicsFloatType(1) - t1) * colorProperty[inputColorIndex] + t1 * colorProperty[inputColorIndex + 1]);
                                            segmentColors.push_back((GraphicsFloatType(1) - t2) * colorProperty[inputColorIndex] + t2 * colorProperty[inputColorIndex + 1]);
                                        }
                                        else if(pseudoColorArray) {
                                            GraphicsFloatType ps1 = pseudoColorArray.get<GraphicsFloatType>(inputColorIndex + 0, pseudoColorPropertyComponent);
                                            GraphicsFloatType ps2 = pseudoColorArray.get<GraphicsFloatType>(inputColorIndex + 1, pseudoColorPropertyComponent);
                                            segmentPseudoColors.push_back((GraphicsFloatType(1) - t1) * ps1 + t1 * ps2);
                                            segmentPseudoColors.push_back((GraphicsFloatType(1) - t2) * ps1 + t2 * ps2);
                                        }
                                    });
                        if(!roundedCaps() && (pos + 1 != pos_end) && (!id || id[1] == (id + 1)[1]) &&
                           (!sampleTime || sampleTime[1] != endFrame)) {
                            clipPoint(pos[1], simulationCell, lines->cuttingPlanes(),
                                      std::bind(clipPointCallback, std::placeholders::_1, 1));
                        }
                        else if(roundedCaps()) {
                            clipPoint(pos[0], simulationCell, lines->cuttingPlanes(),
                                      std::bind(clipPointCallback, std::placeholders::_1, 0));
                            if((pos + 1 == pos_end) || (!id || id[1] != (id + 1)[1])) {
                                clipPoint(pos[1], simulationCell, lines->cuttingPlanes(),
                                          std::bind(clipPointCallback, std::placeholders::_1, 1));
                            }
                        }
                        subobjToSegmentMap.push_back(subobjIndex);
                    }
                    subobjIndex++;
                    inputColorIndex++;
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
        visCache.pickInfo = OORef<LinesPickInfo>::create(lines, std::move(subobjToSegmentMap));
    }

    if(!visCache.segments.basePositions())
        return status;

    // Update the color mapping.
    visCache.segments.setPseudoColorMapping(colorMapping()->pseudoColorMapping());

    // Convert the pseudocolors of the corner spheres to RGB colors if necessary.
    if(visCache.cornerPseudoColors) {
        // Perform a cache lookup to check if latest pseudocolors have already been mapped to RGB colors.
        auto& cornerColorsUpToDate =
            frameGraph.visCache().lookup<bool>(std::make_pair(visCache.cornerPseudoColors, visCache.segments.pseudoColorMapping()));
        if(!cornerColorsUpToDate) {
            // Create an RGB color array, which will be filled and then assigned to the ParticlesPrimitive.
            BufferFactory<ColorG> cornerColorsArray(visCache.cornerPseudoColors->size());
            boost::transform(BufferReadAccess<GraphicsFloatType>(visCache.cornerPseudoColors), cornerColorsArray.begin(),
                             [&](GraphicsFloatType v) { return visCache.segments.pseudoColorMapping().valueToColor(v); });
            visCache.corners.setColors(cornerColorsArray.take());
            cornerColorsUpToDate = true;
        }
    }

    auto pickingGroup = frameGraph.addPickingGroup(pipeline, visCache.pickInfo);
    frameGraph.addPrimitive(std::make_unique<CylinderPrimitive>(visCache.segments), pipeline, pickingGroup);
    frameGraph.addPrimitive(std::make_unique<ParticlePrimitive>(visCache.corners), pipeline, pickingGroup);

    return status;
}

/******************************************************************************
 * Clips a linear line segment at the periodic box boundaries or cutting planes.
 ******************************************************************************/
void LinesVis::clipLine(const Point3& v1, const Point3& v2, const SimulationCell* simulationCell, const QVector<Plane3>& clippingPlanes,
                        const std::function<void(const Point3&, const Point3&, GraphicsFloatType, GraphicsFloatType)>& segmentCallback)
{
    auto clippingFunction = [&clippingPlanes, &segmentCallback](Point3 p1, Point3 p2, GraphicsFloatType t1, GraphicsFloatType t2) {
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
                t1 += (t2 - t1) * (c1 / (c1 - c2));
            }
            else if(c1 < -FLOATTYPE_EPSILON && c2 > FLOATTYPE_EPSILON) {
                p2 += (p1 - p2) * (c2 / (c2 - c1));
                t2 += (t1 - t2) * (c2 / (c2 - c1));
            }
        }
        if(!isClipped) {
            segmentCallback(p1, p2, t1, t2);
        }
    };

    if(simulationCell) {
        Point3 rp1 = simulationCell->absoluteToReduced(v1);
        Vector3 shiftVector = Vector3::Zero();
        for(size_t dim = 0; dim < 3; dim++) {
            if(simulationCell->hasPbcCorrected(dim)) {
                while(rp1[dim] >= 1) {
                    rp1[dim] -= 1;
                    shiftVector[dim] -= 1;
                }
                while(rp1[dim] < 0) {
                    rp1[dim] += 1;
                    shiftVector[dim] += 1;
                }
            }
        }
        Point3 rp2 = simulationCell->absoluteToReduced(v2) + shiftVector;
        FloatType t1 = 0;
        FloatType smallestT;
        bool clippedDimensions[3] = {false, false, false};
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
                    clippingFunction(rp1abs, intabs, t1, t2);
                }
                shiftVector[crossDim] -= crossDir;
                rp1 = intersection;
                rp1[crossDim] -= crossDir;
                rp2[crossDim] -= crossDir;
                t1 = t2;
            }
        } while(smallestT != FLOATTYPE_MAX);

        clippingFunction(simulationCell->reducedToAbsolute(rp1), simulationCell->reducedToAbsolute(rp2), t1, 1);
    }
    else {
        clippingFunction(v1, v2, 0, 1);
    }
}

/******************************************************************************
 * Clips a point at the periodic box boundaries or cutting planes.
 ******************************************************************************/
void LinesVis::clipPoint(const Point3& v1, const SimulationCell* simulationCell, const QVector<Plane3>& clippingPlanes,
                         const std::function<void(const Point3&)>& segmentCallback)
{
    auto clippingFunction = [&clippingPlanes, &segmentCallback](const Point3& p1) {
        bool isClipped = false;
        for(const Plane3& plane : clippingPlanes) {
            if(plane.classifyPoint(p1) > 0) {
                isClipped = true;
                break;
            }
        }
        if(!isClipped) {
            segmentCallback(p1);
        }
    };

    if(simulationCell) {
        clippingFunction(simulationCell->wrapPoint(v1));
    }
    else {
        clippingFunction(v1);
    }
}

}  // namespace Ovito
