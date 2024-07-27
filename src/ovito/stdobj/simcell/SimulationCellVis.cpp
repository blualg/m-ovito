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
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include "SimulationCellVis.h"
#include "SimulationCell.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCellVis);
OVITO_CLASSINFO(SimulationCellVis, "DisplayName", "Simulation cell");
DEFINE_PROPERTY_FIELD(SimulationCellVis, cellLineWidth);
DEFINE_SHADOW_PROPERTY_FIELD(SimulationCellVis, cellLineWidth);
DEFINE_PROPERTY_FIELD(SimulationCellVis, renderCellEnabled);
DEFINE_PROPERTY_FIELD(SimulationCellVis, cellColor);
SET_PROPERTY_FIELD_LABEL(SimulationCellVis, cellLineWidth, "Line width");
SET_PROPERTY_FIELD_LABEL(SimulationCellVis, renderCellEnabled, "Visible in rendered images");
SET_PROPERTY_FIELD_LABEL(SimulationCellVis, cellColor, "Line color");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SimulationCellVis, cellLineWidth, WorldParameterUnit, 0);

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 SimulationCellVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    const SimulationCell* cellObject = path.lastAs<SimulationCell>();
    if(!cellObject)
        return {};

    AffineTransformation matrix = cellObject->cellMatrix();
    if(cellObject->is2D()) {
        matrix.column(2).setZero();
        matrix.translation().z() = 0;
    }

    return Box3(Point3(0), Point3(1)).transformed(matrix);
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus SimulationCellVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    if(const SimulationCell* cell = path.lastAs<SimulationCell>()) {
        if(frameGraph.isInteractive() && !frameGraph.isPreviewMode()) {
            renderWireframe(cell, flowState, frameGraph, pipeline);
        }
        else {
            if(!renderCellEnabled())
                return {};      // Do nothing if rendering has been disabled by the user.

            renderSolid(cell, flowState, frameGraph, pipeline);
        }
    }
    return {};
}

/******************************************************************************
* Renders the given simulation cell using lines.
******************************************************************************/
void SimulationCellVis::renderWireframe(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Look up the vertex data in the vis cache.
    const ConstDataBufferPtr& lineVertices = frameGraph.visCache().lookup<ConstDataBufferPtr>(
        RendererResourceKey<struct WireframeVertices, bool>{cell->is2D()},
        [&](ConstDataBufferPtr& lineVertices) {
            // Depending on whether this cell is 3D or 2D, create a wireframe unit cube or unit square.
            BufferFactory<Point3G> corners(cell->is2D() ? 8 : 24);
            corners[0] = Point3G(0,0,0);
            corners[1] = Point3G(1,0,0);
            corners[2] = Point3G(1,0,0);
            corners[3] = Point3G(1,1,0);
            corners[4] = Point3G(1,1,0);
            corners[5] = Point3G(0,1,0);
            corners[6] = Point3G(0,1,0);
            corners[7] = Point3G(0,0,0);
            if(!cell->is2D()) {
                corners[8]  = Point3G(0,0,1);
                corners[9]  = Point3G(1,0,1);
                corners[10] = Point3G(1,0,1);
                corners[11] = Point3G(1,1,1);
                corners[12] = Point3G(1,1,1);
                corners[13] = Point3G(0,1,1);
                corners[14] = Point3G(0,1,1);
                corners[15] = Point3G(0,0,1);
                corners[16] = Point3G(0,0,0);
                corners[17] = Point3G(0,0,1);
                corners[18] = Point3G(1,0,0);
                corners[19] = Point3G(1,0,1);
                corners[20] = Point3G(1,1,0);
                corners[21] = Point3G(1,1,1);
                corners[22] = Point3G(0,1,0);
                corners[23] = Point3G(0,1,1);
            }
            lineVertices = corners.take();
        });

    // Prepare line drawing primitive.
    std::unique_ptr<LinePrimitive> linePrimitive = std::make_unique<LinePrimitive>();
    linePrimitive->setPositions(lineVertices);
    linePrimitive->setUniformColor(ViewportSettings::getSettings().viewportColor(pipeline->isSelected() ? ViewportSettings::COLOR_SELECTION : ViewportSettings::COLOR_UNSELECTED));

    // Compute transformation matrix from unit cell space to world space.
    TimeInterval iv;
    const AffineTransformation nodeTM = pipeline->getWorldTransform(frameGraph.time(), iv);
    AffineTransformation cellMatrix = cell->cellMatrix();
    if(cell->is2D())
        cellMatrix(2,3) = 0; // For 2D cells, implicitly set z-coordinate of origin to zero.
    frameGraph.addPrimitive(std::move(linePrimitive), nodeTM * cellMatrix, frameGraph.addPickingGroup(pipeline), Box3(Point3(0), Point3(1)));
}

/******************************************************************************
* Renders the given simulation cell using solid shading mode.
******************************************************************************/
void SimulationCellVis::renderSolid(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Lookup the rendering primitive in the vis cache.
    const auto& [edges, corners] = frameGraph.visCache().lookup<std::tuple<CylinderPrimitive, ParticlePrimitive>>(
        RendererResourceKey<struct SolidCellCache, ConstDataObjectRef, FloatType, Color>{ cell, cellLineWidth(), cellColor() },
        [&](CylinderPrimitive& edges, ParticlePrimitive& corners) {
            edges.setShape(CylinderPrimitive::CylinderShape);
            edges.setShadingMode(CylinderPrimitive::NormalShading);
            edges.setUniformColor(cellColor());
            edges.setUniformWidth(2 * cellLineWidth());

            // Create a data buffer for the box corner coordinates.
            BufferFactory<Point3G> cornersAccesor(cell->is2D() ? 4 : 8);

            // Create a data buffer for the cylinder base points.
            BufferFactory<Point3G> basePoints(cell->is2D() ? 4 : 12);

            // Create a data buffer for the cylinder head points.
            BufferFactory<Point3G> headPoints(cell->is2D() ? 4 : 12);

            cornersAccesor[0] = cell->cellOrigin().toDataType<GraphicsFloatType>();
            if(cell->is2D())
                cornersAccesor[0].z() = 0; // For 2D cells, implicitly set z-coordinate of origin to zero.
            cornersAccesor[1] = cornersAccesor[0] + cell->cellVector1().toDataType<GraphicsFloatType>();
            cornersAccesor[2] = cornersAccesor[1] + cell->cellVector2().toDataType<GraphicsFloatType>();
            cornersAccesor[3] = cornersAccesor[0] + cell->cellVector2().toDataType<GraphicsFloatType>();
            basePoints[0] = cornersAccesor[0];
            basePoints[1] = cornersAccesor[1];
            basePoints[2] = cornersAccesor[2];
            basePoints[3] = cornersAccesor[3];
            headPoints[0] = cornersAccesor[1];
            headPoints[1] = cornersAccesor[2];
            headPoints[2] = cornersAccesor[3];
            headPoints[3] = cornersAccesor[0];
            if(cell->is2D() == false) {
                cornersAccesor[4] = cornersAccesor[0] + cell->cellVector3().toDataType<GraphicsFloatType>();
                cornersAccesor[5] = cornersAccesor[1] + cell->cellVector3().toDataType<GraphicsFloatType>();
                cornersAccesor[6] = cornersAccesor[2] + cell->cellVector3().toDataType<GraphicsFloatType>();
                cornersAccesor[7] = cornersAccesor[3] + cell->cellVector3().toDataType<GraphicsFloatType>();
                basePoints[4] = cornersAccesor[4];
                basePoints[5] = cornersAccesor[5];
                basePoints[6] = cornersAccesor[6];
                basePoints[7] = cornersAccesor[7];
                basePoints[8] = cornersAccesor[0];
                basePoints[9] = cornersAccesor[1];
                basePoints[10] = cornersAccesor[2];
                basePoints[11] = cornersAccesor[3];
                headPoints[4] = cornersAccesor[5];
                headPoints[5] = cornersAccesor[6];
                headPoints[6] = cornersAccesor[7];
                headPoints[7] = cornersAccesor[4];
                headPoints[8] = cornersAccesor[4];
                headPoints[9] = cornersAccesor[5];
                headPoints[10] = cornersAccesor[6];
                headPoints[11] = cornersAccesor[7];
            }
            edges.setPositions(basePoints.take(), headPoints.take());

            // Render spheres in the corners of the simulation box.
            corners.setParticleShape(ParticlePrimitive::SphericalShape);
            corners.setShadingMode(ParticlePrimitive::NormalShading);
            corners.setRenderingQuality(ParticlePrimitive::HighQuality);
            corners.setPositions(cornersAccesor.take());
            corners.setUniformRadius(cellLineWidth());
            corners.setUniformColor(cellColor());
        });

    auto pickingGroup = frameGraph.addPickingGroup(pipeline);
    frameGraph.addPrimitive(std::make_unique<CylinderPrimitive>(edges), pipeline, pickingGroup);
    frameGraph.addPrimitive(std::make_unique<ParticlePrimitive>(corners), pipeline, pickingGroup);
}

}   // End of namespace
