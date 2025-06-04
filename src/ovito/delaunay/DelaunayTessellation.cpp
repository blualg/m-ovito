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

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "DelaunayTessellation.h"

namespace Ovito {

/******************************************************************************
* Generates the tessellation.
******************************************************************************/
void DelaunayTessellation::generateTessellation(const SimulationCell* cell, const Point3* positions, size_t numPoints, FloatType ghostLayerSize, bool coverDomainWithFiniteTets, const SelectionIntType* selectedPoints, TaskProgress& progress)
{
    progress.setMaximum(0);

    // Set up the simulation cell geometry.
    initializeSimulationCell(cell, positions, numPoints, selectedPoints);

    // Compile the list of primary input points.
    compileInputPointList(positions, numPoints, selectedPoints);

    // Create ghost images of the input points if periodic boundary conditions are enabled.
    createGhostPointLayer(ghostLayerSize);

    // In order to cover the simulation box completely with finite tetrahedra, add 8 extra input points to the Delaunay tessellation,
    // far away from the simulation cell and real particles. These 8 points form a convex hull, whose interior will get completely tessellated.
    if(coverDomainWithFiniteTets) {
        // Compute bounding box of input points and simulation cell.
        Box3 bb = Box3(Point3(0), Point3(1)).transformed(simCell().cellMatrix());
        bb.addPoints(_pointData.data(), _pointData.size());
        // Add extra padding.
        bb = bb.padBox(ghostLayerSize);
        // Create 8 helper points at the corners of the bounding box.
        for(size_t i = 0; i < 8; i++) {
            Point3 corner = bb[i];
            _pointData.push_back(corner);
            _inputPointIndices.push_back(std::numeric_limits<size_t>::max());
        }
    }

    // Actually construct the Delaunay tessellation by calling Geogram.
    constructDelaunayTessellation(progress);

    // Classify tessellation cells into primary and ghost cells.
    identifyPrimaryAndGhostCells();
}

/******************************************************************************
* Initializes the internal simulation cell information.
* Generates an ad-hoc simulation cell from the axis-aligned bounding box of
* the input points if no simulation cell is provided.
******************************************************************************/
void DelaunayTessellation::initializeSimulationCell(const SimulationCell* simCell, const Point3* points, size_t numPoints, const SelectionIntType* pointMask)
{
    if(simCell) {
        _simCell = simCell;
    }
    else {
        Box3 bbox;
        if(!pointMask) {
            bbox.addPoints(points, numPoints);
        }
        else {
            for(size_t i = 0; i < numPoints; i++) {
                if(pointMask[i])
                    bbox.addPoint(points[i]);
            }
        }
        _simCell = bbox;
    }
}

/******************************************************************************
* Copies the input points into the internal point list and applies a small random perturbation to each point.
******************************************************************************/
void DelaunayTessellation::compileInputPointList(const Point3* points, size_t numPoints, const SelectionIntType* pointMask)
{
    // Make the magnitude of the random perturbations dependent on the size of the system.
    const double lengthScale = (simCell().cellVector1() + simCell().cellVector2() + simCell().cellVector3()).length();
    const double epsilon = 1e-10 * lengthScale;

    // Set up random number generator to generate random perturbations.
    // Use a fixed seed value for the sake of reproducibility.
    std::mt19937 rng(4);
    boost::random::uniform_real_distribution<double> displacement(-epsilon, epsilon);

    // Build the list of input points.
    _inputPointIndices.clear();
    _pointData.clear();
    _inputPointIndices.reserve(numPoints);

    for(size_t i = 0; i < numPoints; i++, ++points) {

        // Skip points which are not included.
        if(pointMask && !*pointMask++)
            continue;

        // Add a small random perturbation to the particle positions to make the Delaunay triangulation more robust
        // against singular input data, e.g. all particles positioned on ideal crystal lattice sites.
        const Point3 wp = simCell().wrapPoint(*points);
        _pointData.emplace_back(
            (double)wp.x() + displacement(rng),
            (double)wp.y() + displacement(rng),
            (double)wp.z() + displacement(rng));

        _inputPointIndices.push_back(i);

        this_task::throwIfCanceled();
    }
    _primaryVertexCount = _inputPointIndices.size();
}

/******************************************************************************
* Creates ghost images of the input points if periodic boundary conditions are enabled.
******************************************************************************/
void DelaunayTessellation::createGhostPointLayer(FloatType ghostLayerSize)
{
    if(!simCell().hasPbc())
        return; // No periodic boundary conditions, no ghost points needed.
    if(ghostLayerSize <= 0)
        return; // No ghost layer requested.

    // Determine how many periodic copies of the input particles are needed in each cell direction
    // to cover the ghost layer of the simulation cell.
    Vector3I stencilCount;
    FloatType cuts[3][2];
    Vector3 cellNormals[3];
    for(size_t dim = 0; dim < 3; dim++) {
        if(simCell().hasPbc(dim)) {
            cellNormals[dim] = simCell().cellNormalVector(dim);
            cuts[dim][0] = cellNormals[dim].dot(simCell().reducedToAbsolute(Point3(0,0,0)) - Point3::Origin()) - ghostLayerSize;
            cuts[dim][1] = cellNormals[dim].dot(simCell().reducedToAbsolute(Point3(1,1,1)) - Point3::Origin()) + ghostLayerSize;
            stencilCount[dim] = (int)std::ceil(ghostLayerSize / simCell().cellMatrix().column(dim).dot(cellNormals[dim]));
        }
        else {
            stencilCount[dim] = 0;
        }
    }

    // Create ghost images of input vertices.
    for(int ix = -stencilCount[0]; ix <= +stencilCount[0]; ix++) {
        for(int iy = -stencilCount[1]; iy <= +stencilCount[1]; iy++) {
            for(int iz = -stencilCount[2]; iz <= +stencilCount[2]; iz++) {
                if(ix == 0 && iy == 0 && iz == 0) continue;

                Vector3 shift = simCell().reducedToAbsolute(Vector3(ix,iy,iz));
                for(size_type vertexIndex = 0; vertexIndex < primaryVertexCount(); vertexIndex++) {
                    this_task::throwIfCanceled();

                    // Create a shifted ghost image of the input point.
                    Point3 pimage = this->points()[vertexIndex] + shift;
                    bool isClipped = false;
                    for(size_t dim = 0; dim < 3; dim++) {
                        if(simCell().hasPbc(dim)) {
                            FloatType d = cellNormals[dim].dot(pimage - Point3::Origin());
                            if(d < cuts[dim][0] || d > cuts[dim][1]) {
                                isClipped = true;
                                break;
                            }
                        }
                    }
                    if(!isClipped) {
                        addGhostPoint(pimage, inputPointIndex(vertexIndex));
                    }
                }
            }
        }
    }
}

/******************************************************************************
* Calls Geogram to construct the actual Delaunay tessellation.
******************************************************************************/
void DelaunayTessellation::constructDelaunayTessellation(TaskProgress& progress)
{
    // Create the internal Geogram Delaunay generator object.
    if(!_dt) {

        // Globally initializes the Geogram library.
        static std::mutex geogramMutex;
        {
            std::lock_guard<std::mutex> lock(geogramMutex);
            GEO::initialize(GEO::GEOGRAM_NO_HANDLER);
            GEO::set_assert_mode(GEO::ASSERT_ABORT);
        }

        _dt = GEO::Delaunay::create(3, "BDEL");
        _dt->set_keeps_infinite(true);
        _dt->set_reorder(true);
    }

    // Install a callback function to report progress during tessellation construction.
    if(&progress != &TaskProgress::Ignore) {
        _dt->set_progress_callback([&progress](GEO::index_t value, GEO::index_t maxProgress) {
            progress.setMaximum(maxProgress, false);
            progress.setValueIntermittent(value);
        });
    }
    else {
        _dt->set_progress_callback(nullptr);
    }

    // Construct Delaunay tessellation.
    _dt->set_vertices(_pointData.size(), reinterpret_cast<const double*>(_pointData.data()));

    this_task::throwIfCanceled();
}

/******************************************************************************
* Classifies tessellation cells as either primary or ghost cells.
******************************************************************************/
void DelaunayTessellation::identifyPrimaryAndGhostCells()
{
    // Infinite cells are also considered ghost cells.
    _numPrimaryTetrahedra = 0;
    _cellInfo.resize(_dt->nb_cells());
    for(CellHandle cell : cells()) {
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
* Decides whether a tetrahedral cell is a primary cell or a ghost/infinite cell.
******************************************************************************/
bool DelaunayTessellation::classifyGhostCell(CellHandle cell) const
{
    // Check if the cell is infinite.
    if(isFiniteCell(cell) == false)
        return true;

    // The cell is a primary cell if the vertex with the lowest input point index is a primary vertex.
    // Find head vertex with the lowest input point index.
    VertexHandle headVertex = cellVertex(cell, 0);
    size_t headVertexIndex = inputPointIndex(headVertex);
    for(int v = 1; v < 4; v++) {
        VertexHandle p = cellVertex(cell, v);
        size_t vindex = inputPointIndex(p);
        if(vindex < headVertexIndex) {
            headVertex = p;
            headVertexIndex = vindex;
        }
    }

    return isGhostVertex(headVertex);
}

/******************************************************************************
* Computes the determinant of a 3x3 matrix.
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
* Performs the alpha test for the given cell and the given alpha value.
* Returns true if the cell passes the alpha test, false if not.
* Returns none if the cell is a degenerate sliver element, for which an alpha value cannot be computed.
******************************************************************************/
std::optional<bool> DelaunayTessellation::alphaTest(CellHandle cell, FloatType alpha) const
{
    OVITO_ASSERT(isFiniteCell(cell)); // Circum sphere can only be computed for finite cells.

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

    // Detect degenerate sliver elements, for which we cannot compute a reliable alpha value.
    if(std::abs(denom) < 1e-9 && std::abs(nomin) < 1e-9) {
        return std::nullopt; // Indeterminate result
    }

    return (nomin / denom) < alpha;
}

/******************************************************************************
* Compute the center and radius of the circumscribed sphere of the given (finite) Delaunay cell.
* The circum sphere is the sphere that passes through all four vertices of the tetrahedron.
******************************************************************************/
std::pair<Point3, FloatType> DelaunayTessellation::circumSphere(CellHandle cell) const
{
    OVITO_ASSERT(isFiniteCell(cell)); // Circum sphere can only be computed for finite cells.

    // Get the coordinates of the four vertices
    const double* v0 = _dt->vertex_ptr(cellVertex(cell, 0));
    const double* v1 = _dt->vertex_ptr(cellVertex(cell, 1));
    const double* v2 = _dt->vertex_ptr(cellVertex(cell, 2));
    const double* v3 = _dt->vertex_ptr(cellVertex(cell, 3));

    // Let a = v0, b = v1, c = v2, d = v3
    // Compute vectors relative to a
    double bax[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
    double cax[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
    double dax[3] = { v3[0] - v0[0], v3[1] - v0[1], v3[2] - v0[2] };

    // Compute squared lengths
    double balen2 = bax[0]*bax[0] + bax[1]*bax[1] + bax[2]*bax[2];
    double calen2 = cax[0]*cax[0] + cax[1]*cax[1] + cax[2]*cax[2];
    double dalen2 = dax[0]*dax[0] + dax[1]*dax[1] + dax[2]*dax[2];

    // Compute cross products
    double cross_cd[3] = {
        cax[1]*dax[2] - cax[2]*dax[1],
        cax[2]*dax[0] - cax[0]*dax[2],
        cax[0]*dax[1] - cax[1]*dax[0]
    };
    double cross_db[3] = {
        dax[1]*bax[2] - dax[2]*bax[1],
        dax[2]*bax[0] - dax[0]*bax[2],
        dax[0]*bax[1] - dax[1]*bax[0]
    };
    double cross_bc[3] = {
        bax[1]*cax[2] - bax[2]*cax[1],
        bax[2]*cax[0] - bax[0]*cax[2],
        bax[0]*cax[1] - bax[1]*cax[0]
    };

    // Compute denominator (6 times the volume of the tetrahedron)
    double denom = 2.0 * (
        bax[0] * (cax[1]*dax[2] - cax[2]*dax[1]) -
        bax[1] * (cax[0]*dax[2] - cax[2]*dax[0]) +
        bax[2] * (cax[0]*dax[1] - cax[1]*dax[0])
    );

    // If denom is zero, the points are coplanar or degenerate
    if(std::abs(denom) < 1e-16)
        return { Point3(v0[0], v0[1], v0[2]), 0.0 };

    // Compute circumcenter relative to a (v0)
    double cx = (balen2 * (cross_cd[0]) + calen2 * (cross_db[0]) + dalen2 * (cross_bc[0])) / denom;
    double cy = (balen2 * (cross_cd[1]) + calen2 * (cross_db[1]) + dalen2 * (cross_bc[1])) / denom;
    double cz = (balen2 * (cross_cd[2]) + calen2 * (cross_db[2]) + dalen2 * (cross_bc[2])) / denom;

    Point3 center(v0[0] + cx, v0[1] + cy, v0[2] + cz);

    // Compute radius
    double dx = center.x() - v0[0];
    double dy = center.y() - v0[1];
    double dz = center.z() - v0[2];
    FloatType radius = std::sqrt(dx*dx + dy*dy + dz*dz);

    return { center, radius };
}

}  // namespace Ovito
