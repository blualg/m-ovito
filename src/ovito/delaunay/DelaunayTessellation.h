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
#include <ovito/stdobj/simcell/SimulationCell.h>

#include <geogram/Delaunay_psm.h>
#include <boost/iterator/counting_iterator.hpp>

namespace Ovito {

/**
 * Generates a Delaunay tessellation of a particle system.
 */
class OVITO_DELAUNAY_EXPORT DelaunayTessellation
{
public:

    typedef GEO::index_t size_type;
    typedef GEO::index_t CellHandle;
    typedef GEO::index_t VertexHandle;
    typedef boost::counting_iterator<size_type> CellIterator;

    /// Data structure attached to each tessellation cell.
    struct CellInfo {
        bool isGhost;   // Indicates whether this is a ghost tetrahedron.
        int userField;  // An additional field that can be used by client code.
        qint64 index;   // An index assigned to the cell.
    };

    typedef std::pair<CellHandle, int> Facet;

    class FacetCirculator {
    public:
        FacetCirculator(const DelaunayTessellation& tess, CellHandle cell, int i, int j) :
            _tess(tess), _pos(cell), _s(tess.cellVertex(cell, i)), _t(tess.cellVertex(cell, j)) {}

        FacetCirculator(const DelaunayTessellation& tess, CellHandle cell, int s, int t, CellHandle start, int f) :
            _tess(tess), _s(tess.cellVertex(cell, s)), _t(tess.cellVertex(cell, t)) {
            int i = tess.findVertexInCell(start, _s);
            int j = tess.findVertexInCell(start, _t);
            OVITO_ASSERT(f != i && f != j);
            if(f == next_around_edge(i,j))
                _pos = start;
            else
                _pos = tess.cellAdjacent(start, f); // other cell with same facet
        }
        FacetCirculator& operator--() {
            _pos = _tess.cellAdjacent(_pos, next_around_edge(_tess.findVertexInCell(_pos, _t), _tess.findVertexInCell(_pos, _s)));
            return *this;
        }
        FacetCirculator operator--(int) {
            FacetCirculator tmp(*this);
            --(*this);
            return tmp;
        }
        FacetCirculator& operator++() {
            _pos = _tess.cellAdjacent(_pos, next_around_edge(_tess.findVertexInCell(_pos, _s), _tess.findVertexInCell(_pos, _t)));
            return *this;
        }
        FacetCirculator operator++(int) {
            FacetCirculator tmp(*this);
            ++(*this);
            return tmp;
        }
        Facet operator*() const {
            return Facet(_pos, next_around_edge(_tess.findVertexInCell(_pos, _s), _tess.findVertexInCell(_pos, _t)));
        }
        Facet operator->() const {
            return **this;
        }
        bool operator==(const FacetCirculator& ccir) const {
            return _pos == ccir._pos && _s == ccir._s && _t == ccir._t;
        }
        bool operator!=(const FacetCirculator& ccir) const {
            return !(*this == ccir);
        }
        CellHandle cell() const {
            return _pos;
        }

    private:
        const DelaunayTessellation& _tess;
        VertexHandle _s;
        VertexHandle _t;
        CellHandle _pos;

        static constexpr int next_around_edge(int i, int j) {
            constexpr int tab_next_around_edge[4][4] = {
                  {5, 2, 3, 1},
                  {3, 5, 0, 2},
                  {1, 3, 5, 0},
                  {2, 0, 1, 5}};
            return tab_next_around_edge[i][j];
        }
    };

    /// Generates the Delaunay tessellation.
    void generateTessellation(const SimulationCell* simCell, const Point3* points, size_t numPoints, FloatType ghostLayerSize, bool coverDomainWithFiniteTets, const SelectionIntType* pointMask, TaskProgress& progress);

    /// Returns the total number of tetrahedra in the tessellation, including ghost tetrahedra and infinite tetrahedra.
    size_type numberOfTetrahedra() const { return _dt->nb_cells(); }

    /// Returns the number of finite cells, which belong to the primary image of the simulation cell.
    size_type numberOfPrimaryTetrahedra() const { return _numPrimaryTetrahedra; }

    /// Returns the number of Delaunay vertices in the tessellation, including ghost vertices.
    size_type numberOfVertices() const { return _dt->nb_vertices(); }

    /// Returns the beginning of the iterator range over all Delaunay tetrahedra.
    inline CellIterator begin_cells() const { return boost::make_counting_iterator<size_type>(0); }

    /// Returns the end of the iterator range over all Delaunay tetrahedra.
    inline CellIterator end_cells() const { return boost::make_counting_iterator<size_type>(_dt->nb_cells()); }

    /// Returns the iterator range over all Delaunay tetrahedra.
    inline auto cells() const { return boost::make_iterator_range(begin_cells(), end_cells()); }

    void setCellIndex(CellHandle cell, qint64 value) {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        _cellInfo[cell].index = value;
    }

    inline qint64 getCellIndex(CellHandle cell) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        return _cellInfo[cell].index;
    }

    inline void setUserField(CellHandle cell, int value) {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        _cellInfo[cell].userField = value;
    }

    inline int getUserField(CellHandle cell) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        return _cellInfo[cell].userField;
    }

    /// Determines whether the given tessellation cell connects four physical vertices.
    /// Returns false if one of the four vertices is the infinite vertex.
    inline bool isFiniteCell(CellHandle cell) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        return _dt->cell_is_finite(cell);
    }

    /// Determines whether the given tessellation facet connects three physical vertices.
    /// Returns false if one of the three vertices is the infinite vertex.
    inline bool isFiniteFacet(CellHandle cell, int facetIndex) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(facetIndex >= 0 && facetIndex < 4);
        for(int v = 0; v < 3; v++) {
            if(_dt->cell_vertex(cell, cellFacetVertexIndex(facetIndex, v)) == -1)
                return false;
        }
        return true;
    }

    /// Determines whether the given Delaunay vertex is a ghost vertex or a primary vertex.
    /// This method must not be called for the infinite vertex.
    inline bool isGhostVertex(VertexHandle vertex) const {
        OVITO_ASSERT(vertex >= 0 && vertex < numberOfVertices());
        return vertex >= _primaryVertexCount;
    }

    /// Returns true if the given cell is a ghost cell or an infinite cell.
    inline bool isGhostCell(CellHandle cell) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        return _cellInfo[cell].isGhost;
    }

    /// Returns the i-th vertex of the given Delaunay cell.
    inline VertexHandle cellVertex(CellHandle cell, size_type localIndex) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(localIndex >= 0 && localIndex < 4);
        OVITO_ASSERT(_dt->cell_vertex(cell, localIndex) >= 0); // The request vertex must not be the infinite vertex.
        return _dt->cell_vertex(cell, localIndex);
    }

    /// Returns the four vertices of the given Delaunay cell.
    std::array<VertexHandle, 4> cellVertices(CellHandle cell) const {
        return std::array<VertexHandle, 4>{{ cellVertex(cell, 0), cellVertex(cell, 1), cellVertex(cell, 2), cellVertex(cell, 3) }};
    }

    /// Returns the spatial coordinates of the given Delaunay vertex.
    /// This method must not be called for the infinite vertex.
#ifndef FLOATTYPE_FLOAT
    const Point3& vertexPosition(VertexHandle vertex) const {
        OVITO_ASSERT(vertex >= 0 && vertex < numberOfVertices());
        return *reinterpret_cast<const Point3*>(_dt->vertex_ptr(vertex));
    }
#else
    Point3 vertexPosition(VertexHandle vertex) const {
        OVITO_ASSERT(vertex >= 0 && vertex < numberOfVertices());
        const double* xyz = _dt->vertex_ptr(vertex);
        return Point3((FloatType)xyz[0], (FloatType)xyz[1], (FloatType)xyz[2]);
    }
#endif

    /// Performs the alpha test for the given cell and the given alpha value.
    /// Returns true if the cell passes the alpha test, false if not.
    /// Returns none if the cell is a degenerate sliver element, for which an alpha value cannot be computed.
    std::optional<bool> alphaTest(CellHandle cell, FloatType alpha) const;

    /// Compute the center and radius of the circumscribed sphere of the given (finite) Delaunay cell.
    /// The circum sphere is the sphere that passes through all four vertices of the tetrahedron.
    std::pair<Point3, FloatType> circumSphere(CellHandle cell) const;

    /// Returns the input point index corresponding to the given Delaunay vertex.
    /// This method must not be called for the infinite vertex.
    size_t inputPointIndex(VertexHandle vertex) const {
        OVITO_ASSERT(vertex >= 0 && (size_t)vertex < _inputPointIndices.size());
        return _inputPointIndices[vertex];
    }

    /// Returns the triangular cell facet that is opposite to the given facet.
    Facet mirrorFacet(CellHandle cell, int facet) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(facet >= 0 && facet < 4);
        CellHandle adjacentCell = cellAdjacent(cell, facet);
        OVITO_ASSERT(adjacentCell >= 0);
        return Facet(adjacentCell, adjacentFacet(adjacentCell, cell));
    }

    /// Returns the triangular cell facet that is opposite to the given facet.
    Facet mirrorFacet(const Facet& facet) const {
        return mirrorFacet(facet.first, facet.second);
    }

    /// For a given (global) Delaunay vertex, determines the corresponding local vertex index within a Delaunay cell index.
    /// The Delaunay vertex to be searched for must not be the infinite vertex.
    int findVertexInCell(CellHandle cell, VertexHandle vertex) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(vertex < numberOfVertices());
        for(int iv = 0; iv < 4; iv++) {
            auto v = _dt->cell_vertex(cell, iv);
            if(v == vertex)
                return iv;
        }
        return -1;
    }

    /// For backward compatibility, this method is equivalent to findVertexInCell().
    int localVertexIndex(CellHandle cell, VertexHandle vertex) const {
        return findVertexInCell(cell, vertex);
    }

    /// For a given input point index, determines the corresponding local vertex index within a Delaunay cell.
    int findInputPointInCell(CellHandle cell, size_t pointIndex) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        for(int iv = 0; iv < 4; iv++) {
            auto v = _dt->cell_vertex(cell, iv);
            if(v >= 0 && inputPointIndex(v) == pointIndex) {
#ifdef OVITO_DEBUG
                // Verify that the point index is unique within the cell.
                for(int jv = iv + 1; jv < 4; jv++) {
                    auto v2 = _dt->cell_vertex(cell, jv);
                    OVITO_ASSERT(v2 < 0 || inputPointIndex(v2) != pointIndex);
                }
#endif
                return iv;
            }
        }
        return -1;
    }

    /// Returns the adjacent cell for a given triangular facet.
    CellHandle cellAdjacent(CellHandle cell, int localFace) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(localFace >= 0 && localFace < 4);
        return _dt->cell_adjacent(cell, localFace);
    }

    /// Returns the adjacent cell for a given triangular facet.
    CellHandle cellAdjacent(const Facet& facet) const {
        return _dt->cell_adjacent(facet.first, facet.second);
    }

    /// Retrieves a local facet index from two adjacent cell global indices.
    int adjacentFacet(CellHandle c1, CellHandle c2) const {
        OVITO_ASSERT(c1 >= 0 && c1 < numberOfTetrahedra());
        OVITO_ASSERT(c2 >= 0 && c2 < numberOfTetrahedra());
        OVITO_ASSERT(c2 != c1);
        for(int f = 0; f < 4; f++) {
            if(cellAdjacent(c1, f) == c2)
                return f;
        }
        return -1;
    }

    /// Returns the list of point coordinates used in the tessellation, including ghost points.
    const std::vector<Point3>& points() const { return _pointData; }

    /// Returns the cell vertex for the given triangle vertex of the given cell facet.
    static inline int cellFacetVertexIndex(int cellFacetIndex, int facetVertexIndex) {
        static const int tab_vertex_triple_index[4][3] = {
            {1, 3, 2},
            {0, 2, 3},
            {0, 3, 1},
            {0, 1, 2}
        };
        OVITO_ASSERT(cellFacetIndex >= 0 && cellFacetIndex < 4);
        OVITO_ASSERT(facetVertexIndex >= 0 && facetVertexIndex < 3);
        return tab_vertex_triple_index[cellFacetIndex][facetVertexIndex];
    }

    FacetCirculator incidentFacets(CellHandle cell, int i, int j) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(i >= 0 && i < 4);
        OVITO_ASSERT(j >= 0 && j < 4);
        OVITO_ASSERT(i != j);
        return FacetCirculator(*this, cell, i, j);
    }

    FacetCirculator incidentFacets(CellHandle cell, int i, int j, CellHandle start, int f) const {
        OVITO_ASSERT(cell >= 0 && cell < numberOfTetrahedra());
        OVITO_ASSERT(i >= 0 && i < 4);
        OVITO_ASSERT(j >= 0 && j < 4);
        OVITO_ASSERT(i != j);
        return FacetCirculator(*this, cell, i, j, start, f);
    }

    CellHandle locate(const Point3& p, CellHandle hint) const {
        return _dt->locate_point(p.data(), hint);
    }

    /// Returns the simulation cell geometry.
    const SimulationCellData& simCell() const { return _simCell; }

protected:

    /// Initializes the internal simulation cell information.
    /// Generates an ad-hoc simulation cell from the axis-aligned bounding box of the input points if no simulation cell is provided.
    void initializeSimulationCell(const SimulationCell* simCell, const Point3* points, size_t numPoints, const SelectionIntType* pointMask);

    /// Copies the input points into the internal point list and applies a small random perturbation to each point.
    void compileInputPointList(const Point3* points, size_t numPoints, const SelectionIntType* pointMask);

    /// Creates ghost images of the input points if periodic boundary conditions are enabled.
    void createGhostPointLayer(FloatType ghostLayerSize);

    /// Calls Geogram to construct the actual Delaunay tessellation.
    void constructDelaunayTessellation(TaskProgress& progress);

    /// Classifies tessellation cells as either primary or ghost cells.
    void identifyPrimaryAndGhostCells();

    /// Returns the number of primary (non-ghost) vertices.
    size_type primaryVertexCount() const { return _primaryVertexCount; }

    /// Adds a ghost image point to the internal point list (before Delaunay construction).
    void addGhostPoint(const Point3& point, size_t inputPointIndex) {
        _pointData.push_back(point);
        _inputPointIndices.push_back(inputPointIndex);
    }

private:

    /// Decides whether a tetrahedral cell is a primary cell or a ghost/infinite cell.
    bool classifyGhostCell(CellHandle cell) const;

    /// The internal Delaunay generator object.
    GEO::Delaunay_var _dt;

    /// Stores the coordinates of the input points.
    std::vector<Point_3<double>> _pointData;

    /// Stores per-cell auxiliary data.
    std::vector<CellInfo> _cellInfo;

    /// Mapping of Delaunay vertices to input point indices.
    std::vector<size_t> _inputPointIndices;

    /// The number of primary (non-ghost) vertices.
    size_type _primaryVertexCount;

    /// The number of finite cells in the primary image of the simulation cell.
    size_type _numPrimaryTetrahedra = 0;

    /// The simulation cell (optional).
    SimulationCellData _simCell;
};

}  // End of namespace Ovito
