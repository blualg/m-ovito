////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/delaunay/ManifoldConstructionHelper.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/grid/modifier/MarchingCubes.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "ConstructSurfaceModifier.h"

#include <boost/range/numeric.hpp>

namespace Ovito::Particles {

using namespace Ovito::Delaunay;

IMPLEMENT_OVITO_CLASS(ConstructSurfaceModifier);
DEFINE_REFERENCE_FIELD(ConstructSurfaceModifier, surfaceMeshVis);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, smoothingLevel);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, probeSphereRadius);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, selectSurfaceParticles);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, transferParticleProperties);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, identifyRegions);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, method);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, gridResolution);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, radiusFactor);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, isoValue);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, computeSurfaceDistance);
DEFINE_PROPERTY_FIELD(ConstructSurfaceModifier, mapParticlesToRegions);
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, smoothingLevel, "Smoothing level");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, probeSphereRadius, "Probe sphere radius");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, onlySelectedParticles, "Use only selected input particles");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, selectSurfaceParticles, "Select particles on the surface");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, transferParticleProperties, "Transfer particle properties to surface");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, identifyRegions, "Identify volumetric regions (filled/void)");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, method, "Construction method");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, gridResolution, "Resolution");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, radiusFactor, "Radius scaling");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, isoValue, "Iso value");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, computeSurfaceDistance, "Compute particle distances from surface");
SET_PROPERTY_FIELD_LABEL(ConstructSurfaceModifier, mapParticlesToRegions, "Map particles to regions");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ConstructSurfaceModifier, probeSphereRadius, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ConstructSurfaceModifier, smoothingLevel, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(ConstructSurfaceModifier, gridResolution, IntegerParameterUnit, 2, 600);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ConstructSurfaceModifier, radiusFactor, PercentParameterUnit, 0);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ConstructSurfaceModifier::ConstructSurfaceModifier(ObjectCreationParams params) : AsynchronousModifier(params),
    _smoothingLevel(8),
    _probeSphereRadius(4),
    _onlySelectedParticles(false),
    _selectSurfaceParticles(false),
    _transferParticleProperties(false),
    _method(AlphaShape),
    _gridResolution(50),
    _radiusFactor(1.0),
    _isoValue(0.6),
    _identifyRegions(false),
    _computeSurfaceDistance(false),
    _mapParticlesToRegions(false)
{
    if(params.createSubObjects()) {
        // Create the vis element for rendering the surface generated by the modifier.
        setSurfaceMeshVis(OORef<SurfaceMeshVis>::create(params));
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ConstructSurfaceModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<ParticlesObject>();
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the
* modifier's results.
******************************************************************************/
Future<AsynchronousModifier::EnginePtr> ConstructSurfaceModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Get input particle positions.
    const ParticlesObject* particles = input.expectObject<ParticlesObject>();
    particles->verifyIntegrity();
    const PropertyObject* posProperty = particles->expectProperty(ParticlesObject::PositionProperty);

    // Get particle selection flags if requested.
    const PropertyObject* selProperty = onlySelectedParticles() ? particles->expectProperty(ParticlesObject::SelectionProperty) : nullptr;

    // Get particle "Grain" property.
    ConstPropertyPtr grainProperty = particles->getProperty(QStringLiteral("Grain"));
    if(grainProperty && grainProperty->componentCount() != 1)
        grainProperty.reset();
    if(grainProperty && grainProperty->dataType() != PropertyObject::Int64) {
        auto copy = DataOORef<PropertyObject>::makeCopy(grainProperty);
        copy->convertDataType(DataBuffer::Int64);
        grainProperty = std::move(copy);
    }

    // Get simulation cell.
    const SimulationCellObject* simCell = input.expectObject<SimulationCellObject>();
    if(simCell->is2D())
        throw Exception(tr("The construct surface mesh modifier does not support 2d simulation cells."));

    // Collect the set of particle properties that should be transferred over to the surface mesh vertices.
    std::vector<ConstPropertyPtr> particleProperties;
    if(transferParticleProperties()) {
        for(const PropertyObject* property : particles->properties()) {
            // Certain properties should not be transferred to the mesh vertices.
            if(property->type() == ParticlesObject::SelectionProperty) continue;
            if(property->type() == ParticlesObject::PositionProperty) continue;
            if(property->type() == ParticlesObject::IdentifierProperty) continue;
            particleProperties.push_back(property);
        }
    }

    // Create an empty surface mesh.
    DataOORef<SurfaceMesh> mesh = DataOORef<SurfaceMesh>::create(ObjectCreationParams::WithoutVisElement, tr("Surface"));
    mesh->setIdentifier(input.generateUniqueIdentifier<SurfaceMesh>(QStringLiteral("surface")));
    mesh->setDataSource(request.modApp());
    mesh->setDomain(simCell);
    mesh->setVisElement(surfaceMeshVis());

    if(method() == AlphaShape) {
        // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
        return std::make_shared<AlphaShapeEngine>(
                request,
                posProperty,
                selProperty,
                std::move(grainProperty),
                std::move(mesh),
                probeSphereRadius(),
                smoothingLevel(),
                selectSurfaceParticles(),
                identifyRegions(),
                mapParticlesToRegions() && identifyRegions(),
                computeSurfaceDistance(),
                std::move(particleProperties));
    }
    else {
        // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
        return std::make_shared<GaussianDensityEngine>(
                request,
                posProperty,
                selProperty,
                std::move(mesh),
                radiusFactor(),
                isoValue(),
                gridResolution(),
                computeSurfaceDistance(),
                particles->inputParticleRadii(),
                std::move(particleProperties));
    }
}

/******************************************************************************
* Performs the actual analysis. This method is executed in a worker thread.
******************************************************************************/
void ConstructSurfaceModifier::AlphaShapeEngine::perform()
{
    setProgressText(tr("Constructing surface mesh"));
    OVITO_ASSERT(mesh()->domain());

    if(probeSphereRadius() <= 0)
        throw Exception(tr("Radius parameter must be positive."));

    if(mesh()->domain()->volume3D() <= FLOATTYPE_EPSILON*FLOATTYPE_EPSILON*FLOATTYPE_EPSILON)
        throw Exception(tr("Simulation cell is degenerate (volume of parallelepiped is zero)."));

    double alpha = probeSphereRadius() * probeSphereRadius();
    FloatType ghostLayerSize = probeSphereRadius() * FloatType(3.5);

    // Check if combination of radius parameter and simulation cell size is valid.
    for(size_t dim = 0; dim < 3; dim++) {
        if(mesh()->domain()->hasPbc(dim)) {
            int stencilCount = (int)ceil(ghostLayerSize / mesh()->domain()->matrix().column(dim).dot(mesh()->domain()->cellNormalVector(dim)));
            if(stencilCount > 1)
                throw Exception(tr("Cannot generate Delaunay tessellation. Simulation cell is too small, or radius parameter is too large."));
        }
    }

    // Algorithm is divided into several sub-steps.
    // Assign weights to sub-steps according to estimated runtime.
    beginProgressSubStepsWithWeights({ 10, 30, 2, 2, 2, surfaceDistances() ? 1000 : 1 });

    // Generate Delaunay tessellation.
    DelaunayTessellation tessellation;

    // When identifying regions (including empty ones), we need to make sure that the entire simulation cell is covered by
    // finite Delaunay tetrahedra. In other words, we have to prevent infinite tetrahedra from penetrating into the simulation cekk.
    // This is accomplished in the DelaunayTessellation class by adding 8 extra input points to the tessellation,
    // far away from the simulation cell and any real input particles. These 8 points form a convex hull, whose interior gets tessellated.
    bool coverDomainWithFiniteTets = _identifyRegions;

    if(!tessellation.generateTessellation(
            mesh()->domain(),
            ConstPropertyAccess<Point3>(positions()).cbegin(),
            positions()->size(),
            ghostLayerSize,
            coverDomainWithFiniteTets,
            selection() ? ConstPropertyAccess<int>(selection()).cbegin() : nullptr,
            *this))
        return;
    OVITO_ASSERT(tessellation.simCell());

    nextProgressSubStep();

    SurfaceMeshAccess mesh(this->mesh());

    // Predefine the filled spatial regions of the output SurfaceMesh if the input particles are divided into separate grains by e.g. a GrainSegmentationModifier.
    if(_identifyRegions && particleGrains()) {

        // Determine the maximum grain ID.
        qlonglong maxGrainId = 0;
        if(particleGrains()->size() != 0) {
            maxGrainId = qBound((qlonglong)0,
                *boost::max_element(ConstPropertyAccess<qlonglong>(particleGrains())),
                static_cast<qlonglong>(std::numeric_limits<SurfaceMeshAccess::region_index>::max() - 1));
        }

        // Create one region in the output mesh for each grain.
        mesh.createRegions(maxGrainId + 1);
    }

    // Helper function that determines which spatial region a filled Delaunay cell belongs to.
    // This is only used if the input particles have previously been divided into grains by a GrainSegmentationModifier.
    // Otherwise, all tetrahedra are attributed to the null grain initially. Subsequently, they will be
    // grouped into disconnected sets, which form the regions of the output SurfaceMesh.
    auto tetrahedronRegion = [&,grains = ConstPropertyAccess<qlonglong>(_identifyRegions ? particleGrains() : nullptr)](DelaunayTessellation::CellHandle cell) -> SurfaceMeshAccess::region_index {
        if(grains) {
            // Decide which particle cluster the Delaunay cell belongs to.
            // We need a tie-breaker in case the four vertex atoms belong to different grains.
            qlonglong result = 0;
            for(int v = 0; v < 4; v++) {
                size_t particleIndex = tessellation.vertexIndex(tessellation.cellVertex(cell, v));
                qlonglong clusterId = grains[particleIndex];
                if(clusterId > result)
                    result = clusterId;
            }
            return result;
        }
        return 0;
    };

    // This callback function is called for every surface facet created by the manifold construction helper.
    // It marks the particles corresponding to the mesh vertices as belonging to the surface.
    PropertyAccess<int> surfaceParticleSelectionArray(surfaceParticleSelection());
    auto prepareMeshFace = [&](SurfaceMeshAccess::face_index face, const std::array<size_t,3>& vertexIndices, const std::array<DelaunayTessellation::VertexHandle,3>& vertexHandles, DelaunayTessellation::CellHandle cell) {
        // Mark the face's corner particles as belonging to the surface.
        if(surfaceParticleSelectionArray) {
            for(size_t vi : vertexIndices) {
                OVITO_ASSERT(vi < surfaceParticleSelectionArray.size());
                surfaceParticleSelectionArray[vi] = 1;
            }
        }
    };

    // This callback function is called for every surface vertex created by the manifold construction helper.
    // It registers the vertex in the map that associates each mesh vertex with its original input particle.
    std::vector<size_t> vertexToParticleMap;
    auto prepareMeshVertex = [&](SurfaceMeshAccess::vertex_index vertex, size_t particleIndex) {
        OVITO_ASSERT(vertex == vertexToParticleMap.size());
        vertexToParticleMap.push_back(particleIndex);
    };

    if(!_identifyRegions) {
        // Predefine the filled spatial region.
        // An empty region is not defined, because we are creating only a one-sided surface mesh.
        mesh.createRegion();
        OVITO_ASSERT(mesh.regionCount() == 1);

        // Just construct a one-sided surface mesh without caring about spatial regions.
        ManifoldConstructionHelper manifoldConstructor(tessellation, mesh, alpha, false, positions());
        if(!manifoldConstructor.construct(tetrahedronRegion, *this, std::move(prepareMeshFace), std::move(prepareMeshVertex)))
            return;
    }
    else {
        if(!particleRegionIds())
            beginProgressSubStepsWithWeights({ 2, 1 });
        else
            beginProgressSubStepsWithWeights({ 2, 1, 1 });

        // Construct a two-sided surface mesh with mesh faces associated with spatial regions (filled or solid).
        ManifoldConstructionHelper manifoldConstructor(tessellation, mesh, alpha, true, positions());
        if(!manifoldConstructor.construct(tetrahedronRegion, *this, std::move(prepareMeshFace), std::move(prepareMeshVertex)))
            return;

        nextProgressSubStep();

        // After construct() above has identified the filled regions, now identify the empty regions.
        if(!manifoldConstructor.formEmptyRegions(*this))
            return;

        _filledRegionCount = manifoldConstructor.filledRegionCount();
        _emptyRegionCount = manifoldConstructor.emptyRegionCount();

        // Transfer the region ID information to the output particles.
        if(PropertyAccess<int> regionIds = particleRegionIds()) {
            nextProgressSubStep();
            setProgressMaximum(regionIds.size());
            size_t numProcessedParticles = 0;
            // Initially, mark all particles as not assigned to any region (special region ID -1).
            boost::fill(regionIds, -1);
            // Visit each tetrahedral cell and assign its four vertex particles to the region of the cell.
            DelaunayTessellation::CellHandle queryHint = DelaunayTessellation::CellHandle(-1);
            for(DelaunayTessellation::CellIterator cell = tessellation.begin_cells(); cell != tessellation.end_cells(); ++cell) {
                if(tessellation.isGhostCell(*cell) || !tessellation.isFiniteCell(*cell))
                    continue;
                queryHint = *cell;
                if(int regionId = tessellation.getUserField(*cell); regionId >= 0) {
                    OVITO_ASSERT(regionId >= 0 && regionId < _filledRegionCount + _emptyRegionCount);
                    for(int v = 0; v < 4; v++) {
                        size_t particleIndex = tessellation.vertexIndex(tessellation.cellVertex(*cell, v));
                        OVITO_ASSERT(particleIndex < regionIds.size() || particleIndex == std::numeric_limits<size_t>::max());
                        // Give precedence to filled regions. Particles on the boundary are always assigned to the filled region, not the empty region.
                        if(particleIndex != std::numeric_limits<size_t>::max()) {
                            if(regionIds[particleIndex] == -1) {
                                if(!setProgressValueIntermittent(++numProcessedParticles))
                                    return;
                            }
                            if(regionId < _filledRegionCount || regionIds[particleIndex] == -1)
                                regionIds[particleIndex] = regionId;
                        }
                    }
                }
            }

            // If only selected particles were used as input points for the Delaunay tessellation, the unselected particles
            // are not attributed to any region yet. We do the attribution next by performing point queries on the Delaunay tessellation.
            // For each unassigned particle we determine the Delaunay cell it is located in and then use its region.
            auto particleRegionId = regionIds.begin();
            for(const Point3& pos : ConstPropertyAccess<Point3>(positions())) {
                if(*particleRegionId == -1) {
                    if(!setProgressValueIntermittent(++numProcessedParticles))
                        return;

                    DelaunayTessellation::CellHandle cell = tessellation.locate(tessellation.simCell()->wrapPoint(pos), queryHint);
                    OVITO_ASSERT(cell >= 0 && cell < tessellation.numberOfTetrahedra());

                    if(int regionId = tessellation.getUserField(cell); regionId >= 0) {
                        OVITO_ASSERT(regionId >= 0 && regionId < _filledRegionCount + _emptyRegionCount);
                        *particleRegionId = regionId;
                    }
                    queryHint = cell;
                }
                ++particleRegionId;
            }
        }

        // Output "Filled" region property.
        PropertyAccess<int> filledProperty(mesh.createRegionProperty(SurfaceMeshRegions::IsFilledProperty));
        std::fill(filledProperty.begin(), filledProperty.begin() + _filledRegionCount, 1);
        std::fill(filledProperty.begin() + _filledRegionCount, filledProperty.end(), 0);

        endProgressSubSteps();
    }

    // Create mesh vertex properties.
    for(const ConstPropertyPtr& particleProperty : particleProperties()) {
        PropertyPtr vertexProperty;
        if(particleProperty->type() < PropertyObject::FirstSpecificProperty && SurfaceMeshVertices::OOClass().isValidStandardPropertyId(particleProperty->type())) {
            // Input property is also a standard property for mesh vertices.
            vertexProperty = mesh.createVertexProperty(static_cast<SurfaceMeshVertices::Type>(particleProperty->type()));
            OVITO_ASSERT(vertexProperty->dataType() == particleProperty->dataType());
            OVITO_ASSERT(vertexProperty->stride() == particleProperty->stride());
        }
        else if(SurfaceMeshVertices::OOClass().standardPropertyTypeId(particleProperty->name()) != 0) {
            // Input property name is that of a standard property for mesh vertices.
            // Must rename the property to avoid conflict, because user properties may not have a standard property name.
            QString newPropertyName = particleProperty->name() + tr("_particles");
            vertexProperty = mesh.createVertexProperty(newPropertyName, particleProperty->dataType(), particleProperty->componentCount(), DataBuffer::NoFlags, particleProperty->componentNames());
        }
        else {
            // Input property is a user property for mesh vertices.
            vertexProperty = mesh.createVertexProperty(particleProperty->name(), particleProperty->dataType(), particleProperty->componentCount(), DataBuffer::NoFlags, particleProperty->componentNames());
        }
        // Copy particle property values to mesh vertices using precomputed index mapping.
        particleProperty->mappedCopyTo(*vertexProperty, vertexToParticleMap);
    }

    nextProgressSubStep();

    // Make sure every mesh vertex is only part of one surface manifold.
    SurfaceMeshAccess::size_type duplicatedVertices = mesh.makeManifold();

    nextProgressSubStep();
    if(!mesh.smoothMesh(_smoothingLevel, *this))
        return;

    nextProgressSubStep();

    if(_identifyRegions) {
        // Create the 'Surface area' region property.
        PropertyAccess<FloatType> surfaceAreaProperty = mesh.createRegionProperty(SurfaceMeshRegions::SurfaceAreaProperty, DataBuffer::InitializeMemory);

        // Compute surface area (total and per region) by summing up the triangle face areas.
        setProgressMaximum(mesh.faceCount());
        for(SurfaceMeshAccess::edge_index edge : mesh.firstFaceEdges()) {
            if(!incrementProgressValue()) return;
            const Vector3& e1 = mesh.edgeVector(edge);
            const Vector3& e2 = mesh.edgeVector(mesh.nextFaceEdge(edge));
            FloatType faceArea = e1.cross(e2).length() / 2;
            SurfaceMeshAccess::region_index region = mesh.faceRegion(mesh.adjacentFace(edge));
            surfaceAreaProperty[region] += faceArea;

            // Only count surface area of outer surface, which is bordering an empty region.
            // Don't count area of internal interfaces, which have filled regions on either side.
            if(region >= _filledRegionCount)
                addSurfaceArea(faceArea);
        }

        // Compute total volumes.
        // Total volume of filled regions:
        for(SurfaceMeshAccess::region_index region = 0; region < _filledRegionCount; region++)
            _totalFilledVolume += mesh.regionVolume(region);
        // Total volume of empty regions (all and only interior voids):
        ConstPropertyAccess<int> regionPropertyIsExterior = mesh.regionProperty(SurfaceMeshRegions::IsExteriorProperty);
        for(SurfaceMeshAccess::region_index region = _filledRegionCount; region < mesh.regionCount(); region++) {
            FloatType vol = mesh.regionVolume(region);
            _totalEmptyVolume += vol;
            if(!regionPropertyIsExterior[region]) {
                _totalVoidVolume += vol;
                _voidRegionCount++;
            }
        }
    }
    else {
        // Compute total surface area by summing up the triangle face areas.
        setProgressMaximum(mesh.faceCount());
        for(SurfaceMeshAccess::edge_index edge : mesh.firstFaceEdges()) {
            if(!incrementProgressValue()) return;
            const Vector3& e1 = mesh.edgeVector(edge);
            const Vector3& e2 = mesh.edgeVector(mesh.nextFaceEdge(edge));
            FloatType faceArea = e1.cross(e2).length() / 2;
            addSurfaceArea(faceArea);
        }
    }

    if(isCanceled())
        return;

    nextProgressSubStep();

    // Compute the distance of each input particle from the constructed surface.
    computeSurfaceDistances(mesh);

    endProgressSubSteps();

    // Release data that is no longer needed.
    releaseWorkingData();
}

/******************************************************************************
* Performs the actual analysis. This method is executed in a worker thread.
******************************************************************************/
void ConstructSurfaceModifier::GaussianDensityEngine::perform()
{
    setProgressText(tr("Constructing surface mesh"));
    OVITO_ASSERT(mesh()->domain());

    // Check input data.
    if(mesh()->domain()->volume3D() <= FLOATTYPE_EPSILON*FLOATTYPE_EPSILON*FLOATTYPE_EPSILON)
        throw Exception(tr("Simulation cell is degenerate."));

    if(positions()->size() == 0) {
        // Release data that is no longer needed.
        releaseWorkingData();
        return;
    }

    // Algorithm is divided into several sub-steps.
    // Assign weights to sub-steps according to estimated runtime.
    beginProgressSubStepsWithWeights({ 1, 30, 1600, 1500, 30, 500, 100, 300, surfaceDistances() ? 10000 : 1 });

    // Access the atomic radii.
    ConstPropertyAccess<FloatType> particleRadii(_particleRadii);

    // Determine the cutoff range of atomic Gaussians.
    FloatType cutoffSize = FloatType(3) * *boost::max_element(particleRadii) * _radiusFactor;

    // Determine the extents of the density grid.
    AffineTransformation gridBoundaries = mesh()->domain()->matrix();
    ConstPropertyAccess<Point3> positionsArray(positions());
    for(size_t dim = 0; dim < 3; dim++) {
        // Use bounding box of particles in directions that are non-periodic.
        if(!mesh()->domain()->hasPbc(dim)) {
            // Compute range of relative atomic coordinates in the current direction.
            FloatType xmin =  FLOATTYPE_MAX;
            FloatType xmax = -FLOATTYPE_MAX;
            const AffineTransformation inverseCellMatrix = mesh()->domain()->inverseMatrix();
            for(const Point3& p : positionsArray) {
                FloatType rp = inverseCellMatrix.prodrow(p, dim);
                if(rp < xmin) xmin = rp;
                if(rp > xmax) xmax = rp;
            }

            // Need to add extra margin along non-periodic dimensions, because Gaussian functions reach beyond atomic radii.
            FloatType rcutoff = cutoffSize / gridBoundaries.column(dim).length();
            xmin -= rcutoff;
            xmax += rcutoff;

            gridBoundaries.column(3) += xmin * gridBoundaries.column(dim);
            gridBoundaries.column(dim) *= (xmax - xmin);
        }
    }

    // Determine the number of voxels in each direction of the density grid.
    size_t gridDims[3];
    FloatType voxelSizeX = gridBoundaries.column(0).length() / _gridResolution;
    FloatType voxelSizeY = gridBoundaries.column(1).length() / _gridResolution;
    FloatType voxelSizeZ = gridBoundaries.column(2).length() / _gridResolution;
    FloatType voxelSize = std::max(voxelSizeX, std::max(voxelSizeY, voxelSizeZ));
    gridDims[0] = std::max((size_t)2, (size_t)(gridBoundaries.column(0).length() / voxelSize));
    gridDims[1] = std::max((size_t)2, (size_t)(gridBoundaries.column(1).length() / voxelSize));
    gridDims[2] = std::max((size_t)2, (size_t)(gridBoundaries.column(2).length() / voxelSize));

    nextProgressSubStep();

    // Allocate storage for the density grid values.
    std::vector<FloatType> densityData(gridDims[0] * gridDims[1] * gridDims[2], FloatType(0));

    // Set up a particle neighbor finder to speed up density field computation.
    CutoffNeighborFinder neighFinder;
    if(!neighFinder.prepare(cutoffSize, positions(), mesh()->domain(), selection()))
        return;

    nextProgressSubStep();

    // Set up a matrix that converts grid coordinates to spatial coordinates.
    AffineTransformation gridToCartesian = gridBoundaries;
    gridToCartesian.column(0) /= gridDims[0] - (mesh()->domain()->hasPbc(0)?0:1);
    gridToCartesian.column(1) /= gridDims[1] - (mesh()->domain()->hasPbc(1)?0:1);
    gridToCartesian.column(2) /= gridDims[2] - (mesh()->domain()->hasPbc(2)?0:1);

    // Compute the accumulated density at each grid point.
    parallelForWithProgress(densityData.size(), [&](size_t voxelIndex) {

        // Determine the center coordinates of the current grid cell.
        size_t ix = voxelIndex % gridDims[0];
        size_t iy = (voxelIndex / gridDims[0]) % gridDims[1];
        size_t iz = voxelIndex / (gridDims[0] * gridDims[1]);
        Point3 voxelCenter = gridToCartesian * Point3(ix, iy, iz);
        FloatType& density = densityData[voxelIndex];

        // Visit all particles in the vicinity of the center point.
        for(CutoffNeighborFinder::Query neighQuery(neighFinder, voxelCenter); !neighQuery.atEnd(); neighQuery.next()) {
            FloatType alpha = _radiusFactor * particleRadii[neighQuery.current()];
            density += std::exp(-neighQuery.distanceSquared() / (FloatType(2) * alpha * alpha));
        }
    });
    if(isCanceled())
        return;

    nextProgressSubStep();

    // Set up callback function returning the field value, which will be passed to the marching cubes algorithm.
    auto getFieldValue = [
            _data = densityData.data(),
            _pbcFlags = mesh()->domain()->pbcFlags(),
            _gridShape = gridDims
            ](int i, int j, int k) -> FloatType {
        if(_pbcFlags[0]) {
            if(i == _gridShape[0]) i = 0;
        }
        else {
            if(i == 0 || i == _gridShape[0] + 1) return std::numeric_limits<FloatType>::lowest();
            i--;
        }
        if(_pbcFlags[1]) {
            if(j == _gridShape[1]) j = 0;
        }
        else {
            if(j == 0 || j == _gridShape[1] + 1) return std::numeric_limits<FloatType>::lowest();
            j--;
        }
        if(_pbcFlags[2]) {
            if(k == _gridShape[2]) k = 0;
        }
        else {
            if(k == 0 || k == _gridShape[2] + 1) return std::numeric_limits<FloatType>::lowest();
            k--;
        }
        OVITO_ASSERT(i >= 0 && i < _gridShape[0]);
        OVITO_ASSERT(j >= 0 && j < _gridShape[1]);
        OVITO_ASSERT(k >= 0 && k < _gridShape[2]);
        return _data[(i + j*_gridShape[0] + k*_gridShape[0]*_gridShape[1])];
    };

    // Temporarily set the domain of the output mesh to the grid domain.
    DataOORef<const SimulationCellObject> originalDomain = mesh()->domain();
    if(mesh()->domain()->cellMatrix() != gridBoundaries) {
        auto newCell = DataOORef<SimulationCellObject>::makeCopy(mesh()->domain());
        newCell->setCellMatrix(gridBoundaries);
        mesh()->setDomain(std::move(newCell));
    }

    // Construct isosurface of the density field.
    SurfaceMeshAccess mesh(this->mesh());
    MarchingCubes mc(mesh, gridDims[0], gridDims[1], gridDims[2], false, std::move(getFieldValue));
    if(!mc.generateIsosurface(_isoLevel, *this))
        return;

    nextProgressSubStep();

    // Transform mesh vertices from orthogonal grid space to world space.
    mesh.transformVertices(gridToCartesian);
    if(isCanceled())
        return;

    nextProgressSubStep();

    // Create mesh vertex properties for transferring particle property values to the surface.
    std::vector<std::pair<ConstPropertyAccess<FloatType,true>, PropertyAccess<FloatType,true>>> propertyMapping;
    for(const ConstPropertyPtr& particleProperty : particleProperties()) {
        // Can only transfer floating-point properties, because we'll need to blend values of several particles.
        if(particleProperty->dataType() == PropertyObject::Float) {
            PropertyPtr vertexProperty;
            if(particleProperty->type() < PropertyObject::FirstSpecificProperty && SurfaceMeshVertices::OOClass().isValidStandardPropertyId(particleProperty->type())) {
                // Input property is also a standard property for mesh vertices.
                vertexProperty = mesh.createVertexProperty(static_cast<SurfaceMeshVertices::Type>(particleProperty->type()), DataBuffer::InitializeMemory);
                OVITO_ASSERT(vertexProperty->dataType() == particleProperty->dataType());
                OVITO_ASSERT(vertexProperty->stride() == particleProperty->stride());
            }
            else if(SurfaceMeshVertices::OOClass().standardPropertyTypeId(particleProperty->name()) != 0) {
                // Input property name is that of a standard property for mesh vertices.
                // Must rename the property to avoid conflict, because user properties may not have a standard property name.
                QString newPropertyName = particleProperty->name() + tr("_particles");
                vertexProperty = mesh.createVertexProperty(newPropertyName, particleProperty->dataType(), particleProperty->componentCount(), DataBuffer::InitializeMemory, particleProperty->componentNames());
            }
            else {
                // Input property is a user property for mesh vertices.
                vertexProperty = mesh.createVertexProperty(particleProperty->name(), particleProperty->dataType(), particleProperty->componentCount(), DataBuffer::InitializeMemory, particleProperty->componentNames());
            }
            propertyMapping.emplace_back(particleProperty, std::move(vertexProperty));
        }
    }

    // Transfer property values from particles to the mesh vertices.
    if(!propertyMapping.empty()) {
        // Compute the accumulated density at each grid point.
        parallelForWithProgress(mesh.vertexCount(), [&](size_t vertexIndex) {
            // Visit all particles in the vicinity of the vertex.
            FloatType weightSum = 0;
            for(CutoffNeighborFinder::Query neighQuery(neighFinder, mesh.vertexPosition(vertexIndex)); !neighQuery.atEnd(); neighQuery.next()) {
                FloatType alpha = _radiusFactor * particleRadii[neighQuery.current()];
                FloatType weight = std::exp(-neighQuery.distanceSquared() / (FloatType(2) * alpha * alpha));
                // Perform summation of particle contributions to the property values at the current mesh vertex.
                for(auto& p : propertyMapping) {
                    for(size_t component = 0; component < p.first.componentCount(); component++) {
                        p.second.value(vertexIndex, component) += weight * p.first.get(neighQuery.current(), component);
                    }
                }
                weightSum += weight;
            }
            if(weightSum != 0) {
                // Normalize property values.
                for(auto& p : propertyMapping) {
                    for(size_t component = 0; component < p.second.componentCount(); component++) {
                        p.second.value(vertexIndex, component) /= weightSum;
                    }
                }
            }
        });
        if(isCanceled())
            return;
    }

    // Flip surface orientation if cell is mirrored.
    if(gridToCartesian.determinant() < 0)
        mesh.flipFaces();

    // Restore original mesh domain.
    mesh.setDomain(std::move(originalDomain));

    nextProgressSubStep();

    if(!mesh.connectOppositeHalfedges())
        throw Exception(tr("Something went wrong. Isosurface mesh is not closed."));
    if(isCanceled())
        return;

    nextProgressSubStep();

    // Compute surface area (only total) by summing up the triangle face areas.
    for(SurfaceMeshAccess::edge_index edge : mesh.firstFaceEdges()) {
        if(isCanceled()) return;
        const Vector3& e1 = mesh.edgeVector(edge);
        const Vector3& e2 = mesh.edgeVector(mesh.nextFaceEdge(edge));
        FloatType area = e1.cross(e2).length() / 2;
        addSurfaceArea(area);
    }
    if(isCanceled())
        return;

    nextProgressSubStep();

    // Compute the distance of each input particle from the constructed surface.
    computeSurfaceDistances(mesh);

    endProgressSubSteps();

    // Release data that is no longer needed.
    releaseWorkingData();
    particleRadii.reset();
    _particleRadii.reset();
}

/******************************************************************************
* Compute the distance of each input particle from the constructed surface.
******************************************************************************/
void ConstructSurfaceModifier::ConstructSurfaceEngineBase::computeSurfaceDistances(const SurfaceMeshAccess& mesh)
{
    if(!surfaceDistances())
        return;
    setProgressText(tr("Computing surface distances"));

    // Access output array.
    PropertyAccess<FloatType> distanceArray(surfaceDistances());
    // Access input positions.
    ConstPropertyAccess<Point3> positionArray(positions());

    // Perform computation for each particle.
    size_t progressChunkSize = 64;
    parallelForWithProgress(positions()->size(), [&](size_t index) {
        auto result = mesh.locatePoint(positionArray[index], 0.0);
        distanceArray[index] = result ? result->second : 0.0;
    }, progressChunkSize);
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ConstructSurfaceModifier::AlphaShapeEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    ConstructSurfaceModifier* modifier = static_object_cast<ConstructSurfaceModifier>(request.modifier());

    // Output the constructed surface mesh to the pipeline.
    state.addObjectWithUniqueId<SurfaceMesh>(mesh());

    if(surfaceParticleSelection() || particleRegionIds() || surfaceDistances()) {
        ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
        particles->verifyIntegrity();

        // Output selection of surface particles.
        if(surfaceParticleSelection())
            particles->createProperty(surfaceParticleSelection());

        // Output particle region IDs.
        if(particleRegionIds())
            particles->createProperty(particleRegionIds());

        // Output computed particle distances from surface.
        if(surfaceDistances())
            particles->createProperty(surfaceDistances());
    }

    // Output total surface area.
    state.addAttribute(QStringLiteral("ConstructSurfaceMesh.surface_area"), QVariant::fromValue(surfaceArea()), request.modApp());

    if(_identifyRegions) {

        // Output more global attributes.
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.cell_volume"), QVariant::fromValue(_totalCellVolume), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.specific_surface_area"), QVariant::fromValue(_totalCellVolume ? (surfaceArea() / _totalCellVolume) : 0), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.filled_volume"), QVariant::fromValue(_totalFilledVolume), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.filled_fraction"), QVariant::fromValue(_totalCellVolume ? (_totalFilledVolume / _totalCellVolume) : 0), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.filled_region_count"), QVariant::fromValue(_filledRegionCount), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.empty_volume"), QVariant::fromValue(_totalEmptyVolume), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.empty_fraction"), QVariant::fromValue(_totalCellVolume ? (_totalEmptyVolume / _totalCellVolume) : 0), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.empty_region_count"), QVariant::fromValue(_emptyRegionCount), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.void_volume"), QVariant::fromValue(_totalVoidVolume), request.modApp());
        state.addAttribute(QStringLiteral("ConstructSurfaceMesh.void_region_count"), QVariant::fromValue(_voidRegionCount), request.modApp());

        QString statusString = tr("Surface area: %1\n# filled regions (volume): %2 (%3)\n# empty regions (volume): %4 (%5)\n# void regions (volume): %6 (%7)")
                .arg(surfaceArea())
                .arg(_filledRegionCount)
                .arg(_totalFilledVolume)
                .arg(_emptyRegionCount)
                .arg(_totalEmptyVolume)
                .arg(_voidRegionCount)
                .arg(_totalVoidVolume);

        state.setStatus(PipelineStatus(PipelineStatus::Success, std::move(statusString)));
    }
    else {
        state.setStatus(PipelineStatus(PipelineStatus::Success, tr("Surface area: %1").arg(surfaceArea())));
    }
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ConstructSurfaceModifier::GaussianDensityEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    ConstructSurfaceModifier* modifier = static_object_cast<ConstructSurfaceModifier>(request.modifier());

    // Output the constructed surface mesh to the pipeline.
    state.addObjectWithUniqueId<SurfaceMesh>(mesh());

    // Output computed particle distances from surface.
    if(surfaceDistances()) {
        ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
        particles->verifyIntegrity();
        particles->createProperty(surfaceDistances());
    }

    // Output total surface area.
    state.addAttribute(QStringLiteral("ConstructSurfaceMesh.surface_area"), QVariant::fromValue(surfaceArea()), request.modApp());

    state.setStatus(PipelineStatus(PipelineStatus::Success, tr("Surface area: %1").arg(surfaceArea())));
}

}   // End of namespace
