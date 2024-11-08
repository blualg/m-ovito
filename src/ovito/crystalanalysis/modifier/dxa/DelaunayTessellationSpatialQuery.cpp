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

#include "DelaunayTessellationSpatialQuery.h"

namespace Ovito {

/******************************************************************************
 * Initializes the query class with a tessellation and an alpha value.
 * Alpha can be used to pre-filter cells added to the tree to include only cells that are located within the bulk region.
 * This function modifies the user field of the tessellation cells!
 ******************************************************************************/
DelaunayTessellationSpatialQuery::DelaunayTessellationSpatialQuery(DelaunayTessellation& tessellation, std::optional<FloatType> alpha)
{
    // Create rtree with the bounding boxes
    // rtree insertion is not thread safe!
    int idx = 0;
    for(DelaunayTessellation::CellHandle cell : tessellation.cells()) {
        // Only add defective and finite tetrahedrons
        if(!tessellation.isFiniteCell(cell) || tessellation.getUserField(cell) != -1) {
            tessellation.setUserField(cell, -1);
            continue;
        }

        // Skip cells that are outside the solid region based on alpha criterion
        if(alpha.has_value()) {
            bool isFilledTetrahedron = tessellation.alphaTest(cell, alpha.value()).value_or(false);
            if(!isFilledTetrahedron) {
                tessellation.setUserField(cell, -1);
                continue;
            }
        }

        // Add bounding box to tree
        Box3 bbox;
        for(size_t vert = 0; vert < 4; ++vert) {
            bbox.addPoint(tessellation.vertexPosition(tessellation.cellVertex(cell, vert)));
        }
        _rtree.insert(bBox({bbox.minc, cell}, {bbox.maxc, cell}));
        // Give cells included in the tree a contiguous range of indices that can used to index into a separate data vector (DislocationTracer::_cellDataForCoreAtomIdentification)
        tessellation.setUserField(cell, idx++);
        OVITO_ASSERT(_rtree.size() == idx);
    }
}

/******************************************************************************
 * Get all Delaunay cells intersecting with a given bounding box.
 * Results are returned in the vector provided by the caller.
 * The result is a list of rtree bounding boxes, which hold the actual cell references.
 ******************************************************************************/
void DelaunayTessellationSpatialQuery::getOverlappingCells(const Box3& bbox, std::vector<bBox>& cells) const
{
    namespace bgi = boost::geometry::index;
    cells.clear(); // Recycling the existing vector avoids memory allocation overhead
    _rtree.query(bgi::intersects(bBox({bbox.minc, 0}, {bbox.maxc, 0})), std::back_inserter(cells));
}

}  // namespace Ovito