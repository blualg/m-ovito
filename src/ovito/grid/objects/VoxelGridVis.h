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


#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering VoxelGrid data objects.
 */
class OVITO_GRID_EXPORT VoxelGridVis : public DataVis
{
    OVITO_CLASS(VoxelGridVis)
    OVITO_CLASSINFO("DisplayName", "Voxel grid");

public:

    /// \brief Constructor.
    explicit VoxelGridVis(ObjectInitializationFlags flags);

    /// Lets the visualization element render the data object.
    virtual PipelineStatus render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const Pipeline* pipeline) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBox(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval) override;

    /// Returns the transparency parameter.
    FloatType transparency() const { return transparencyController() ? transparencyController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the transparency parameter.
    void setTransparency(FloatType t) { if(transparencyController()) transparencyController()->setFloatValue(AnimationTime(0), t); }

protected:

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// Controls the transparency of the grid's faces.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, transparencyController, setTransparencyController);

    /// Controls whether the grid lines should be highlighted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, highlightGridLines, setHighlightGridLines);

    /// Controls whether the voxel face colors should be interpolated.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, interpolateColors, setInterpolateColors);

    /// Transfer function for pseudo-color visualization of a grid property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PropertyColorMapping>, colorMapping, setColorMapping);
};

/**
 * \brief This data structure is attached to the geometry rendered by the VoxelGridVis
 * in the viewports. It facilitates the picking of grid cells with the mouse.
 */
class OVITO_GRID_EXPORT VoxelGridPickInfo : public ObjectPickInfo
{
    OVITO_CLASS(VoxelGridPickInfo)

public:

    /// Constructor.
    VoxelGridPickInfo(const VoxelGridVis* visElement, const VoxelGrid* voxelGrid, std::array<int, 3> numCells, size_t trianglesPerCell) :
        _visElement(visElement), _voxelGrid(voxelGrid), _numCells(numCells), _trianglesPerCell(trianglesPerCell) {}

    /// Returns the data object.
    const DataOORef<const VoxelGrid>& voxelGrid() const { return _voxelGrid; }

    /// Returns the vis element that rendered the voxel grid.
    const VoxelGridVis* visElement() const { return _visElement; }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(Pipeline* pipeline, quint32 subobjectId) override;

private:

    /// The data object holding the original grid data.
    DataOORef<const VoxelGrid> _voxelGrid;

    /// The vis element that rendered the voxel grid.
    OORef<VoxelGridVis> _visElement;

    /// Number of visible cells in each grid direction.
    std::array<int, 3> _numCells;

    /// The number of triangles rendered per voxel grid cell.
    size_t _trianglesPerCell;
};

}   // End of namespace
