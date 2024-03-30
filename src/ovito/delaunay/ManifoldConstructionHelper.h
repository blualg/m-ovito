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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/delaunay/DelaunayTessellation.h>

namespace Ovito {

/**
 * Constructs a SurfaceMesh structure from a DelaunayTessellation representing the separating surface manifold
 * between different spatial regions of the tessellation.
 */
class ManifoldConstructionHelper
{
public:

    // A no-op face-preparation functor.
    struct DefaultPrepareMeshFaceFunc {
        void operator()(SurfaceMesh::face_index face,
                const std::array<size_t,3>& vertexIndices,
                const std::array<DelaunayTessellation::VertexHandle,3>& vertexHandles,
                DelaunayTessellation::CellHandle cell) {}
    };

    // A no-op vertex-preparation functor.
    struct DefaultPrepareMeshVertexFunc {
        void operator()(SurfaceMesh::vertex_index vertex, size_t particleIndex) {}
    };

public:

    /// Constructor.
    ManifoldConstructionHelper(DelaunayTessellation& tessellation, SurfaceMeshBuilder& outputMesh, FloatType alpha, bool createRegions,
            const Property* positions) : _tessellation(tessellation), _mesh(outputMesh), _alpha(alpha), _createRegions(createRegions), _positions(positions) { OVITO_ASSERT(_tessellation.simCell()); }

    /// Returns the number of filled regions that have been identified.
    SurfaceMesh::size_type filledRegionCount() const { return _filledRegionCount; }

    /// Returns the number of empty regions that have been identified.
    SurfaceMesh::size_type emptyRegionCount() const { return _emptyRegionCount; }

    /// This is the main function, which constructs the manifold triangle mesh.
    template<typename CellRegionFunc, typename PrepareMeshFaceFunc = DefaultPrepareMeshFaceFunc, typename PrepareMeshVertexFunc = DefaultPrepareMeshVertexFunc>
    void construct(CellRegionFunc&& determineCellRegion, PrepareMeshFaceFunc&& prepareMeshFaceFunc = PrepareMeshFaceFunc(), PrepareMeshVertexFunc&& prepareMeshVertexFunc = PrepareMeshVertexFunc())
    {
        // Algorithm is divided into several sub-steps.
        if(_createRegions)
            this_task::beginProgressSubStepsWithWeights({1,8,2,1});
        else
            this_task::beginProgressSubStepsWithWeights({1,1,2});

        // Assign tetrahedra to spatial regions.
        classifyTetrahedra(std::move(determineCellRegion));
        this_task::throwIfCanceled();
        this_task::nextProgressSubStep();

        // Group connected tetrahedra into volumetric regions.
        if(_createRegions) {
            formFilledRegions();
            this_task::throwIfCanceled();
            this_task::nextProgressSubStep();
        }

        // Create triangle facets at interfaces between two different regions.
        createInterfaceFacets(std::move(prepareMeshFaceFunc), std::move(prepareMeshVertexFunc));
        this_task::throwIfCanceled();
        this_task::nextProgressSubStep();

        // Connect triangles with one another to form a closed manifold.
        linkHalfedges();
        this_task::throwIfCanceled();

#ifdef OVITO_DEBUG
        // Verify that generated manifold connectivity is correct when a two-sided mesh was created.
        if(_createRegions) {
            for(SurfaceMesh::edge_index edge = 0; edge < _mesh.edgeCount(); edge++) {
                // Each edge must be part of at least two opposite manifolds.
                OVITO_ASSERT(_mesh.countManifolds(edge) >= 2);
                // Manifold count of the edge must be consistent with its opposite edge.
                OVITO_ASSERT(_mesh.countManifolds(edge) == _mesh.countManifolds(_mesh.oppositeEdge(edge)));
            }
        }
#endif

        this_task::endProgressSubSteps();
        this_task::throwIfCanceled();
    }

    /// Finds disconnected empty regions and computes their volumes.
    void formEmptyRegions() {
        this_task::beginProgressSubStepsWithWeights({1,1});

        // Count number of identified regions.
        _emptyRegionCount = 0;

        // Flags indicating which periodic cell directions are connected by a surface through the cell boundary.
        const SimulationCell* simCell = _tessellation.simCell();
        bool surfaceCrossesBoundaries[3] = { false, false, false };
        bool detectBoundaryCrossings = simCell->hasPbc();

        // Stack of faces to visit. Used for implementing recursive algorithm.
        std::deque<SurfaceMesh::face_index> facesToProcess;

        // Access the per-face region indices.
        BufferWriteAccess<SurfaceMesh::region_index, access_mode::read_write> faceRegions(_mesh.mutableFaceProperty(SurfaceMeshFaces::RegionProperty));

        // Access the vertex coordinates.
        BufferReadAccess<Point3> vertexPositions(_mesh.expectVertexProperty(SurfaceMeshVertices::PositionProperty));

        // Identify disconnected components of the surface mesh bordering to an empty region.
        this_task::setProgressMaximum(_mesh.faceCount() / 2); // Note: Dividing by two, because only every other face is oriented towards the empty region.
        size_t progressValue = 0;
        for(SurfaceMesh::face_index face = 0; face < _mesh.faceCount(); face++) {
            // Look for mesh faces that are not adjacent to a filled region and which have not been visited yet.
            SurfaceMesh::region_index faceRegion = faceRegions[face];
            if(faceRegion != SurfaceMesh::InvalidIndex)
                continue;

            // Found a first seed face. Start a new mesh component.
            faceRegion = _filledRegionCount + (_emptyRegionCount++);
            faceRegions[face] = faceRegion;
            size_t faceCount = 1;

            facesToProcess.push_back(face);
            do {
                // Take next face from the stack.
                SurfaceMesh::face_index currentFace = facesToProcess.front();
                facesToProcess.pop_front();
                this_task::setProgressValueIntermittent(progressValue++);

                // Visit neighbors of current face.
                SurfaceMesh::edge_index firstEdge = _mesh.firstFaceEdge(currentFace);
                SurfaceMesh::edge_index edge = firstEdge;
                OVITO_ASSERT(firstEdge != SurfaceMesh::InvalidIndex);
                do {
                    // Determine whether this edge crosses a periodic simulation cell boundary.
                    if(detectBoundaryCrossings) {
                        const Point3& p1 = vertexPositions[_mesh.vertex1(edge)];
                        const Point3& p2 = vertexPositions[_mesh.vertex2(edge)];
                        Vector3 delta = p2 - p1;
                        for(size_t dim = 0; dim < 3; dim++) {
                            if(!surfaceCrossesBoundaries[dim] && simCell->hasPbc(dim)) {
                                // The edge is crossing the periodic boundary if it spans more than half of the simulation cell in that direction.
                                if(std::abs(simCell->inverseMatrix().prodrow(delta, dim)) >= FloatType(0.5)) {
                                    surfaceCrossesBoundaries[dim] = true;
                                    detectBoundaryCrossings = (simCell->hasPbc(0) && !surfaceCrossesBoundaries[0]) || (simCell->hasPbc(1) && !surfaceCrossesBoundaries[1]) || (simCell->hasPbc(2) && !surfaceCrossesBoundaries[2]);
                                }
                            }
                        }
                    }

                    // Get mesh face adjacent to the opposite half-edge of the current half-edge.
                    SurfaceMesh::edge_index oppositeEdge = _mesh.oppositeEdge(edge);
                    OVITO_ASSERT(oppositeEdge != SurfaceMesh::InvalidIndex);
                    SurfaceMesh::face_index neighborFace = _mesh.adjacentFace(oppositeEdge);
                    OVITO_ASSERT(neighborFace != SurfaceMesh::InvalidIndex);

                    if(faceRegions[neighborFace] == SurfaceMesh::InvalidIndex) {
                        // Assign neighbor face to the current empty region.
                        faceRegions[neighborFace] = faceRegion;
                        // Put on recursive stack.
                        facesToProcess.push_back(neighborFace);
                        faceCount++;
                    }

                    edge = _mesh.nextFaceEdge(edge);
                }
                while(edge != firstEdge);
            }
            while(!facesToProcess.empty());
        }

        this_task::nextProgressSubStep();
        std::deque<DelaunayTessellation::CellHandle> cellsToProcess;

        // Disjoint set data structure for merging empty regions:
        std::vector<size_t> regionParents(_emptyRegionCount);
        std::vector<size_t> regionSizes(_emptyRegionCount);
        std::vector<double> regionVolumes(_emptyRegionCount, 0.0);
        boost::dynamic_bitset<> regionIsExterior(_emptyRegionCount);
        std::iota(regionParents.begin(), regionParents.end(), (size_t)0);
        std::fill(regionSizes.begin(), regionSizes.end(), 1);

        // "Find" part of Union-Find:
        auto findRegion = [&](size_t index) {
            OVITO_ASSERT(index < regionParents.size());
            // Find root and make root as parent of i (path compression).
            size_t parent = regionParents[index];
            while(parent != regionParents[parent]) {
                parent = regionParents[parent];
            }
            regionParents[index] = parent;
            return parent;
        };
        // "Union" part of Union-Find.
        auto mergeRegions = [&](size_t regionA, size_t regionB) {
            if(regionA == regionB) return;
            size_t parentA = findRegion(regionA - _filledRegionCount);
            size_t parentB = findRegion(regionB - _filledRegionCount);
            if(parentA != parentB) {
                // Attach smaller tree under root of larger tree.
                if(regionSizes[parentA] < regionSizes[parentB]) {
                    regionParents[parentA] = parentB;
                    regionSizes[parentB] += regionSizes[parentA];
                    regionVolumes[parentB] += regionVolumes[parentA];
                    regionIsExterior[parentB] |= regionIsExterior[parentA];
                }
                else {
                    regionParents[parentB] = parentA;
                    regionSizes[parentA] += regionSizes[parentB];
                    regionVolumes[parentA] += regionVolumes[parentB];
                    regionIsExterior[parentA] |= regionIsExterior[parentB];
                }
            }
        };

        // The ID of the empty region that is split by a periodic simulation box boundary.
        SurfaceMesh::region_index splitPeriodicRegion = SurfaceMesh::InvalidIndex;

        // Loop over all cells to cluster them.
        this_task::setProgressMaximum(_tessellation.numberOfTetrahedra() - _numFilledCells);
        progressValue = 0;
        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;

            // Only consider finite cells.
            if(!_tessellation.isFiniteCell(cell))
                continue;

            // Skip filled cells, or cells that have been visited before.
            if(_tessellation.getUserField(cell) != SurfaceMesh::InvalidIndex)
                continue;

            // Determine whether the cell is adjacent to a face of the generated surface mesh.
            SurfaceMesh::region_index emptyRegion = SurfaceMesh::InvalidIndex;
            for(int f = 0; f < 4; f++) {

                SurfaceMesh::face_index adjacentMeshFace = findCellFace(_tessellation.mirrorFacet(cell, f));
                if(adjacentMeshFace == SurfaceMesh::InvalidIndex)
                    continue;
                SurfaceMesh::face_index oppositeFace = _mesh.oppositeFace(adjacentMeshFace);

                OVITO_ASSERT(oppositeFace != SurfaceMesh::InvalidIndex);
                // The surface mesh face should be bordering an empty and not a filled region.
                if(faceRegions[oppositeFace] >= _filledRegionCount)
                    emptyRegion = faceRegions[oppositeFace];
            }

            // Skip the cell if it is not adjacent to a face of the surface mesh.
            if(emptyRegion == SurfaceMesh::InvalidIndex)
                continue;

            // Start recursive algorithm to gather all connected empty tetrahedra.
            cellsToProcess.push_back(cell);
            _tessellation.setUserField(cell, emptyRegion);
            double regionVolume = 0;
            bool touchesOpenBoundaries = false;
            do {
                // Take next tetrahedron from the stack.
                DelaunayTessellation::CellHandle currentCell = cellsToProcess.front();
                cellsToProcess.pop_front();
                this_task::setProgressValueIntermittent(progressValue++);

                // Flags indicating whether the current tetrahedron crosses the cell boundaries.
                std::array<bool,3> cellCrossesBoundaries = {false, false, false};

                // Compute the overlap of the tetrahedron with the simulation box.
                FloatType overlapVolume = calculateVolumeOverlap(currentCell, cellCrossesBoundaries);

                // Detect whether the region is an exterior empty region, i.e. it is adjacent to the open boundaries of the simulation cell.
                touchesOpenBoundaries |=
                    (cellCrossesBoundaries[0] && !_tessellation.simCell()->pbcX()) ||
                    (cellCrossesBoundaries[1] && !_tessellation.simCell()->pbcY()) ||
                    (cellCrossesBoundaries[2] && !_tessellation.simCell()->pbcZ());

                // Stop at cells that are completely outside of the simulation box.
                if(overlapVolume == 0)
                    continue;
                regionVolume += overlapVolume;

                // Determine whether the current spatial region is split by a periodic box boundary
                // that is not crossed by the surface mesh. In this case, we need to merge the two empty
                // regions that were created separately on either side of the simulation box boundary.
                for(size_t dim = 0; dim < 3; dim++) {
                    if(cellCrossesBoundaries[dim] && !surfaceCrossesBoundaries[dim] && simCell->hasPbc(dim)) {
                        if(splitPeriodicRegion == SurfaceMesh::InvalidIndex)
                            splitPeriodicRegion = emptyRegion;
                        else
                            mergeRegions(emptyRegion, splitPeriodicRegion);
                    }
                }

                // Continue with neighboring cells.
                for(int f = 0; f < 4; f++) {

                    // Check if this is a region border or not.
                    DelaunayTessellation::Facet mirrorFacet = _tessellation.mirrorFacet(currentCell, f);
                    SurfaceMesh::face_index adjacentMeshFace = findCellFace(mirrorFacet);
                    if(adjacentMeshFace != SurfaceMesh::InvalidIndex) {
                        // We have hit a region border. Merge the two empty region IDs.
                        SurfaceMesh::face_index oppositeFace = _mesh.oppositeFace(adjacentMeshFace);
                        SurfaceMesh::region_index secondEmptyRegion = faceRegions[oppositeFace];
                        if(secondEmptyRegion >= _filledRegionCount) {
                            mergeRegions(emptyRegion, secondEmptyRegion);
                        }
                    }
                    else {
                        DelaunayTessellation::CellHandle neighborCell = mirrorFacet.first;

                        // Only consider finite cells.
                        if(!_tessellation.isFiniteCell(neighborCell))
                            continue;

                        // Skip filled cells, or cells that have been visited before.
                        SurfaceMesh::region_index neighborRegion = _tessellation.getUserField(neighborCell);
                        if(neighborRegion != SurfaceMesh::InvalidIndex) {
                            if(neighborRegion >= _filledRegionCount)
                                mergeRegions(emptyRegion, neighborRegion);
                            continue;
                        }

                        // Put neighbor cell on processing stack.
                        _tessellation.setUserField(neighborCell, emptyRegion);
                        cellsToProcess.push_back(neighborCell);
                    }
                }
            }
            while(!cellsToProcess.empty());

            // Add the total volume of the visited tetrahedra to the region's volume.
            auto index = findRegion(emptyRegion - _filledRegionCount);
            regionVolumes[index] += regionVolume;

            // Mark the region as exterior/interior empty space.
            regionIsExterior[index] |= touchesOpenBoundaries;
        }
        this_task::throwIfCanceled();

        // Create the "Exterior" region property in the output mesh.
        Property* regionPropertyIsExterior = _mesh.createRegionProperty(DataBuffer::Initialized, SurfaceMeshRegions::IsExteriorProperty);

        // Get access to the region volume property.
        Property* regionPropertyVolume = _mesh.mutableRegionProperty(SurfaceMeshRegions::VolumeProperty);

        OVITO_ASSERT(_mesh.regionCount() == _filledRegionCount);

        // Remap merged regions to contiguous range.
        std::vector<SurfaceMesh::region_index> regionMapping(_emptyRegionCount);
        PropertyContainer::Grower regionGrower(_mesh.mutableRegions());
        _emptyRegionCount = 0;
        for(size_t i = 0; i < regionParents.size(); i++) {
            if(findRegion(i) == i) {
                SurfaceMesh::region_index ridx = regionGrower.grow(1);
                OVITO_ASSERT(ridx == _emptyRegionCount + _filledRegionCount);
                regionMapping[i] = ridx;
                BufferWriteAccess<SelectionIntType, access_mode::write>{regionPropertyIsExterior}[ridx] = regionIsExterior[i];
                BufferWriteAccess<FloatType, access_mode::write>{regionPropertyVolume}[ridx] = regionVolumes[i];
                _emptyRegionCount++;
            }
        }

        // Map each mesh face's preliminary region ID to the final ID.
        for(SurfaceMesh::region_index& region : faceRegions) {
            if(region >= _filledRegionCount) {
                region = regionMapping[findRegion(region - _filledRegionCount)];
            }
        }

        // Update tesselation user field to reflect the compressed empty region IDs
        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;
            int currentRegionId = _tessellation.getUserField(cell);
            if(currentRegionId >= _filledRegionCount) {
                _tessellation.setUserField(cell, regionMapping[findRegion(currentRegionId - _filledRegionCount)]);
            }
        }

        // Create a single space-filling empty region if there is no filled region at all.
        if(_emptyRegionCount == 0 && _filledRegionCount == 0) {
            regionGrower.grow(1);
            BufferWriteAccess<FloatType, access_mode::discard_write>{regionPropertyVolume}[0] = simCell->volume3D();
            BufferWriteAccess<SelectionIntType, access_mode::discard_write>{regionPropertyIsExterior}[0] = (!simCell->pbcX() || !simCell->pbcY() || !simCell->pbcZ());
            _emptyRegionCount = 1;
        }

        this_task::endProgressSubSteps();
    }

private:

    /// Assigns each tetrahedron to a region.
    template<typename CellRegionFunc>
    void classifyTetrahedra(CellRegionFunc&& determineCellRegion)
    {
        this_task::setProgressMaximum(_tessellation.numberOfTetrahedra());

        _numFilledCells = 0;
        size_t progressCounter = 0;
        _mesh.setSpaceFillingRegion(SurfaceMesh::InvalidIndex);
        bool spaceFillingRegionUndetermined = true;
        bool isSpaceFilling = true;
        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;

            // Update progress indicator.
            this_task::setProgressValueIntermittent(progressCounter++);

            // Alpha-shape criterion: This determines whether the Delaunay tetrahedron is part of a filled region.
            bool isFilledTetrehedron = false;
            if(_tessellation.isFiniteCell(cell)) {
                if(auto alphaTestResult = _tessellation.alphaTest(cell, _alpha)) {
                    isFilledTetrehedron = *alphaTestResult;
                }
                else {
                    // If the alpha test is inconclusive (which may happen if the element is a sliver tetrahedron),
                    // then we check the surrounding tetrahedra. Only if all four neighbors are classified as filled or inconclusive,
                    // then we accept the sliver tetradron as filled too.
                    int f = 0;
                    for(; f < 4; f++) {
                        DelaunayTessellation::CellHandle adjacentCell = _tessellation.mirrorFacet(cell, f).first;
                        if(!_tessellation.isFiniteCell(adjacentCell))
                            break;
                        auto adjacentAlphaTestResult = _tessellation.alphaTest(adjacentCell, _alpha);
                        if(adjacentAlphaTestResult && !*adjacentAlphaTestResult)
                            break;
                    }
                    if(f == 4)
                        isFilledTetrehedron = true;
                }
            }

            SurfaceMesh::region_index region = SurfaceMesh::InvalidIndex;
            if(isFilledTetrehedron) {
                region = determineCellRegion(cell);
                OVITO_ASSERT(region >= 0 || region == SurfaceMesh::InvalidIndex);
                if(_createRegions) {
                    OVITO_ASSERT(_mesh.regionCount() != 0 || (region == 0 || region == SurfaceMesh::InvalidIndex));
                    OVITO_ASSERT(_mesh.regionCount() == 0 || region < _mesh.regionCount());
                }
                else {
                    OVITO_ASSERT(region < _mesh.regionCount() || region == SurfaceMesh::InvalidIndex);
                }
            }
            _tessellation.setUserField(cell, region);

            if(!_tessellation.isGhostCell(cell)) {
                if(spaceFillingRegionUndetermined) {
                    _mesh.setSpaceFillingRegion(region);
                    spaceFillingRegionUndetermined = false;
                }
                else {
                    if(isSpaceFilling && _mesh.spaceFillingRegion() != region) {
                        _mesh.setSpaceFillingRegion(SurfaceMesh::InvalidIndex);
                        isSpaceFilling = false;
                    }
                }
            }

            if(region != SurfaceMesh::InvalidIndex && !_tessellation.isGhostCell(cell)) {
                _tessellation.setCellIndex(cell, _numFilledCells++);
            }
            else {
                _tessellation.setCellIndex(cell, -1);
            }
        }
    }

    /// Computes the volume of a Delaunay tetrahedron.
    FloatType cellVolume(DelaunayTessellation::CellHandle cell) const
    {
        Point3 p0 = _tessellation.vertexPosition(_tessellation.cellVertex(cell, 0));
        Vector3 ad = _tessellation.vertexPosition(_tessellation.cellVertex(cell, 1)) - p0;
        Vector3 bd = _tessellation.vertexPosition(_tessellation.cellVertex(cell, 2)) - p0;
        Vector3 cd = _tessellation.vertexPosition(_tessellation.cellVertex(cell, 3)) - p0;
        return std::abs(ad.dot(cd.cross(bd))) / FloatType(6);
    }

    /// Aggregates Delaunay tetrahedra into connected regions.
    void formFilledRegions()
    {
        this_task::beginProgressSubStepsWithWeights({2,3,1});

        // Create a lookup map that allows retrieving the primary image of a Delaunay cell for a triangular face formed by three particles.
        createCellMap();
        this_task::throwIfCanceled();

        this_task::nextProgressSubStep();
        this_task::setProgressMaximum(_tessellation.numberOfTetrahedra());
        size_t progressValue = 0;

        // Identify connected tetrahedra to create regions if no regions were previously defined.
        if(_mesh.regionCount() == 0) {
            // Create the 'Volume' property for the identified regions.
            BufferWriteAccessAndRef<FloatType, access_mode::discard_write> regionVolumes = SurfaceMeshRegions::OOClass().createStandardProperty(DataBuffer::Uninitialized, 0, SurfaceMeshRegions::VolumeProperty);

            // Loop over all Delaunay cells to cluster them into connected components.
            // All filled cells have initially a user field value of 0.
            std::deque<size_t> toProcess;
            for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
                DelaunayTessellation::CellHandle cell = *cellIter;

                // Skip empty cells and cells that have already been assigned to a region.
                if(_tessellation.getUserField(cell) != 0)
                    continue;

                // Skip ghost cells.
                if(_tessellation.isGhostCell(cell))
                    continue;

                // Start a new region.
                int currentRegionId = regionVolumes.size() + 1;
                OVITO_ASSERT(currentRegionId >= 1);
                _tessellation.setUserField(cell, currentRegionId);
                // Compute total region volume as we go while visiting all tetrahedra.
                double regionVolume = 0;

                // Now recursively iterate over all neighbors of the seed cell and add them to the current region.
                toProcess.push_back(cell);
                do {
                    DelaunayTessellation::CellHandle currentCell = toProcess.front();
                    toProcess.pop_front();
                    this_task::setProgressValueIntermittent(progressValue++);

                    // Add the volume of the current cell to the total region volume.
                    regionVolume += cellVolume(currentCell);

                    // Loop over the 4 facets of the cell
                    for(int f = 0; f < 4; f++) {

                        // Get the 3 vertices of the facet.
                        // Note that we reverse their order to find the opposite face.
                        std::array<size_t, 3> vertices;
                        for(int v = 0; v < 3; v++)
                            vertices[v] = _tessellation.vertexIndex(_tessellation.cellVertex(currentCell, DelaunayTessellation::cellFacetVertexIndex(f, 2-v)));

                        // Bring vertices into a well-defined order, which can be used as lookup key to find the adjacent tetrahedron.
                        reorderFaceVertices(vertices);

                        // Look up the adjacent Delaunay cell.
                        if(auto neighborCell = _cellLookupMap.find(vertices); neighborCell != _cellLookupMap.end()) {
                            // Add adjacent cell to the deque if it has not been visited yet.
                            if(_tessellation.getUserField(neighborCell->second) == 0) {
                                toProcess.push_back(neighborCell->second);
                                _tessellation.setUserField(neighborCell->second, currentRegionId);
                            }
                        }
                    }
                }
                while(!toProcess.empty());

                // Create a spatial region in the output mesh.
                SurfaceMesh::region_index regionId = regionVolumes.size();
                regionVolumes.push_back(regionVolume);
                OVITO_ASSERT(regionId + 1 == currentRegionId);
            }

            // Put region property into container.
            size_t nreg = regionVolumes.size();
            _mesh.mutableRegions()->setContent(nreg, { static_object_cast<Property>(regionVolumes.take()) });

            if(_mesh.regionCount() > 0) {
                // Shift filled region IDs to start at index 0.
                for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
                    int region = _tessellation.getUserField(*cellIter);
                    if(region > 0)
                        _tessellation.setUserField(*cellIter, region - 1);
                }
            }
        }
        else {
            // Create the 'Volume' property for the identified regions.
            BufferWriteAccess<FloatType, access_mode::read_write> regionVolumes = _mesh.createRegionProperty(DataBuffer::Initialized, SurfaceMeshRegions::VolumeProperty);

            // Filled mesh regions have already been predefined by the caller.
            // We just need to compute the volume of each spatial region.
            for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
                this_task::throwIfCanceled();
                DelaunayTessellation::CellHandle cell = *cellIter;

                // Skip empty cells.
                SurfaceMesh::region_index region = _tessellation.getUserField(cell);
                if(region == SurfaceMesh::InvalidIndex)
                    continue;

                // Skip ghost cells.
                if(_tessellation.isGhostCell(cell))
                    continue;

                regionVolumes[region] += cellVolume(cell);
            }
        }

        this_task::nextProgressSubStep();

        _filledRegionCount = _mesh.regionCount();
        if(_filledRegionCount != 0) {
            // Copy assigned region IDs from primary tetrahedra to ghost tetrahedra.
            this_task::setProgressMaximum(_tessellation.numberOfTetrahedra());
            for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
                DelaunayTessellation::CellHandle cell = *cellIter;
                if(_tessellation.isGhostCell(cell) && _tessellation.getUserField(cell) != SurfaceMesh::InvalidIndex) {
                    this_task::setProgressValueIntermittent(cell);

                    // Get the 3 vertices of the first face of the tet.
                    std::array<size_t, 3> vertices;
                    for(int v = 0; v < 3; v++)
                        vertices[v] = _tessellation.vertexIndex(_tessellation.cellVertex(cell, DelaunayTessellation::cellFacetVertexIndex(0, v)));

                    // Bring vertices into a well-defined order, which can be used as lookup key.
                    reorderFaceVertices(vertices);

                    // Find the primary tet whose face connects the same three particles.
                    if(auto neighborCell = _cellLookupMap.find(vertices); neighborCell != _cellLookupMap.end()) {
                        _tessellation.setUserField(cell, _tessellation.getUserField(neighborCell->second));
                    }
                }
            }
        }
        this_task::endProgressSubSteps();
    }

    /// Creates a lookup map that allows to retrieve the primary Delaunay cell image that belongs to a
    /// triangular face formed by three particles.
    void createCellMap()
    {
        this_task::setProgressMaximum(_tessellation.numberOfTetrahedra());
        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;

            // Skip cells that belong to the exterior region.
            if(_tessellation.getUserField(cell) == SurfaceMesh::InvalidIndex)
                continue;

            // Skip ghost cells.
            if(_tessellation.isGhostCell(cell))
                continue;

            // Update progress indicator.
            this_task::setProgressValueIntermittent(cell);

            // Loop over the 4 facets of the cell.
            for(int f = 0; f < 4; f++) {

                // Get the 3 vertices of the facet.
                std::array<size_t, 3> vertices;
                for(int v = 0; v < 3; v++)
                    vertices[v] = _tessellation.vertexIndex(_tessellation.cellVertex(cell, DelaunayTessellation::cellFacetVertexIndex(f, v)));

                // Bring vertices into a well-defined order, which can be used as lookup key.
                reorderFaceVertices(vertices);

                OVITO_ASSERT(_cellLookupMap.find(vertices) == _cellLookupMap.end());

                // Add facet and its adjacent cell to the loopup map.
                _cellLookupMap.emplace(vertices, cell);
            }
        }
    }

    /// Constructs the triangle facets that separate different regions in the tetrahedral mesh.
    template<typename PrepareMeshFaceFunc, typename PrepareMeshVertexFunc>
    void createInterfaceFacets(PrepareMeshFaceFunc&& prepareMeshFaceFunc, PrepareMeshVertexFunc&& prepareMeshVertexFunc)
    {
        // Stores the triangle mesh vertices created for the vertices of the tetrahedral mesh.
        std::vector<SurfaceMesh::vertex_index> vertexMap(_positions.size(), SurfaceMesh::InvalidIndex);
        _tetrahedraFaceList.clear();
        _faceLookupMap.clear();

        // Create the vertex coordinates array, which will dynamically grow.
        BufferWriteAccessAndRef<Point3, access_mode::discard_write> vertexPositions = SurfaceMeshVertices::OOClass().createStandardProperty(DataBuffer::Uninitialized, 0, SurfaceMeshVertices::PositionProperty);

        // Create the per-face region array, which will dynamically grow.
        BufferWriteAccessAndRef<SurfaceMesh::region_index, access_mode::discard_write> faceRegions = SurfaceMeshFaces::OOClass().createStandardProperty(DataBuffer::Uninitialized, 0, SurfaceMeshFaces::RegionProperty);

        this_task::setProgressMaximum(_numFilledCells);
        SurfaceMeshTopology* topo = _mesh.mutableTopology();

        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;

            // Consider only filled local tetrahedra.
            if(_tessellation.getCellIndex(cell) == -1)
                continue;
            SurfaceMesh::region_index filledRegion = _tessellation.getUserField(cell);
            OVITO_ASSERT(filledRegion != SurfaceMesh::InvalidIndex);

            // Update progress indicator.
            this_task::setProgressValueIntermittent(_tessellation.getCellIndex(cell));

            Point3 unwrappedVerts[4];
            for(int i = 0; i < 4; i++)
                unwrappedVerts[i] = _tessellation.vertexPosition(_tessellation.cellVertex(cell, i));

            // Check validity of tessellation.
            // Delaunay edges (of filled tetrahedro) should never span more than half of the simulation box in periodic directions.
            Vector3 ad = unwrappedVerts[0] - unwrappedVerts[3];
            Vector3 bd = unwrappedVerts[1] - unwrappedVerts[3];
            Vector3 cd = unwrappedVerts[2] - unwrappedVerts[3];
            if(_tessellation.simCell()->isWrappedVector(ad) || _tessellation.simCell()->isWrappedVector(bd) || _tessellation.simCell()->isWrappedVector(cd))
                throw Exception("Cannot construct manifold. Simulation cell length is too small for the given probe sphere radius parameter.");

            // Iterate over the four faces of the tetrahedron cell.
            _tessellation.setCellIndex(cell, -1);
            for(int f = 0; f < 4; f++) {

                // Check if the adjacent tetrahedron belongs to a different region.
                std::pair<DelaunayTessellation::CellHandle,int> mirrorFacet = _tessellation.mirrorFacet(cell, f);
                DelaunayTessellation::CellHandle adjacentCell = mirrorFacet.first;
                if(_tessellation.getUserField(adjacentCell) == filledRegion)
                    continue;

                // Create the three vertices of the face or use existing output vertices.
                std::array<SurfaceMesh::vertex_index,3> facetVertices;
                std::array<DelaunayTessellation::VertexHandle,3> vertexHandles;
                std::array<size_t,3> vertexIndices;
                for(int v = 0; v < 3; v++) {
                    vertexHandles[v] = _tessellation.cellVertex(cell, DelaunayTessellation::cellFacetVertexIndex(f, _flipOrientation ? v : (2-v)));
                    size_t vertexIndex = vertexIndices[v] = _tessellation.vertexIndex(vertexHandles[v]);
                    OVITO_ASSERT(vertexIndex < vertexMap.size());
                    if(vertexMap[vertexIndex] == SurfaceMesh::InvalidIndex) {
                        vertexMap[vertexIndex] = topo->createVertex();
                        vertexPositions.push_back(_positions[vertexIndex]);
                        prepareMeshVertexFunc(vertexMap[vertexIndex], vertexIndex);
                    }
                    facetVertices[v] = vertexMap[vertexIndex];
                }

                // Create a new triangle facet.
                SurfaceMesh::face_index face = topo->createFaceAndEdges(facetVertices.begin(), facetVertices.end());
                faceRegions.push_back(filledRegion);

                // Tell client code about the new facet.
                prepareMeshFaceFunc(face, vertexIndices, vertexHandles, cell);

                // Create additional face for exterior region if requested.
                if(_createRegions && _tessellation.getUserField(adjacentCell) == SurfaceMesh::InvalidIndex) {

                    // Build face vertex list.
                    std::array<size_t,3> reverseVertexIndices;
                    for(int v = 0; v < 3; v++) {
                        vertexHandles[v] = _tessellation.cellVertex(adjacentCell, DelaunayTessellation::cellFacetVertexIndex(mirrorFacet.second, _flipOrientation ? v : (2-v)));
                        size_t vertexIndex = reverseVertexIndices[v] = _tessellation.vertexIndex(vertexHandles[v]);
                        OVITO_ASSERT(vertexIndex < vertexMap.size());
                        OVITO_ASSERT(vertexMap[vertexIndex] != SurfaceMesh::InvalidIndex);
                        facetVertices[v] = vertexMap[vertexIndex];
                    }

                    // Create a new triangle facet.
                    SurfaceMesh::face_index oppositeFace = topo->createFaceAndEdges(facetVertices);
                    faceRegions.push_back(SurfaceMesh::InvalidIndex);

                    // Tell client code about the new facet.
                    prepareMeshFaceFunc(oppositeFace, reverseVertexIndices, vertexHandles, adjacentCell);

                    // Insert new facet into lookup map.
                    reorderFaceVertices(reverseVertexIndices);
                    _faceLookupMap.emplace(reverseVertexIndices, oppositeFace);
                }

                // Insert new facet into lookup map.
                reorderFaceVertices(vertexIndices);
                _faceLookupMap.emplace(vertexIndices, face);

                // Insert into contiguous list of tetrahedron faces.
                if(_tessellation.getCellIndex(cell) == -1) {
                    _tessellation.setCellIndex(cell, _tetrahedraFaceList.size());
                    _tetrahedraFaceList.push_back(std::array<SurfaceMesh::face_index, 4>{{ SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex }});
                }
                _tetrahedraFaceList[_tessellation.getCellIndex(cell)][f] = face;
            }
        }

        // Store the vertex coordinates in the mesh.
        _mesh.mutableVertices()->setContent(topo->vertexCount(), { static_object_cast<Property>(vertexPositions.take()) });

        // Store the per-face region information in the mesh.
        _mesh.mutableFaces()->setContent(topo->faceCount(), { static_object_cast<Property>(faceRegions.take()) });
    }

    SurfaceMesh::face_index findAdjacentFace(DelaunayTessellation::CellHandle cell, int f, int e, bool reverse = false)
    {
        int vertexIndex1, vertexIndex2;
        if(!_flipOrientation) {
            vertexIndex1 = DelaunayTessellation::cellFacetVertexIndex(f, 2-e);
            vertexIndex2 = DelaunayTessellation::cellFacetVertexIndex(f, (4-e)%3);
        }
        else {
            vertexIndex1 = DelaunayTessellation::cellFacetVertexIndex(f, (e+1)%3);
            vertexIndex2 = DelaunayTessellation::cellFacetVertexIndex(f, e);
        }
        DelaunayTessellation::FacetCirculator circulator_start = _tessellation.incident_facets(cell, vertexIndex1, vertexIndex2, cell, f);
        DelaunayTessellation::FacetCirculator circulator = circulator_start;
        OVITO_ASSERT((*circulator).first == cell);
        OVITO_ASSERT((*circulator).second == f);
        int region = _tessellation.getUserField(cell);
        if(!reverse) {
            --circulator;
            OVITO_ASSERT(circulator != circulator_start);
            do {
                // Look for the first cell while going around the edge that belongs to a different region.
                if(_tessellation.getUserField((*circulator).first) != region)
                    break;
                --circulator;
            }
            while(circulator != circulator_start);
            OVITO_ASSERT(circulator != circulator_start);
        }
        else {
            ++circulator;
            OVITO_ASSERT(circulator != circulator_start);
            for(;;) {
                // Look for the first cell while going around the edge in reverse direction that belongs to the same region.
                if(_tessellation.getUserField((*circulator).first) == region)
                    break;
                ++circulator;
            }
            --circulator;
        }

        // Get the current adjacent cell, which is part of the same region as the first tet.
        std::pair<DelaunayTessellation::CellHandle,int> mirrorFacet = _tessellation.mirrorFacet(*circulator);
        OVITO_ASSERT(_tessellation.getUserField(mirrorFacet.first) == region);

        SurfaceMesh::face_index adjacentFace = findCellFace(mirrorFacet);
        OVITO_ASSERT(adjacentFace != SurfaceMesh::InvalidIndex);
        if(adjacentFace == SurfaceMesh::InvalidIndex)
            throw Exception("Cannot construct mesh for this input dataset. Adjacent cell face not found.");
        return adjacentFace;
    }

    void linkHalfedges()
    {
        this_task::setProgressMaximum(_tetrahedraFaceList.size());

#ifdef OVITO_DEBUG
        BufferReadAccess<SurfaceMesh::region_index> faceRegions(_mesh.expectFaceProperty(SurfaceMeshFaces::RegionProperty));
#endif

        auto tet = _tetrahedraFaceList.cbegin();
        for(DelaunayTessellation::CellIterator cellIter = _tessellation.begin_cells(); cellIter != _tessellation.end_cells(); ++cellIter) {
            DelaunayTessellation::CellHandle cell = *cellIter;

            // Look for filled cells being adjacent to at least one mesh face.
            if(_tessellation.getCellIndex(cell) == -1) continue;
            OVITO_ASSERT(_tetrahedraFaceList.cbegin() + _tessellation.getCellIndex(cell) == tet);

            // Update progress indicator.
            this_task::setProgressValueIntermittent(_tessellation.getCellIndex(cell));

            // Visit the mesh faces adjacent to the current cell.
            for(int f = 0; f < 4; f++) {
                SurfaceMesh::face_index facet = (*tet)[f];
                if(facet == SurfaceMesh::InvalidIndex) continue;

                // Link within manifold.
                SurfaceMesh::edge_index edge = _mesh.firstFaceEdge(facet);
                for(int e = 0; e < 3; e++, edge = _mesh.nextFaceEdge(edge)) {
                    if(_mesh.hasOppositeEdge(edge)) continue;
                    SurfaceMesh::face_index adjacentFace = findAdjacentFace(cell, f, e);
                    SurfaceMesh::edge_index oppositeEdge = _mesh.findEdge(adjacentFace, _mesh.vertex2(edge), _mesh.vertex1(edge));
                    if(oppositeEdge == SurfaceMesh::InvalidIndex)
                        throw Exception("Cannot construct mesh for this input dataset. Opposite half-edge not found.");
                    _mesh.linkOppositeEdges(edge, oppositeEdge);
                }

                if(_createRegions) {
                    std::pair<DelaunayTessellation::CellHandle,int> oppositeFacet = _tessellation.mirrorFacet(cell, f);
                    OVITO_ASSERT(_tessellation.getUserField(oppositeFacet.first) != _tessellation.getUserField(cell));
                    SurfaceMesh::face_index outerFacet = findCellFace(oppositeFacet);
                    OVITO_ASSERT(outerFacet != SurfaceMesh::InvalidIndex);

                    // Link opposite mesh faces (Note: they may have already been linked).
                    _mesh.linkOppositeFaces(facet, outerFacet);

                    // Link adjacent facets in opposite manifold if it is bounding an empty region.
                    if(_tessellation.getUserField(oppositeFacet.first) == SurfaceMesh::InvalidIndex) {
                        OVITO_ASSERT(faceRegions[outerFacet] == SurfaceMesh::InvalidIndex);
                        SurfaceMesh::edge_index edge = _mesh.firstFaceEdge(outerFacet);
                        for(int e = 0; e < 3; e++, edge = _mesh.nextFaceEdge(edge)) {
                            if(_mesh.hasOppositeEdge(edge)) continue;
                            SurfaceMesh::face_index adjacentFace = findAdjacentFace(oppositeFacet.first, oppositeFacet.second, e, true);
                            OVITO_ASSERT(faceRegions[adjacentFace] == SurfaceMesh::InvalidIndex);
                            SurfaceMesh::edge_index oppositeEdge = _mesh.findEdge(adjacentFace, _mesh.vertex2(edge), _mesh.vertex1(edge));
                            if(oppositeEdge == SurfaceMesh::InvalidIndex)
                                throw Exception("Cannot construct mesh for this input dataset. Opposite half-edge (2) not found.");
                            _mesh.linkOppositeEdges(edge, oppositeEdge);
                        }
                    }
                }
            }
            ++tet;
        }
        OVITO_ASSERT(tet == _tetrahedraFaceList.cend());
        OVITO_ASSERT(_mesh.topology()->isClosed());

        // Set up manifold pointers at edges of the mesh.
        if(_createRegions) {
            for(SurfaceMesh::edge_index edge1 = 0; edge1 < _mesh.edgeCount(); edge1++) {
                this_task::throwIfCanceled();
                // Link surface manifolds.
                SurfaceMesh::edge_index oppositeEdge = _mesh.oppositeEdge(edge1);
                SurfaceMesh::face_index adjacentFace = _mesh.adjacentFace(oppositeEdge);
                SurfaceMesh::face_index oppositeFace = _mesh.oppositeFace(adjacentFace);
                for(SurfaceMesh::edge_index edge2 = _mesh.firstFaceEdge(oppositeFace); ; edge2 = _mesh.nextFaceEdge(edge2)) {
                    if(_mesh.vertex2(edge2) == _mesh.vertex2(edge1)) {
                        _mesh.setNextManifoldEdge(edge1, edge2);
                        break;
                    }
                }
            }
        }
    }

    SurfaceMesh::face_index findCellFace(const std::pair<DelaunayTessellation::CellHandle,int>& facet)
    {
        // If the cell is a ghost cell, find the corresponding real cell first.
        auto cell = facet.first;
        if(_tessellation.getCellIndex(cell) != -1) {
            OVITO_ASSERT(_tessellation.getCellIndex(cell) >= 0 && _tessellation.getCellIndex(cell) < (qint64)_tetrahedraFaceList.size());
            return _tetrahedraFaceList[_tessellation.getCellIndex(cell)][facet.second];
        }
        else {
            std::array<size_t,3> faceVerts;
            for(size_t i = 0; i < 3; i++) {
                int vertexIndex = DelaunayTessellation::cellFacetVertexIndex(facet.second, _flipOrientation ? i : (2-i));
                faceVerts[i] = _tessellation.vertexIndex(_tessellation.cellVertex(cell, vertexIndex));
            }
            reorderFaceVertices(faceVerts);
            if(auto item = _faceLookupMap.find(faceVerts); item != _faceLookupMap.end())
                return item->second;
            else
                return SurfaceMesh::InvalidIndex;
        }
    }

    static void reorderFaceVertices(std::array<size_t,3>& vertexIndices) {
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WASM)
        // Shift the order of vertices so that the smallest index is at the front.
        std::rotate(std::begin(vertexIndices), std::min_element(std::begin(vertexIndices), std::end(vertexIndices)), std::end(vertexIndices));
#else
        // Workaround for compiler bug in Xcode 10.0. Clang hangs when compiling the code above with -O2/-O3 flag.
        auto min_index = std::min_element(vertexIndices.begin(), vertexIndices.end()) - vertexIndices.begin();
        std::rotate(vertexIndices.begin(), vertexIndices.begin() + min_index, vertexIndices.end());
#endif
    }

    /// Computes the volume of the given Delaunay cell that is (partially) overlapping with the simulation box.
    FloatType calculateVolumeOverlap(DelaunayTessellation::CellHandle cell, std::array<bool,3>& outsideDir)
    {
        // Gather the positions of the four Delaunay vertices and check if they are all inside the simulation cell domain.
        bool isCompletelyInsideBox = true;
        Point3 vertexPositions[4];
        Point3 reducedVertexPositions[4];
        for(int v = 0; v < 4; v++) {
            const Point3& vpos = vertexPositions[v] = _tessellation.vertexPosition(_tessellation.cellVertex(cell, v));
            const Point3& rp = reducedVertexPositions[v] = _tessellation.simCell()->absoluteToReduced(vpos);
            if(rp.x() < 0.0 || rp.x() > 1.0) {
                isCompletelyInsideBox = false;
                outsideDir[0] = true;
            }
            if(rp.y() < 0.0 || rp.y() > 1.0) {
                isCompletelyInsideBox = false;
                outsideDir[1] = true;
            }
            if(rp.z() < 0.0 || rp.z() > 1.0) {
                isCompletelyInsideBox = false;
                outsideDir[2] = true;
            }
        }

        // Compute volume of the full tetrahedron if it is completely inside the simulation box.
        if(isCompletelyInsideBox) {
            Vector3 ad = vertexPositions[1] - vertexPositions[0];
            Vector3 bd = vertexPositions[2] - vertexPositions[0];
            Vector3 cd = vertexPositions[3] - vertexPositions[0];
            return std::abs(ad.dot(cd.cross(bd))) / FloatType(6);
        }

        // There may be a partial overlap. We need to compute the intersection of the two shapes to determine the overlap volume.

        // We start with six line segments along the six edges of the tetrahedron.
        // The six line segments will be clipped at the boundaries of the simulation box.
        Point3 lineSegments[(6 + 12) * 2];
        static const int edgeVertices[6][2] = {{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}};
        int numPoints = 0;
        for(int e = 0; e < 6; e++) {
            Point3 p1 = reducedVertexPositions[edgeVertices[e][0]];
            Point3 p2 = reducedVertexPositions[edgeVertices[e][1]];

            // Clip the segment at the boundaries of the simulation box.
            bool isDegenerate = false;
            for(size_t dim = 0; dim < 3; dim++) {
                // Clip at lower box boundary.
                if(p1[dim] <= 0 && p2[dim] <= 0) {
                    isDegenerate = true;
                    break;
                }
                else if(p1[dim] < 0 && p2[dim] >= 0) {
                    Vector3 delta = p2 - p1;
                    p1 -= delta * (p1[dim] / delta[dim]);
                    OVITO_ASSERT(std::abs(p1[dim]) < FLOATTYPE_EPSILON);
                }
                else if(p1[dim] >= 0 && p2[dim] < 0) {
                    Vector3 delta = p2 - p1;
                    p2 -= delta * (p2[dim] / delta[dim]);
                    OVITO_ASSERT(std::abs(p2[dim]) < FLOATTYPE_EPSILON);
                }

                // Clip at upper box boundary.
                if(p1[dim] >= 1.0 && p2[dim] >= 1.0) {
                    isDegenerate = true;
                    break;
                }
                else if(p1[dim] > 1.0 && p2[dim] <= 1.0) {
                    Vector3 delta = p2 - p1;
                    p1 -= delta * ((p1[dim] - 1.0) / delta[dim]);
                    OVITO_ASSERT(std::abs(p1[dim] - 1.0) < FLOATTYPE_EPSILON);
                }
                else if(p1[dim] <= 1.0 && p2[dim] > 1.0) {
                    Vector3 delta = p2 - p1;
                    p2 -= delta * ((p2[dim] - 1.0) / delta[dim]);
                    OVITO_ASSERT(std::abs(p2[dim] - 1.0) < FLOATTYPE_EPSILON);
                }
            }

            // Add the line segment to the list of output segments if it wasn't clipped away entirely.
            if(!isDegenerate) {
                lineSegments[numPoints++] = p1;
                lineSegments[numPoints++] = p2;
            }
        }

        // Compute the four planes of the tetrahedron faces.
        Plane3 planes[4];
        for(int f = 0; f < 4; f++) {
            const Point3& p0 = reducedVertexPositions[DelaunayTessellation::cellFacetVertexIndex(f, 0)];
            const Point3& p1 = reducedVertexPositions[DelaunayTessellation::cellFacetVertexIndex(f, 1)];
            const Point3& p2 = reducedVertexPositions[DelaunayTessellation::cellFacetVertexIndex(f, 2)];
            planes[f] = Plane3(p0, p1, p2, true);
        }

        // Clip the 12 edges of the unit box at the 4 four planes of the tetrahedron.
        static const Point3 boxVertices[12][2] = {
            {Point3(0,0,0),Point3(1,0,0)}, {Point3(0,1,0),Point3(1,1,0)}, {Point3(0,0,1),Point3(1,0,1)}, {Point3(0,1,1),Point3(1,1,1)},
            {Point3(0,0,0),Point3(0,1,0)}, {Point3(1,0,0),Point3(1,1,0)}, {Point3(0,0,1),Point3(0,1,1)}, {Point3(1,0,1),Point3(1,1,1)},
            {Point3(0,0,0),Point3(0,0,1)}, {Point3(1,0,0),Point3(1,0,1)}, {Point3(0,1,0),Point3(0,1,1)}, {Point3(1,1,0),Point3(1,1,1)}
        };
        for(int e = 0; e < 12; e++) {
            Point3 p1 = boxVertices[e][0];
            Point3 p2 = boxVertices[e][1];

            // Clip at each plane.
            bool isDegenerate = false;
            for(const Plane3& plane : planes) {
                FloatType t1 = plane.pointDistance(p1);
                FloatType t2 = plane.pointDistance(p2);
                if(t1 < 0 && t2 < 0) {
                    isDegenerate = true;
                    break;
                }
                else if(t1 < 0 && t2 > 0) {
                    Vector3 delta = p2 - p1;
                    p1 -= delta * (t1 / (t2 - t1));
                    OVITO_ASSERT(std::abs(plane.pointDistance(p1)) <= FLOATTYPE_EPSILON);
                }
                else if(t1 > 0 && t2 < 0) {
                    Vector3 delta = p2 - p1;
                    p2 += delta * (t2 / (t1 - t2));
                    OVITO_ASSERT(std::abs(plane.pointDistance(p2)) <= FLOATTYPE_EPSILON);
                }
            }

            // Add the line segment to the list of output segments if it wasn't clipped away entirely.
            if(!isDegenerate) {
                lineSegments[numPoints++] = p1;
                lineSegments[numPoints++] = p2;
            }
        }
        if(numPoints < 4)
            return 0;   // Overlap is degenerate.

        // Create and re-use SurfaceMesh to avoid too many object reallocations.
        if(!_convexHullMesh)
            _convexHullMesh.reset(DataOORef<SurfaceMesh>::create(ObjectInitializationFlag::DontCreateVisElement));

        // Construct convex hull of remaining line segments.
        SurfaceMeshBuilder meshBuilder(_convexHullMesh);
        meshBuilder.clearMesh();
        meshBuilder.constructConvexHull(std::vector<Point3>(lineSegments, lineSegments + numPoints), SurfaceMesh::InvalidIndex);
#ifdef OVITO_DEBUG
        _convexHullMesh->verifyMeshIntegrity();
#endif

        // The convex hull may be empty if the input point set is degenerate.
        if(meshBuilder.faceCount() == 0)
            return 0;

        // Compute volume enclosed by the convex hull polyhedron.
        BufferReadAccess<Point3> vertexCoordinates(meshBuilder.expectVertexProperty(SurfaceMeshVertices::PositionProperty));
        const Point3 apex = vertexCoordinates[0];
        FloatType convexVolume = 0;
        for(SurfaceMesh::edge_index firstEdge : meshBuilder.firstFaceEdges()) {
            SurfaceMesh::edge_index edge1 = meshBuilder.nextFaceEdge(firstEdge);
            SurfaceMesh::edge_index edge2 = meshBuilder.nextFaceEdge(edge1);
            Matrix3 tripod;
            tripod.column(0) = vertexCoordinates[meshBuilder.vertex1(firstEdge)] - apex;
            tripod.column(2) = vertexCoordinates[meshBuilder.vertex2(firstEdge)] - apex;
            while(edge2 != firstEdge) {
                tripod.column(1) = tripod.column(2);
                tripod.column(2) = vertexCoordinates[meshBuilder.vertex2(edge1)] - apex;
                convexVolume += tripod.determinant();
                edge1 = edge2;
                edge2 = meshBuilder.nextFaceEdge(edge2);
            }
        }

        return (convexVolume / 6.0) * _tessellation.simCell()->volume3D();
    }

private:

    /// The tetrahedral tessellation.
    DelaunayTessellation& _tessellation;

    /// The squared probe sphere radius used to classify tetrahedra as open or solid.
    FloatType _alpha;

    /// Controls the grouping of Delaunay cells into volumetric regions and the generation
    /// of a two-sided surface mesh.
    bool _createRegions;

    /// Counts the number of tetrehedral cells that belong to the filled regions.
    size_t _numFilledCells = 0;

    /// Number of filled regions that have been identified.
    SurfaceMesh::size_type _filledRegionCount = 0;

    /// Number of empty regions that have been identified.
    SurfaceMesh::size_type _emptyRegionCount = 0;

    /// The input particle positions.
    BufferReadAccess<Point3> _positions;

    /// The output surface mesh.
    SurfaceMeshBuilder& _mesh;

    /// Controls the reversal of the normal orientation of the generated surface facets.
    bool _flipOrientation = false;

    /// Stores the faces of the local tetrahedra that have a least one facet for which a triangle has been created.
    std::vector<std::array<SurfaceMesh::face_index, 4>> _tetrahedraFaceList;

    /// This map allows looking up surface mesh faces based on their three vertices.
    std::map<std::array<size_t,3>, SurfaceMesh::face_index> _faceLookupMap;

    /// This map allows looking up the tetrahedron that is adjacent to a given triangular face.
    std::map<std::array<size_t,3>, DelaunayTessellation::CellHandle> _cellLookupMap;

    /// Working data structure used in calculateVolumeOverlap() for computing the volume of a truncated tetrahedral cell.
    DataOORef<SurfaceMesh> _convexHullMesh;
};

}   // End of namespace
