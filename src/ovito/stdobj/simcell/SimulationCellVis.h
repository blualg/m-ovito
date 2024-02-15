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


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

/**
 * \brief A visual element that renders a SimulationCell as a wireframe box.
 */
class OVITO_STDOBJ_EXPORT SimulationCellVis : public DataVis
{
    OVITO_CLASS(SimulationCellVis)
    OVITO_CLASSINFO("DisplayName", "Simulation cell");

public:

    /// Constructor.
    explicit SimulationCellVis(ObjectInitializationFlags flags);

    /// Lets the visualization element render the data object.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Indicates whether this object should be surrounded by a selection marker in the viewports when it is selected.
    virtual bool showSelectionMarker() override { return false; }

protected:

    /// Renders the given simulation using wireframe mode.
    void renderWireframe(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline);

    /// Renders the given simulation using solid shading mode.
    void renderSolid(const SimulationCell* cell, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline);

protected:

    /// Controls the line width used to render the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, cellLineWidth, setCellLineWidth);
    DECLARE_SHADOW_PROPERTY_FIELD(cellLineWidth);

    /// Controls whether the simulation cell is visible.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, renderCellEnabled, setRenderCellEnabled);

    /// Controls the rendering color of the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, cellColor, setCellColor, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
