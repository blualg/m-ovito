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
#include <ovito/core/utilities/concurrent/Task.h>
#include "DelaunayTessellation.h"
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <boost/functional/hash.hpp>

namespace Ovito {

/******************************************************************************
* Generates the tessellation.
******************************************************************************/
void DelaunayTessellation::generateTessellation(const SimulationCell* simCell, const Point3* positions, size_t numPoints, FloatType ghostLayerSize, bool coverDomainWithFiniteTets, const SelectionIntType* selectedPoints)
{
    this_task::setProgressMaximum(0);

    // Initialize the Geogram library (in a thread-safe way).
    static std::mutex geogramMutex;
    {
        std::lock_guard<std::mutex> lock(geogramMutex);
        GEO::initialize(GEO::GEOGRAM_NO_HANDLER);
        GEO::set_assert_mode(GEO::ASSERT_ABORT);
    }

    // Make the magnitude of the randomly perturbed particle positions dependent on the size of the system.
    double lengthScale;
    if(simCell) {
        lengthScale = (simCell->matrix().column(0) + simCell->matrix().column(1) + simCell->matrix().column(2)).length();
    }
    else {
        Box3 bbox;
        bbox.addPoints(positions, numPoints);
        lengthScale = bbox.size().length();
    }
    double epsilon = 1e-10 * lengthScale;

    // Set up random number generator to generate random perturbations.
    std::mt19937 rng;
    std::uniform_real_distribution<double> displacement(-epsilon, epsilon);
    // Use fixed seed value for the sake of reproducibility.
    rng.seed(4);

    _simCell = simCell;

    // Build the list of input points.
    _particleIndices.clear();
    _pointData.clear();

    for(size_t i = 0; i < numPoints; i++, ++positions) {

        // Skip points which are not included.
        if(selectedPoints && !*selectedPoints++)
            continue;

        // Add a small random perturbation to the particle positions to make the Delaunay triangulation more robust
        // against singular input data, e.g. all particles positioned on ideal crystal lattice sites.
        Point3 wp = simCell ? simCell->wrapPoint(*positions) : *positions;
        _pointData.emplace_back(
            (double)wp.x() + displacement(rng),
            (double)wp.y() + displacement(rng),
            (double)wp.z() + displacement(rng));

        _particleIndices.push_back(i);

        this_task::throwIfCanceled();
    }
    _primaryVertexCount = _particleIndices.size();

    if(simCell) {
        // Determine how many periodic copies of the input particles are needed in each cell direction
        // to ensure a consistent periodic topology in the border region.
        Vector3I stencilCount;
        FloatType cuts[3][2];
        Vector3 cellNormals[3];
        for(size_t dim = 0; dim < 3; dim++) {
            cellNormals[dim] = simCell->cellNormalVector(dim);
            cuts[dim][0] = cellNormals[dim].dot(simCell->reducedToAbsolute(Point3(0,0,0)) - Point3::Origin());
            cuts[dim][1] = cellNormals[dim].dot(simCell->reducedToAbsolute(Point3(1,1,1)) - Point3::Origin());

            if(simCell->hasPbc(dim)) {
                stencilCount[dim] = (int)ceil(ghostLayerSize / simCell->matrix().column(dim).dot(cellNormals[dim]));
                cuts[dim][0] -= ghostLayerSize;
                cuts[dim][1] += ghostLayerSize;
            }
            else {
                stencilCount[dim] = 0;
                cuts[dim][0] -= ghostLayerSize;
                cuts[dim][1] += ghostLayerSize;
            }
        }

        // Create ghost images of input vertices.
        for(int ix = -stencilCount[0]; ix <= +stencilCount[0]; ix++) {
            for(int iy = -stencilCount[1]; iy <= +stencilCount[1]; iy++) {
                for(int iz = -stencilCount[2]; iz <= +stencilCount[2]; iz++) {
                    if(ix == 0 && iy == 0 && iz == 0) continue;

                    Vector3 shift = simCell->reducedToAbsolute(Vector3(ix,iy,iz));
                    for(size_t vertexIndex = 0; vertexIndex < _primaryVertexCount; vertexIndex++) {
                        this_task::throwIfCanceled();

                        Point3 pimage = _pointData[vertexIndex] + shift;
                        bool isClipped = false;
                        for(size_t dim = 0; dim < 3; dim++) {
                            if(simCell->hasPbc(dim)) {
                                FloatType d = cellNormals[dim].dot(pimage - Point3::Origin());
                                if(d < cuts[dim][0] || d > cuts[dim][1]) {
                                    isClipped = true;
                                    break;
                                }
                            }
                        }
                        if(!isClipped) {
                            _pointData.push_back(pimage);
                            _particleIndices.push_back(_particleIndices[vertexIndex]);
                        }
                    }
                }
            }
        }
    }

    // In order to cover the simulation box completely with finite tetrahedra, add 8 extra input points to the Delaunay tessellation,
    // far away from the simulation cell and real particles. These 8 points form a convex hull, whose interior will get completely tessellated.
    if(coverDomainWithFiniteTets) {
        OVITO_ASSERT(simCell);

        // Compute bounding box of input points and simulation cell.
        Box3 bb = Box3(Point3(0), Point3(1)).transformed(simCell->matrix());
        bb.addPoints(_pointData.data(), _pointData.size());
        // Add extra padding.
        bb = bb.padBox(ghostLayerSize);
        // Create 8 helper points at the corners of the bounding box.
        for(size_t i = 0; i < 8; i++) {
            Point3 corner = bb[i];
            _pointData.push_back(corner);
            _particleIndices.push_back(std::numeric_limits<size_t>::max());
        }
    }

    // Create the internal Delaunay generator object.
    _dt = GEO::Delaunay::create(3, "BDEL");
    _dt->set_keeps_infinite(true);
    _dt->set_reorder(true);

    // The internal compute_BRIO_order() routine and other parts of Geogram use std::random_shuffle() and the random() function.
    // This results in unstable ordering of the Delaunay cell list, unless we fix the seed number:
    GEO::Numeric::random_reset();

    // Construct Delaunay tessellation.
    _dt->set_vertices(_pointData.size(), reinterpret_cast<const double*>(_pointData.data()), [](GEO::index_t value, GEO::index_t maxProgress) {
        this_task::setProgressMaximum(maxProgress, false);
        this_task::setProgressValueIntermittent(value);
    });
    this_task::throwIfCanceled();

    // Classify tessellation cells as ghost or local cells.
    _numPrimaryTetrahedra = 0;
    _cellInfo.resize(_dt->nb_cells());
    for(CellIterator cellIter = begin_cells(); cellIter != end_cells(); ++cellIter) {
        CellHandle cell = *cellIter;
        if(classifyGhostCell(cell)) {
            _cellInfo[cell].isGhost = true;
            _cellInfo[cell].index = -1;
        }
        else {
            _cellInfo[cell].isGhost = false;
            _cellInfo[cell].index = _numPrimaryTetrahedra++;
        }
    }

    this_task::throwIfCanceled();
}

/******************************************************************************
* Determines whether the given tetrahedral cell is a ghost cell (or an invalid cell).
******************************************************************************/
bool DelaunayTessellation::classifyGhostCell(CellHandle cell) const
{
    if(!isFiniteCell(cell))
        return true;

    // Find head vertex with the lowest index.
    VertexHandle headVertex = cellVertex(cell, 0);
    size_t headVertexIndex = vertexIndex(headVertex);
    for(int v = 1; v < 4; v++) {
        VertexHandle p = cellVertex(cell, v);
        size_t vindex = vertexIndex(p);
        if(vindex < headVertexIndex) {
            headVertex = p;
            headVertexIndex = vindex;
        }
    }

    return isGhostVertex(headVertex);
}

/******************************************************************************
* Computes the dterminant of a 3x3 matrix.
******************************************************************************/
static inline double determinant(double a00, double a01, double a02,
                                 double a10, double a11, double a12,
                                 double a20, double a21, double a22)
{
    double m02 = a00*a21 - a20*a01;
    double m01 = a00*a11 - a10*a01;
    double m12 = a10*a21 - a20*a11;
    double m012 = m01*a22 - m02*a12 + m12*a02;
    return m012;
}

/******************************************************************************
* Alpha test routine.
******************************************************************************/
std::optional<bool> DelaunayTessellation::alphaTest(CellHandle cell, FloatType alpha) const
{
    auto v0 = _dt->vertex_ptr(cellVertex(cell, 0));
    auto v1 = _dt->vertex_ptr(cellVertex(cell, 1));
    auto v2 = _dt->vertex_ptr(cellVertex(cell, 2));
    auto v3 = _dt->vertex_ptr(cellVertex(cell, 3));

    auto qpx = v1[0]-v0[0];
    auto qpy = v1[1]-v0[1];
    auto qpz = v1[2]-v0[2];
    auto qp2 = qpx*qpx + qpy*qpy + qpz*qpz;
    auto rpx = v2[0]-v0[0];
    auto rpy = v2[1]-v0[1];
    auto rpz = v2[2]-v0[2];
    auto rp2 = rpx*rpx + rpy*rpy + rpz*rpz;
    auto spx = v3[0]-v0[0];
    auto spy = v3[1]-v0[1];
    auto spz = v3[2]-v0[2];
    auto sp2 = spx*spx + spy*spy + spz*spz;

    auto num_x = determinant(qpy,qpz,qp2,rpy,rpz,rp2,spy,spz,sp2);
    auto num_y = determinant(qpx,qpz,qp2,rpx,rpz,rp2,spx,spz,sp2);
    auto num_z = determinant(qpx,qpy,qp2,rpx,rpy,rp2,spx,spy,sp2);
    auto den   = determinant(qpx,qpy,qpz,rpx,rpy,rpz,spx,spy,spz);

    FloatType nomin = (num_x*num_x + num_y*num_y + num_z*num_z);
    FloatType denom = (4 * den * den);

#if 0
    // Code is only used for debugging purposes:
    std::array<int,4> searchVertexIds1 = {180620, 458358, 474869, 1603607};
    std::array<int,4> vertexIds;
    for(int v = 0; v < 4; v++)
        vertexIds[v] = cellVertex(cell, v);
    std::sort(vertexIds.begin(), vertexIds.end());
    if(vertexIds == searchVertexIds1)
        qInfo() << "Found element 1 " << "nomin=" << nomin << "denom=" << denom << "(nomin / denom)=" << (nomin / denom) << "alpha=" << alpha;
#endif

    // Detect degnerate sliver elements, for which we cannot compute a reliable alpha value.
    if(std::abs(denom) < 1e-9 && std::abs(nomin) < 1e-9) {
        return {}; // Indeterminate result
    }

    return (nomin / denom) < alpha;
}

#if 0
DelaunayTessellationSpatialQuery::DelaunayTessellationSpatialQuery(const DelaunayTessellation& tessellation, FloatType binSize)
    : _tessellation(tessellation), _binSize(binSize)
{
    // Calculate the bounding box corners
    Point3 bboxMin(std::numeric_limits<FloatType>::max());
    Point3 bboxMax(std::numeric_limits<FloatType>::min());
    for(const Point3& point : _tessellation.points()) {
        for(size_t i = 0; i < 3; ++i) {
            bboxMin[i] = std::min(bboxMin[i], point[i]);
            bboxMax[i] = std::max(bboxMax[i], point[i]);
        }
    }

    // Calculate the cell including the ghost points added during tessellation
    AffineTransformation cell;
    if(_tessellation.simCell()) {
        cell = _tessellation.simCell()->cellMatrix();
        OVITO_ASSERT(!_tessellation.simCell()->is2D());
        // TODO: cleanup this calculation
        Matrix3 scaleMatrix = cell.getMatrix() * Matrix3(Matrix3::Identity());
        Vector3 delta = (bboxMax - bboxMin);
        Vector3 diag = scaleMatrix.diagonal();
        scaleMatrix.setIdentity();
        for(size_t i = 0; i < 3; ++i) {
            scaleMatrix[i][i] = delta[i] / std::abs(diag[i]);
        }
        cell.setMatrix(cell.getMatrix() * scaleMatrix);
        cell.column(3) = bboxMin - Point3::Origin();
    }
    else {
        cell = AffineTransformation({bboxMax[0] - bboxMin[0], 0, 0}, {0, bboxMax[1] - bboxMin[1], 0}, {0, 0, bboxMax[2] - bboxMin[2]},
                                    bboxMin - Point3::Origin());
    }

    // Determine the number of bins along each simulation cell vector.
    for(size_t i = 0; i < 3; ++i) {
        _binCounts[i] = std::max((size_t)std::floor(cell.column(i).length() / _binSize), (size_t)1);
    }

    // Calculate the small cell for each bin
    for(size_t i = 0; i < 3; ++i) {
        _binCell.column(i) = cell.column(i) / (FloatType)_binCounts[i];
    }
    _binCell.column(3) = cell.column(3);

    // Assign each tetrahedron a location hash value based on its centroid's position
    std::vector<size_t> cellHashes(_tessellation.numberOfTetrahedra());
#if 1
    for(size_t cell = 0; cell < _tessellation.numberOfTetrahedra(); ++cell) {
        if(_tessellation.isGhostCell(cell) || !_tessellation.isFiniteCell(cell) || _tessellation.getUserField(cell) != -1) {
            continue;
        }

        bool isFilledTetrehedron = false;
        if(auto alphaTestResult = tessellation.alphaTest(cell, 2 * _binSize)) {
            isFilledTetrehedron = *alphaTestResult;
        }
        if(!isFilledTetrehedron) {
            continue;
        }

        Point3 com = Point3::Origin();
        for(size_t vert = 0; vert < 4; ++vert) {
            com += 0.25 * (_tessellation.vertexPosition(_tessellation.cellVertex(cell, vert)) - Point3::Origin());
        }
        cellHashes[cell] = hashPoint(com);
    }
#else
    parallelFor(_tessellation.numberOfTetrahedra(), 1024, [&](size_t cell) {
        if(_tessellation.isGhostCell(cell) || !_tessellation.isFiniteCell(cell) || _tessellation.getUserField(cell) != -1) {
            return;
        }

        bool isFilledTetrehedron = false;
        if(auto alphaTestResult = _tessellation.alphaTest(cell, 2 * _binSize)) {
            isFilledTetrehedron = *alphaTestResult;
        }
        if(!isFilledTetrehedron) {
            return;
        }

        Point3 com = Point3::Origin();
        for(size_t vert = 0; vert < 4; ++vert) {
            com += 0.25 * (_tessellation.vertexPosition(_tessellation.cellVertex(cell, vert)) - Point3::Origin());
        }
        cellHashes[cell] = hashPoint(com);
    });
#endif

    // Sort the cell indices based on the location hash
    _cellIndices.clear();
    _cellIndices.resize(_tessellation.numberOfTetrahedra(), 0);
    std::iota(_cellIndices.begin(), _cellIndices.end(), 0);
    std::stable_sort(_cellIndices.begin(), _cellIndices.end(), [&](size_t l, size_t r) { return cellHashes[l] < cellHashes[r]; });

    // Create offset table to find indices in the location hash vector
    _cellCounts.clear();
    _cellCounts.resize(_binCounts[0] * _binCounts[1] * _binCounts[2] + 1, size_t(0));
    for(size_t hash : cellHashes) {
        _cellCounts[hash + 1]++;
    }
    std::partial_sum(_cellCounts.cbegin(), _cellCounts.cend(), _cellCounts.begin());
}

[[nodiscard]] size_t DelaunayTessellationSpatialQuery::hashCell(size_t i, size_t j, size_t k) const noexcept
{
    return k + _binCounts[2] * (j + _binCounts[1] * i);
}

[[nodiscard]] size_t DelaunayTessellationSpatialQuery::hashPoint(const Point3& p) const noexcept
{
    Point3 loc = _binCell.inverse() * p;
    size_t i = std::clamp((size_t)std::floor(loc[0]), (size_t)0, _binCounts[0] - 1);
    size_t j = std::clamp((size_t)std::floor(loc[1]), (size_t)0, _binCounts[1] - 1);
    size_t k = std::clamp((size_t)std::floor(loc[2]), (size_t)0, _binCounts[2] - 1);
    return hashCell(i, j, k);
}

// TODO: what to return
[[nodiscard]] std::span<const size_t> DelaunayTessellationSpatialQuery::getRange(size_t hash) const
{
    return std::span(_cellIndices).subspan(_cellCounts[hash], _cellCounts[hash + 1] - _cellCounts[hash]);
}

[[nodiscard]] std::span<const size_t> DelaunayTessellationSpatialQuery::getRange(size_t i, size_t j, size_t k) const
{
    return getRange(hashCell(i, j, k));
}

[[nodiscard]] std::array<int, 3> DelaunayTessellationSpatialQuery::reverseCellHash(size_t hash) const noexcept
{
    int k = static_cast<int>(hash % _binCounts[2]);
    int j = static_cast<int>((hash / _binCounts[2]) % _binCounts[1]);
    int i = static_cast<int>(hash / (_binCounts[1] * _binCounts[2]));
    return {i, j, k};
}

void DelaunayTessellationSpatialQuery::getSurroundingCells(size_t hash, std::vector<std::span<const size_t>>& outRanges) const
{
    outRanges.clear();
    std::array<int, 3> ijk = reverseCellHash(hash);

    // TODO: is this correct -> is there a better way
    std::array<std::array<int, 2>, 3> ijkLimits;
    for(size_t idx = 0; idx < 3; ++idx) {
        ijkLimits[idx][0] = (_tessellation.simCell()->pbcFlags()[idx]) ? SimulationCell::modulo(ijk[idx] - 1, (int)_binCounts[idx])
                                                                       : std::max((int)0, ijk[idx] - 1);
        ijkLimits[idx][1] = (_tessellation.simCell()->pbcFlags()[idx]) ? SimulationCell::modulo(ijk[idx] + 1, (int)_binCounts[idx])
                                                                       : std::min((int)_binCounts[idx] - 1, ijk[idx] + 1);
        if(ijkLimits[idx][0] > ijkLimits[idx][1]) {
            std::swap(ijkLimits[idx][0], ijkLimits[idx][1]);
        }
    }

    for(int ii = ijkLimits[0][0]; ii <= ijkLimits[0][1]; ++ii) {
        OVITO_ASSERT(ii >= 0 && ii < _binCounts[0]);
        for(int jj = ijkLimits[1][0]; jj <= ijkLimits[1][1]; ++jj) {
            OVITO_ASSERT(jj >= 0 && jj < _binCounts[1]);
            for(int kk = ijkLimits[2][0]; kk <= ijkLimits[2][1]; ++kk) {
                OVITO_ASSERT(kk >= 0 && kk < _binCounts[2]);

                outRanges.push_back(getRange(hashCell(ii, jj, kk)));
            }
        }
    }
}
#else

/******************************************************************************
 * Initialize the query class with a tessellation and a alpha value
 * Alpha can be used to pre-filter cells added to the tree
 ******************************************************************************/
DelaunayTessellationSpatialQuery::DelaunayTessellationSpatialQuery(const DelaunayTessellation& tessellation, std::optional<FloatType> alpha)
    : _tessellation(tessellation)
{
    namespace bg = boost::geometry;

    // Create rtree with the bounding boxes
    // rtree insertion is not thread safe!
    for(size_t cell = 0; cell < _tessellation.numberOfTetrahedra(); ++cell) {
        // Only add defective and finite tetrahedrons
        if(!_tessellation.isFiniteCell(cell) || _tessellation.getUserField(cell) != -1) {
            continue;
        }

        // Skip based on alpha criterion
        if(alpha) {
            bool isFilledTetrehedron = false;
            if(auto alphaTestResult = tessellation.alphaTest(cell, alpha.value())) {
                isFilledTetrehedron = *alphaTestResult;
            }
            if(!isFilledTetrehedron) {
                continue;
            }
        }

        // Add bbounding box to tree
        bPoint bboxLo{std::numeric_limits<Point3::value_type>::max(), cell};
        bPoint bboxHi{std::numeric_limits<Point3::value_type>::min(), cell};
        for(size_t vert = 0; vert < 4; ++vert) {
            Point3 vertex = _tessellation.vertexPosition(_tessellation.cellVertex(cell, vert));
            bg::set<0>(bboxLo, std::min(bg::get<0>(bboxLo), vertex[0]));
            bg::set<0>(bboxHi, std::max(bg::get<0>(bboxHi), vertex[0]));
            bg::set<1>(bboxLo, std::min(bg::get<1>(bboxLo), vertex[1]));
            bg::set<1>(bboxHi, std::max(bg::get<1>(bboxHi), vertex[1]));
            bg::set<2>(bboxLo, std::min(bg::get<0>(bboxLo), vertex[2]));
            bg::set<2>(bboxHi, std::max(bg::get<0>(bboxHi), vertex[2]));
        }
        _rtree.insert(bBox(bboxLo, bboxHi));
    }
}

/******************************************************************************
 * Get all cells intersecting with a given bounding box
 * Target bounding box is defined by bboxLo and bboxHi
 * Boxes are returned in the cells vector
 ******************************************************************************/
void DelaunayTessellationSpatialQuery::getCells(const Point3& bboxLo, const Point3& bboxHi, std::vector<bBox>& cells) const
{
    namespace bgi = boost::geometry::index;
    cells.clear();
    _rtree.query(bgi::intersects(bBox(bPoint{bboxLo}, bPoint{bboxHi})), std::back_inserter(cells));
}

#endif

}  // namespace Ovito
