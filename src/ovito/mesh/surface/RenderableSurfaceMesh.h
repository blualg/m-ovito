////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/util/CapPolygonTessellator.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>

namespace Ovito {

/**
 * \brief A non-periodic triangle mesh generated from a periodic SurfaceMesh.
 */
class OVITO_MESH_EXPORT RenderableSurfaceMesh
{
public:

    /// Constructor.
    RenderableSurfaceMesh(DataOORef<const TriangleMesh> surface, DataOORef<const TriangleMesh> capPolygons, std::vector<ColorA> materialColors, std::vector<size_t> originalFaceMap, bool backfaceCulling, PipelineStatus&& status) :
        _surface(std::move(surface)),
        _capPolygons(std::move(capPolygons)),
        _materialColors(std::move(materialColors)),
        _originalFaceMap(std::move(originalFaceMap)),
        _backfaceCulling(backfaceCulling),
        _status(std::move(status)) {}

    /// Returns the surface part of the mesh.
    const DataOORef<const TriangleMesh>& surface() const { return _surface; }

    /// Returns the cap polygon part of the mesh (optional).
    const DataOORef<const TriangleMesh>& capPolygons() const { return _capPolygons; }

    /// Returns the material colors assigned to the surface mesh (optional).
    const std::vector<ColorA>& materialColors() const { return _materialColors; }

    /// Returns the mapping of triangles of the renderable surface mesh to the original mesh (optional).
    const std::vector<size_t>& originalFaceMap() const { return _originalFaceMap; }

    /// Returns whether triangles of the surface mesh should be rendered with active backface culling.
    bool backfaceCulling() const { return _backfaceCulling; }

    /// Returns any warnings or errors that occurred during the construction of the renderable surface mesh.
    const PipelineStatus& status() const { return _status; }

private:

    /// The surface part of the mesh.
    DataOORef<const TriangleMesh> _surface;

    /// The cap polygon part of the mesh.
    DataOORef<const TriangleMesh> _capPolygons;

    /// The material colors assigned to the surface mesh (optional).
    std::vector<ColorA> _materialColors;

    /// The mapping of triangles of the renderable surface mesh to the original mesh (optional).
    std::vector<size_t> _originalFaceMap;

    /// Indicates whether triangles of the surface mesh should be rendered with active backface culling.
    bool _backfaceCulling = false;

    /// Holds any warnings or errors that occurred during the construction of the renderable surface mesh.
    PipelineStatus _status;
};

}   // End of namespace
