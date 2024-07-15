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
 * Initialize the query class with a tessellation and a alpha value
 * Alpha can be used to pre-filter cells added to the tree
 * This function modifies the user field in the tessellation!
 ******************************************************************************/
DelaunayTessellationSpatialQuery::DelaunayTessellationSpatialQuery(DelaunayTessellation& tessellation, std::optional<FloatType> alpha)
    : _tessellation(tessellation)
{
    // Create rtree with the bounding boxes
    // rtree insertion is not thread safe!
    int idx = 0;
    for(size_t cell = 0; cell < _tessellation.numberOfTetrahedra(); ++cell) {
        // Only add defective and finite tetrahedrons
        if(!_tessellation.isFiniteCell(cell) || _tessellation.getUserField(cell) != -1) {
            // invalid index in user field
            tessellation.setUserField(cell, -1);
            continue;
        }

        // Skip based on alpha criterion
        if(alpha) {
            bool isFilledTetrehedron = false;
            if(auto alphaTestResult = tessellation.alphaTest(cell, alpha.value())) {
                isFilledTetrehedron = *alphaTestResult;
            }
            if(!isFilledTetrehedron) {
                // invalid index in user field
                tessellation.setUserField(cell, -1);
                continue;
            }
        }

        // Add bbounding box to tree
        Box3 bbox;
        for(size_t vert = 0; vert < 4; ++vert) {
            bbox.addPoint(_tessellation.vertexPosition(_tessellation.cellVertex(cell, vert)));
        }
        _rtree.insert(bBox({bbox.minc, cell}, {bbox.maxc, cell}));
        // Give cells included in the tree a contiguous  user field that can used to index into a vector
        tessellation.setUserField(cell, idx++);
        OVITO_ASSERT(_rtree.size() == idx);
    }
}

/******************************************************************************
 * Get all cells intersecting with a given bounding box
 * Target bounding box is defined by bboxLo and bboxHi
 * Boxes are returned in the cells vector
 ******************************************************************************/
void DelaunayTessellationSpatialQuery::getCells(const Box3& bbox, std::vector<bBox>& cells) const
{
    namespace bgi = boost::geometry::index;
    cells.clear();
    _rtree.query(bgi::intersects(bBox({bbox.minc, 0}, {bbox.maxc, 0})), std::back_inserter(cells));
}

}  // namespace Ovito