////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/mesh/surface/SurfaceMeshAccess.h>
#include <ovito/core/utilities/concurrent/ProgressingTask.h>

namespace Ovito::Grid {

/**
 * The Marching Cubes algorithm for constructing isosurfaces from grid data.
 */
class OVITO_GRID_EXPORT MarchingCubes
{
public:
    // Constructor
    MarchingCubes(SurfaceMeshAccess& outputMesh, int size_x, int size_y, int size_z, bool lowerIsSolid,
                  std::function<FloatType(int i, int j, int k)> field, bool infiniteDomain = false,
                  bool outputCellCoordinates = false, bool identifyRegions = false);

    /// The main algorithm routine.
    bool generateIsosurface(FloatType iso, ProgressingTask& operation);

    /// Returns the generated surface mesh.
    const SurfaceMeshAccess& mesh() const { return _outputMesh; }

    /// Returns the array indicating for each generated mesh face which voxel grid cell it is located in.
    const std::vector<std::tuple<int, int, int>>& meshFaceVoxelCoordinates() const
    {
        return std::move(_meshFaceVoxelCoordinates);
    }

    /// Returns the array indicating for each generated mesh face which voxel grid cell it is located in.
    std::vector<std::tuple<int, int, int>>&& takeMeshFaceVoxelCoordinates() { return std::move(_meshFaceVoxelCoordinates); }

private:
    /// Tessellates one cube.
    void processCube(int i, int j, int k);

    // Processes a single case from teh marching cubes table
    void processCase(int i, int j, int k, const signed char* triangles, const signed char* triangleRegions,
                     const signed char* vertexRegions, const signed char** volumeRegionsTriangulation, int numTriangles,
                     int numVolumeRegions, SurfaceMeshAccess::vertex_index v12 = SurfaceMeshAccess::InvalidIndex);

    /// Tests if the components of the tessellation of the cube should be
    /// connected by the interior of an ambiguous face.
    bool testFace(signed char face);

    /// Tests if the components of the tessellation of the cube should be
    /// connected through the interior of the cube.
    bool testInterior(signed char s);

    /// Computes almost all the vertices of the mesh by interpolation along the cubes edges.
    void computeIntersectionPoints(ProgressingTask& operation);

    /// Adds triangles to the mesh.
    void addTriangle(int i, int j, int k, const signed char* triangles, signed char numTriangles,
                     SurfaceMeshAccess::vertex_index v12 = SurfaceMeshAccess::InvalidIndex);
    // Adds triangles to the mesh and assigns them a local region index
    void addTriangle(int i, int j, int k, const signed char* triangles, const signed char* triangleRegions,
                     const std::array<int, 5>& localRegionMap, signed char numTriangles, SurfaceMeshAccess::vertex_index v12);

    // Calculates the volume per region inside a single voxel
    void addVolume(int i, int j, int k, const signed char** volumeRegions, const std::array<int, 5>& localRegionMap,
                   const int numVolumeRegions, SurfaceMeshAccess::vertex_index v12);

    // Merge connected regions of the generated iso surface
    void mergeIdentifiedRegions();

    // Handle case where the domain is fully filled
    void handleSpaceFillingRegion();

    // Converts the local to the global region index results are stored in localRegionMap
    std::array<int, 5> processRegionsVoxelVertices(int i, int j, int k, const signed char* vertexRegions);
    void processRegionsVoxelVertex(int i, int j, int k, signed char vertexRegion, std::array<int, 5>& localRegionMap);

    //   Converts the local (per voxel) edge indices to global vertices used in the mesh
    SurfaceMeshAccess::vertex_index localToGlobalEdgeVertex(int i, int j, int k, int edgeIndex,
                                                            SurfaceMeshAccess::vertex_index v12) const;
    //   Converts the local (per voxel) edge indices to global vertices used in the mesh and returns the axis
    Vector3 getTriangleEdgeVector(int i, int j, int k, int edgeIndex, SurfaceMeshAccess::vertex_index v12) const;

    // Adds a vertex on the current horizontal edge.
    SurfaceMeshAccess::vertex_index createEdgeVertexX(int i, int j, int k, FloatType u);

    // Adds a vertex on the current longitudinal edge.
    SurfaceMeshAccess::vertex_index createEdgeVertexY(int i, int j, int k, FloatType u);

    /// Adds a vertex on the current vertical edge.
    SurfaceMeshAccess::vertex_index createEdgeVertexZ(int i, int j, int k, FloatType u);

    /// Adds a vertex inside the current cube.
    SurfaceMeshAccess::vertex_index createCenterVertex(int i, int j, int k);

    /// Accesses the pre-computed vertex on a lower edge of a specific cube.
    SurfaceMeshAccess::vertex_index getEdgeVertex(int i, int j, int k, int axis) const;

    // Calculates the position of a specific voxel corner in the volume.
    Vector3 getCornerVertex(int i, int j, int k, int edgeIndex) const;

    // Gets the region for each voxel corner
    int getVertexRegion(int i, int j, int k) const;

    // Sets the region for each voxel corner
    void setVertexRegion(int i, int j, int k, int value);

private:
    const std::array<bool, 3> _pbcFlags;  ///< PBC flags
    int _size_x;                          ///< width  of the grid
    int _size_y;                          ///< depth  of the grid
    int _size_z;                          ///< height of the grid
    FloatType _isolevel;
    std::function<FloatType(int i, int j, int k)> getFieldValue;

    // Controls the identification of regions in the volumetric mesh.
    bool _identifyRegions;

    bool _lowerIsSolid;           ///< Controls the inward/outward orientation of the created triangle surface.
    bool _infiniteDomain;         ///< Controls whether the volumetric domain is infinite extended.
                                  ///< Setting this to true will result in an isosource that is not closed.
                                  ///< This option is used by the VoxelGridSliceModifierDelegate to construct the slice plane.
    bool _outputCellCoordinates;  ///< Controls whether the algorithm should keep track for each generated mesh face in which
                                  ///< voxel grid it is located.

    /// Vertices created along cube edges.
    std::vector<SurfaceMeshAccess::vertex_index> _cubeVerts;

    // Stores the region for each voxel corner
    std::vector<int> _vertRegions;

    // Stores the volumes for each region before merger
    std::vector<FloatType> _regionVolumes;

    // Stores the filled state for each region before merger
    std::vector<bool> _regionFilled;
    std::vector<bool> _regionExterior;

    // Regions to merge
    std::vector<std::tuple<int, int>> _regionsToMerge;

    // Current maximum region index
    int _maxRegionIndex{0};

    /// Stores for each generated mesh face which voxel grid cell it is located in.
    std::vector<std::tuple<int, int, int>> _meshFaceVoxelCoordinates;

    FloatType _cube[8];        ///< values of the implicit function on the active cube
    unsigned char _lut_entry;  ///< cube sign representation in [0..255]
    signed char _case;         ///< case of the active cube in [0..15]
    signed char _config;       ///< configuration of the active cube
    signed char _subconfig;    ///< subconfiguration of the active cube

    /// The generated surface mesh.
    SurfaceMeshAccess& _outputMesh;

#ifdef FLOATTYPE_FLOAT
    static constexpr FloatType _epsilon = FloatType(1e-12);
#else
    static constexpr FloatType _epsilon = FloatType(1e-18);
#endif
};

}  // namespace Ovito::Grid
