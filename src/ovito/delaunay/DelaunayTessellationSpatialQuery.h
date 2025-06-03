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

#include <ovito/stdobj/StdObj.h>
#include <ovito/delaunay/DelaunayTessellation.h>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/register/point.hpp>

// bPointCell cannot be defined inside of DelaunayTessellationSpatialQuery
// since BOOST_GEOMETRY_REGISTER_POINT_3D needs to be placed before bBox but also be in
// global namespace
namespace Ovito::DelaunayTessellationSpatialQueryImpl
{

struct bPointCell {
    Point3 point;
    size_t cell;
};

}  // End of namespace Ovito::DelaunayTessellationSpatialQueryImpl

// Adds the bPointCell type to boost geometry
// Needs to be in the global namespace
BOOST_GEOMETRY_REGISTER_POINT_3D(Ovito::DelaunayTessellationSpatialQueryImpl::bPointCell, Ovito::Point3::value_type,
                                 boost::geometry::cs::cartesian, point[0], point[1], point[2]);

namespace Ovito {

/// Create spatial querys on a Delaunay Tessellation finding all tetrahedrons
/// where their respective bounding boxes intersect with a target bounding bounding box
class OVITO_DELAUNAY_EXPORT DelaunayTessellationSpatialQuery
{
public:
    using bPoint = DelaunayTessellationSpatialQueryImpl::bPointCell;
    using bBox = boost::geometry::model::box<DelaunayTessellationSpatialQueryImpl::bPointCell>;

    /// Initializes the query class with a tessellation and an alpha value.
    /// Alpha can be used to pre-filter cells added to the tree to include only cells that are located within the bulk region.
    /// This function modifies the user field of the tessellation cells!
    DelaunayTessellationSpatialQuery(DelaunayTessellation& tessellation, std::optional<FloatType> alpha);

    /// Get all Delaunay cells intersecting with a given bounding box.
    /// Results are returned in the vector provided by the caller.
    /// The result is a list of rtree bounding boxes, which hold the actual cell references.
    void getOverlappingCells(const Box3& bbox, std::vector<bBox>& cells) const;

    size_t numCells() const { return _rtree.size(); }

private:

    boost::geometry::index::rtree<bBox, boost::geometry::index::quadratic<128>> _rtree;
};

}  // End of namespace Ovito
