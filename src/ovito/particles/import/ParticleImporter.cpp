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

#include <ovito/particles/Particles.h>
#include <ovito/particles/modifier/modify/LoadTrajectoryModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "ParticleImporter.h"

#include <boost/range/numeric.hpp>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParticleImporter);
DEFINE_PROPERTY_FIELD(ParticleImporter, sortParticles);
DEFINE_PROPERTY_FIELD(ParticleImporter, generateBonds);
DEFINE_PROPERTY_FIELD(ParticleImporter, recenterCell);
SET_PROPERTY_FIELD_LABEL(ParticleImporter, sortParticles, "Sort particles by ID");
SET_PROPERTY_FIELD_LABEL(ParticleImporter, generateBonds, "Generate bonds");
SET_PROPERTY_FIELD_LABEL(ParticleImporter, recenterCell, "Center simulation box on coordinate origin");

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ParticleImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
    FileSourceImporter::propertyChanged(field);

    if(field == PROPERTY_FIELD(sortParticles) || field == PROPERTY_FIELD(generateBonds) || field == PROPERTY_FIELD(recenterCell)) {
        if(!isBeingLoaded()) {
            // Reload input file(s) when these options are changed by the user.
            // But there is no need to refetch the data file(s) from the remote location. Re-parsing the cached files is sufficient.
            requestReload();
        }
    }
}

/******************************************************************************
* Returns the particles container object, newly creating it first if necessary.
******************************************************************************/
Particles* ParticleImporter::FrameLoader::particles()
{
    if(!_particles) {
        _particles = state().getMutableObject<Particles>();
        if(!_particles) {
            _particles = state().createObject<Particles>(pipelineNode());
            _areParticlesNewlyCreated = true;
        }
    }
    return _particles;
}

/******************************************************************************
* Returns the bonds container object, newly creating it first if necessary.
******************************************************************************/
Bonds* ParticleImporter::FrameLoader::bonds()
{
    if(!_bonds) {
        setKeepExistingTopology(true);
        if(particles()->bonds()) {
            _bonds = particles()->makeBondsMutable();
        }
        else {
            particles()->setBonds(DataOORef<Bonds>::create());
            _bonds = particles()->makeBondsMutable();
            _bonds->setCreatedByNode(pipelineNode());
            _areBondsNewlyCreated = true;
        }
    }
    return _bonds;
}

/******************************************************************************
* Returns the angles container object, newly creating it first if necessary.
******************************************************************************/
Angles* ParticleImporter::FrameLoader::angles()
{
    if(!_angles) {
        setKeepExistingTopology(true);
        if(particles()->angles()) {
            _angles = particles()->makeAnglesMutable();
        }
        else {
            particles()->setAngles(DataOORef<Angles>::create());
            _angles = particles()->makeAnglesMutable();
            _angles->setCreatedByNode(pipelineNode());
            _areAnglesNewlyCreated = true;
        }
    }
    return _angles;
}

/******************************************************************************
* Returns the dihedrals container object, newly creating it first if necessary.
******************************************************************************/
Dihedrals* ParticleImporter::FrameLoader::dihedrals()
{
    if(!_dihedrals) {
        setKeepExistingTopology(true);
        if(particles()->dihedrals()) {
            _dihedrals = particles()->makeDihedralsMutable();
        }
        else {
            particles()->setDihedrals(DataOORef<Dihedrals>::create());
            _dihedrals = particles()->makeDihedralsMutable();
            _dihedrals->setCreatedByNode(pipelineNode());
            _areDihedralsNewlyCreated = true;
        }
    }
    return _dihedrals;
}

/******************************************************************************
* Returns the impropers container object, newly creating it first if necessary.
******************************************************************************/
Impropers* ParticleImporter::FrameLoader::impropers()
{
    if(!_impropers) {
        setKeepExistingTopology(true);
        if(particles()->impropers()) {
            _impropers = particles()->makeImpropersMutable();
        }
        else {
            particles()->setImpropers(DataOORef<Impropers>::create());
            _impropers = particles()->makeImpropersMutable();
            _impropers->setCreatedByNode(pipelineNode());
            _areImpropersNewlyCreated = true;
        }
    }
    return _impropers;
}

/******************************************************************************
* Creates a particle object (if the particle count is non-zero) and adjusts the
* number of elements of the property container.
******************************************************************************/
void ParticleImporter::FrameLoader::setParticleCount(size_t count)
{
    if(count != 0) {
        particles()->setElementCount(count);
    }
    else {
        if(const Particles* particles = state().getObject<Particles>())
            state().removeObject(particles);
        _particles = nullptr;
    }
}

/******************************************************************************
* Creates a bonds container object (if the bond count is non-zero) and adjusts the
* number of elements of the property container.
******************************************************************************/
void ParticleImporter::FrameLoader::setBondCount(size_t count)
{
    if(count != 0) {
        bonds()->setElementCount(count);
    }
    else {
        if(const Particles* particles = state().getObject<Particles>())
            if(particles->bonds())
                state().makeMutable(particles)->setBonds(nullptr);
        _bonds = nullptr;
    }
}

/******************************************************************************
* Creates an angles container object (if the bond count is non-zero) and adjusts the
* number of elements of the property container.
******************************************************************************/
void ParticleImporter::FrameLoader::setAngleCount(size_t count)
{
    if(count != 0) {
        angles()->setElementCount(count);
    }
    else {
        if(const Particles* particles = state().getObject<Particles>())
            if(particles->angles())
                state().makeMutable(particles)->setAngles(nullptr);
        _angles = nullptr;
    }
}

/******************************************************************************
* Creates a dihedrals container object (if the bond count is non-zero) and adjusts the
* number of elements of the property container.
******************************************************************************/
void ParticleImporter::FrameLoader::setDihedralCount(size_t count)
{
    if(count != 0) {
        dihedrals()->setElementCount(count);
    }
    else {
        if(const Particles* particles = state().getObject<Particles>())
            if(particles->dihedrals())
                state().makeMutable(particles)->setDihedrals(nullptr);
        _dihedrals = nullptr;
    }
}

/******************************************************************************
* Creates an impropers container object (if the bond count is non-zero) and adjusts the
* number of elements of the property container.
******************************************************************************/
void ParticleImporter::FrameLoader::setImproperCount(size_t count)
{
    if(count != 0) {
        impropers()->setElementCount(count);
    }
    else {
        if(const Particles* particles = state().getObject<Particles>())
            if(particles->impropers())
                state().makeMutable(particles)->setImpropers(nullptr);
        _impropers = nullptr;
    }
}

/******************************************************************************
* Determines the PBC shift vectors for bonds using the minimum image convention.
******************************************************************************/
void ParticleImporter::FrameLoader::generateBondPeriodicImageProperty()
{
    BufferReadAccess<Point3> posProperty = particles()->getProperty(Particles::PositionProperty);
    if(!posProperty) return;

    BufferReadAccess<ParticleIndexPair> bondTopologyProperty = bonds()->getProperty(Bonds::TopologyProperty);
    if(!bondTopologyProperty) return;

    BufferWriteAccess<Vector3I, access_mode::discard_write> bondPeriodicImageProperty = bonds()->createProperty(Bonds::PeriodicImageProperty);

    if(!hasSimulationCell() || !simulationCell()->hasPbcCorrected()) {
        boost::fill(bondPeriodicImageProperty, Vector3I::Zero());
    }
    else {
        const AffineTransformation inverseCellMatrix = simulationCell()->inverseMatrix();
        const std::array<bool,3> pbcFlags = simulationCell()->pbcFlagsCorrected();
        for(size_t bondIndex = 0; bondIndex < bondTopologyProperty.size(); bondIndex++) {
            size_t index1 = bondTopologyProperty[bondIndex][0];
            size_t index2 = bondTopologyProperty[bondIndex][1];
            OVITO_ASSERT(index1 < posProperty.size() && index2 < posProperty.size());
            Vector3 delta = posProperty[index1] - posProperty[index2];
            for(size_t dim = 0; dim < 3; dim++) {
                bondPeriodicImageProperty[bondIndex][dim] = pbcFlags[dim] ? std::lround(inverseCellMatrix.prodrow(delta, dim)) : 0;
            }
        }
    }
}

/******************************************************************************
* Generates ad-hoc bonds between atoms based on their van der Waals radii.
******************************************************************************/
void ParticleImporter::FrameLoader::generateBonds(TaskProgress& progress)
{
    this_task::throwIfCanceled();
    if(!_particles)
        return;

    // Get the type particle property.
    const Property* typeProperty = _particles->getProperty(Particles::TypeProperty);
    const Property* positionProperty = _particles->getProperty(Particles::PositionProperty);
    if(!typeProperty || !positionProperty)
        return;

    // Do not delete the generated bonds again in FrameLoader::loadFile().
    setKeepExistingTopology(true);

    // Get the list of van der Waals radii.
    std::vector<FloatType> typeVdWRadiusMap;
    std::vector<bool> isHydrogenType;
    FloatType maxRadius = 0;
    for(const ElementType* type : typeProperty->elementTypes()) {
        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(type)) {
            if(ptype->vdwRadius() > 0.0 && ptype->numericId() >= 0) {
                if(ptype->vdwRadius() > maxRadius)
                    maxRadius = ptype->vdwRadius();
                if(type->numericId() >= typeVdWRadiusMap.size()) {
                    typeVdWRadiusMap.resize(type->numericId() + 1, 0.0);
                    isHydrogenType.resize(type->numericId() + 1, false);
                }
                typeVdWRadiusMap[type->numericId()] = ptype->vdwRadius();
                isHydrogenType[type->numericId()] = (ptype->name() == QStringLiteral("H"));
            }
        }
    }

    // Determine maximum bond distance cutoff.
    FloatType vdwPrefactor = 0.6; // Note: Value 0.6 has been adopted from VMD source code.
    FloatType maxCutoff = vdwPrefactor * 2.0 * maxRadius;
    if(maxCutoff == 0.0)
        return;
    FloatType minCutoffSquared = 1e-10 * maxCutoff * maxCutoff;
    progress.setText(tr("Generating bonds"));

    // Prepare the neighbor finder.
    CutoffNeighborFinder neighborFinder(maxCutoff, positionProperty, state().getObject<SimulationCell>(), {});

    BufferReadAccess<int32_t> particleTypesArray(typeProperty);

    // Multi-threaded loop over all particles, each thread producing a partial bonds list.
    size_t particleCount = positionProperty->size();
    auto partialBondsLists = parallelForCollect<std::vector<Bond>>(particleCount, 4096, progress, [&](size_t particleIndex, std::vector<Bond>& bondList) {
        // Kernel called for each particle: Iterate over the particle's neighbors within the cutoff range.
        for(CutoffNeighborFinder::Query neighborQuery(neighborFinder, particleIndex); !neighborQuery.atEnd(); neighborQuery.next()) {
            int type1 = particleTypesArray[particleIndex];
            int type2 = particleTypesArray[neighborQuery.current()];
            if(type1 >= 0 && type2 >= 0 && type1 < (int)typeVdWRadiusMap.size() && type2 < (int)typeVdWRadiusMap.size()) {
                if(isHydrogenType[type1] && isHydrogenType[type2])
                    continue;
                FloatType cutoff = vdwPrefactor * (typeVdWRadiusMap[type1] + typeVdWRadiusMap[type2]);
                if(neighborQuery.distanceSquared() <= cutoff*cutoff && neighborQuery.distanceSquared() >= minCutoffSquared) {
                    Bond bond = { particleIndex, neighborQuery.current(), neighborQuery.unwrappedPbcShift() };
                    // Skip every other bond to create only one bond per particle pair.
                    if(!bond.isOdd())
                        bondList.push_back(bond);
                }
            }
        }
    });

    // Create bonds.
    setBondCount(boost::accumulate(partialBondsLists, (size_t)0, [](size_t n, const std::vector<Bond>& bonds) { return n + bonds.size(); }));
    BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> bondTopologyProperty = this->bonds()->createProperty(Bonds::TopologyProperty);
    Property* bondTypeProperty = this->bonds()->createProperty(Bonds::TypeProperty);
    BufferWriteAccess<Vector3I, access_mode::discard_write> bondPeriodicImageProperty = this->bonds()->createProperty(Bonds::PeriodicImageProperty);

    // Create bond type.
    addNumericType(Bonds::OOClass(), bondTypeProperty, 1, {});
    bondTypeProperty->fill<int32_t>(1);

    // Transfer bonds lists to Bonds.
    auto bondTopologyIter = bondTopologyProperty.begin();
    auto bondPBCImageIter = bondPeriodicImageProperty.begin();
    for(const std::vector<Bond>& bondsList : partialBondsLists) {
        for(const Bond& bond : bondsList) {
            *bondTopologyIter++ = ParticleIndexPair{{(qlonglong)bond.index1, (qlonglong)bond.index2}};
            *bondPBCImageIter++ = bond.pbcShift;
        }
    }
    OVITO_ASSERT(bondTopologyIter == bondTopologyProperty.end());
}

/******************************************************************************
* If the 'Velocity' vector particle property is present, then this method
* computes the 'Velocity Magnitude' scalar property.
******************************************************************************/
void ParticleImporter::FrameLoader::computeVelocityMagnitude()
{
    if(!_particles || this_task::isCanceled())
        return;

    if(BufferReadAccess<Vector3> velocityVectors = _particles->getProperty(Particles::VelocityProperty)) {
        auto v = velocityVectors.cbegin();
        Property* magnitudeProperty = particles()->createProperty(Particles::VelocityMagnitudeProperty);
        for(FloatType& mag : BufferWriteAccess<FloatType, access_mode::discard_write>(magnitudeProperty)) {
            mag = v->length();
            ++v;
        }
    }
}

/******************************************************************************
* If the particles are centered on the coordinate origin but the current simulation cell corner is positioned at (0,0,0),
* the this method centers the cell at (0,0,0), leaving the particle coordinates unchanged.
******************************************************************************/
void ParticleImporter::FrameLoader::correctOffcenterCell()
{
    if(this_task::isCanceled())
        return;

    // Check if a simulation cell has been defined. It must be periodic in all directions.
    const SimulationCell* simulationCell = state().getObject<SimulationCell>();
    if(!simulationCell || !simulationCell->hasPbc(0) || !simulationCell->hasPbc(1) || (!simulationCell->hasPbc(2) && !simulationCell->is2D()))
        return;

    // The cell corner must be located at (0,0,0).
    if(simulationCell->cellOrigin() != Point3::Origin())
        return;

    // The current implementation is for 3D cells only.
    if(simulationCell->is2D() || simulationCell->cellMatrix().determinant() == 0.0)
        return;

    // Get the particle coordinates.
    BufferReadAccess<Point3> positions = _particles ? _particles->getProperty(Particles::PositionProperty) : nullptr;
    if(!positions || positions.size() == 0)
        return;

    // Compute bounding box of particles in reduced coordinates.
    Box3 boundingBox;
    const AffineTransformation reciprocalCellMatrix = simulationCell->reciprocalCellMatrix();
    for(const Point3& p : positions)
        boundingBox.addPoint(reciprocalCellMatrix * p);
    OVITO_ASSERT(!boundingBox.isEmpty());

    // Check if reduced coordinates of particles are all in the [-0.5, 0.5] range (with an added margin).
    if(boundingBox.minc.x() > -0.01 && boundingBox.minc.y() > -0.01 && boundingBox.minc.z() > -0.01)
        return;
    if(boundingBox.minc.x() < -0.51 || boundingBox.minc.y() < -0.51 || boundingBox.minc.z() < -0.51)
        return;
    if(boundingBox.maxc.x() > 0.51 || boundingBox.maxc.y() > 0.51 || boundingBox.maxc.z() > 0.51)
        return;

    // Translate the simulation box.
    SimulationCell* newSimulationCell = state().makeMutable(simulationCell);
    AffineTransformation cellMatrix = newSimulationCell->cellMatrix();
    cellMatrix.translation() = cellMatrix * Vector3(-0.5, -0.5, -0.5);
    newSimulationCell->setCellMatrix(cellMatrix);
}

/******************************************************************************
* Translates the simulation cell (and the particles) such that it is centered
* at the coordinate origin.
******************************************************************************/
void ParticleImporter::FrameLoader::recenterSimulationCell()
{
    if(this_task::isCanceled())
        return;

    SimulationCell* simulationCell = state().getMutableObject<SimulationCell>();
    if(!simulationCell) return;

    AffineTransformation cellMatrix = simulationCell->cellMatrix();
    Vector3 offset = cellMatrix * Point3(0.5, 0.5, 0.5) - Point3::Origin();
    if(offset == Vector3::Zero()) return;

    cellMatrix.translation() -= offset;
    simulationCell->setCellMatrix(cellMatrix);

    if(_particles) {
        if(BufferWriteAccess<Point3, access_mode::read_write> positions = _particles->getMutableProperty(Particles::PositionProperty)) {
            for(Point3& p : positions)
                p -= offset;
        }
    }
}

/******************************************************************************
* Finalizes the particle data loaded by a sub-class.
******************************************************************************/
void ParticleImporter::FrameLoader::loadFile()
{
    if(this_task::isCanceled())
        return;

    StandardFrameLoader::loadFile();

    // Automatically generate the 'Velocity Magnitude' property if the 'Velocity' vector property was loaded from the input file.
    computeVelocityMagnitude();

    // Center the simulation cell on the coordinate origin if requested.
    if(_recenterCell)
        recenterSimulationCell();

    // If the file reader did not import any bonds, then discard
    // any existing bonds from a previous load operation.
    if(!_keepExistingTopology) {
        if(!_bonds) setBondCount(0);
        if(!_angles) setAngleCount(0);
        if(!_dihedrals) setDihedralCount(0);
        if(!_impropers) setImproperCount(0);
    }

    // Precompute checksum of Particle Identifier property to speed up
    // the detection of particle reorderings.
    if(_particles) {
        if(const Property* identifiers = _particles->getProperty(Particles::IdentifierProperty)) {
            (void)identifiers->checksum();
        }
    }

#ifdef OVITO_DEBUG
    if(_particles) _particles->verifyIntegrity();
    if(_bonds) _bonds->verifyIntegrity();
    if(_angles) _angles->verifyIntegrity();
    if(_dihedrals) _dihedrals->verifyIntegrity();
    if(_impropers) _impropers->verifyIntegrity();
#endif
}

/******************************************************************************
* Is called when importing multiple files of different formats.
******************************************************************************/
Future<OORef<Pipeline>> ParticleImporter::importFurtherFiles(OORef<Scene> scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode, Pipeline* pipeline)
{
    OVITO_ASSERT(!sourceUrlsAndImporters.empty());
    OORef<ParticleImporter> nextImporter = dynamic_object_cast<ParticleImporter>(sourceUrlsAndImporters.front().second);
    if(this->isTrajectoryFormat() == false && nextImporter && nextImporter->isTrajectoryFormat() == true) {

        // Create a new file source for loading the trajectory.
        OORef<FileSource> fileSource = OORef<FileSource>::create();

        // Concatenate all files from the input list having the same file format into one sequence,
        // which gets handled by the trajectory importer.
        std::vector<QUrl> sourceUrls;
        sourceUrls.push_back(std::move(sourceUrlsAndImporters.front().first));
        auto iter = std::next(sourceUrlsAndImporters.begin());
        if(multiFileImportMode == ImportAsTrajectory) {
            for(; iter != sourceUrlsAndImporters.end(); ++iter) {
                if(iter->second->getOOClass() != nextImporter->getOOClass())
                    break;
                sourceUrls.push_back(std::move(iter->first));
            }
        }
        sourceUrlsAndImporters.erase(sourceUrlsAndImporters.begin(), iter);

        // Set the input file location(s) and importer.
        fileSource->setSource(std::move(sourceUrls), nextImporter, autodetectFileSequences);
        this_task::throwIfCanceled();

        // Create a modifier for injecting the trajectory data into the existing pipeline.
        OORef<LoadTrajectoryModifier> loadTrjMod = OORef<LoadTrajectoryModifier>::create();
        loadTrjMod->setTrajectorySource(std::move(fileSource));
        pipeline->applyModifier(scene->animationSettings()->currentTime(), true, std::move(loadTrjMod));

        if(sourceUrlsAndImporters.empty())
            return Future<OORef<Pipeline>>::createImmediateEmpty();
    }

    return FileSourceImporter::importFurtherFiles(std::move(scene), std::move(sourceUrlsAndImporters), importMode, autodetectFileSequences, multiFileImportMode, pipeline);
}

}   // End of namespace
