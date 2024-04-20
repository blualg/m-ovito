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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleBondMap.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ClusterAnalysisModifier.h"

#include <boost/range/combine.hpp>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ClusterAnalysisModifier);
OVITO_CLASSINFO(ClusterAnalysisModifier, "DisplayName", "Cluster analysis");
OVITO_CLASSINFO(ClusterAnalysisModifier, "Description", "Decompose a particle-based structure into disconnected clusters.");
OVITO_CLASSINFO(ClusterAnalysisModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, neighborMode);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, cutoff);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, sortBySize);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, unwrapParticleCoordinates);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, computeCentersOfMass);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, computeRadiusOfGyration);
DEFINE_PROPERTY_FIELD(ClusterAnalysisModifier, colorParticlesByCluster);
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, neighborMode, "Neighbor mode");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, cutoff, "Cutoff distance");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, sortBySize, "Sort clusters by size");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, unwrapParticleCoordinates, "Unwrap particle coordinates");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, computeCentersOfMass, "Compute centers of mass");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, computeRadiusOfGyration, "Compute radii of gyration");
SET_PROPERTY_FIELD_LABEL(ClusterAnalysisModifier, colorParticlesByCluster, "Color particles by cluster");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ClusterAnalysisModifier, cutoff, WorldParameterUnit, 0);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ClusterAnalysisModifier::ClusterAnalysisModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _cutoff(3.2),
    _onlySelectedParticles(false),
    _sortBySize(false),
    _neighborMode(CutoffRange),
    _unwrapParticleCoordinates(false),
    _computeCentersOfMass(false),
    _computeRadiusOfGyration(false),
    _colorParticlesByCluster(false)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ClusterAnalysisModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void ClusterAnalysisModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ClusterAnalysisModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get the current particle positions.
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    // In interactive mode, do not perform a real computation. Instead, reuse old results if available in the pipeline cache.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {

                particles->tryToAdoptProperties(cachedParticles, {
                    cachedParticles->getProperty(Particles::ClusterProperty),
                    colorParticlesByCluster() ? cachedParticles->getProperty(Particles::ColorProperty) : nullptr,
                    unwrapParticleCoordinates() ? cachedParticles->getProperty(Particles::PositionProperty) : nullptr,
                }, {particles});

                if(unwrapParticleCoordinates()) {
                    if(particles->bonds() && cachedParticles->bonds() && cachedParticles->bonds()->elementCount() == particles->bonds()->elementCount()) {
                        if(const Property* cachedImages = cachedParticles->bonds()->getProperty(Bonds::PeriodicImageProperty)) {
                            particles->makeBondsMutable()->createProperty(cachedImages);
                        }
                    }
                }
            }
            if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("clusters"))) {
                state.addObject(cachedTable);
            }
            // Adopt all global attributes computed by the modifier from the cached state.
            state.adoptAttributesFrom(cachedState, request.modificationNode());
        }
        return std::move(state);
    }

    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);

    // Get simulation cell (optional).
    const SimulationCell* inputCell = state.getObject<SimulationCell>();

    // Get particle selection.
    const Property* selectionProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // If there are bonds, get their periodic image property.
    PropertyPtr periodicImageBondProperty;
    if(unwrapParticleCoordinates() && particles->bonds()) {
        // Create a copy of the input bond PBC vectors so that it is safe to modify them.
        periodicImageBondProperty = ConstPropertyPtr::makeCopy(particles->bonds()->getProperty(Bonds::PeriodicImageProperty));
        // If no PBC vectors are present, create ad-hoc vectors initialized to zero.
        if(!periodicImageBondProperty)
            periodicImageBondProperty = Bonds::OOClass().createStandardProperty(DataBuffer::Initialized, particles->bonds()->elementCount(), Bonds::PeriodicImageProperty);
    }

    // Get particle masses, needed for center-of-mass calculation.
    ConstPropertyPtr masses;
    if(computeCentersOfMass() || computeRadiusOfGyration()) {
        if(const Property* massProperty = particles->getProperty(Particles::MassProperty)) {
            // Directly use per-particle mass information.
            masses = massProperty;
        }
        else if(const Property* typeProperty = particles->getProperty(Particles::TypeProperty)) {
            // Use per-type mass information and generate a per-particle mass array from it.
            std::map<int,FloatType> massMap = ParticleType::typeMassMap(typeProperty);
            // Use the per-type masses only if there is at least one type having a positive mass.
            if(!massMap.empty() && boost::algorithm::any_of(massMap, [](const auto& i) { return i.second > 0; })) {
                PropertyPtr massProperty = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::MassProperty);
                boost::transform(BufferReadAccess<int32_t>(typeProperty), BufferWriteAccess<FloatType, access_mode::discard_write>(massProperty).begin(), [&](int32_t t) {
                    auto iter = massMap.find(t);
                    if(iter != massMap.end()) return iter->second;
                    return FloatType(0);
                });
                masses = std::move(massProperty);
            }
        }

        // Extra check: When per-particle weights are being used, make sure they are not all zero.
        if(masses && masses->size() != 0) {
            if(!selectionProperty) {
                if(!boost::algorithm::any_of(BufferReadAccess<FloatType>(masses), [](FloatType mass) { return mass != 0; }))
                    throw Exception(tr("Cannot compute center of mass or radius of gyration if all particle masses are zero. Please check correctness of per-particle and per-type mass values in input dataset."));
            }
            else {
                if(!boost::algorithm::any_of(boost::combine(BufferReadAccess<FloatType>(masses), BufferReadAccess<SelectionIntType>(selectionProperty)), [](const boost::tuple<FloatType, SelectionIntType>& item) { return item.get<1>() && item.get<0>() != 0; }))
                    throw Exception(tr("Cannot compute center of mass or radius of gyration if all particle masses are zero. Please check correctness of per-particle and per-type mass values in input dataset."));
            }
        }
    }

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    std::unique_ptr<ClusterAnalysisEngine> engine;
    if(neighborMode() == CutoffRange) {
        const Property* bondTopology = (periodicImageBondProperty && particles->bonds()) ? particles->bonds()->getProperty(Bonds::TopologyProperty) : nullptr;
        engine = std::make_unique<CutoffClusterAnalysisEngine>(
            request.modificationNode(),
            posProperty,
            std::move(masses),
            inputCell,
            sortBySize(),
            unwrapParticleCoordinates(),
            colorParticlesByCluster(),
            computeCentersOfMass(),
            computeRadiusOfGyration(),
            selectionProperty,
            std::move(periodicImageBondProperty),
            bondTopology,
            cutoff());
    }
    else if(neighborMode() == Bonding) {
        particles->expectBonds()->verifyIntegrity();
        engine = std::make_unique<BondClusterAnalysisEngine>(
            request.modificationNode(),
            posProperty,
            std::move(masses),
            inputCell,
            sortBySize(),
            unwrapParticleCoordinates(),
            colorParticlesByCluster(),
            computeCentersOfMass(),
            computeRadiusOfGyration(),
            selectionProperty,
            std::move(periodicImageBondProperty),
            particles->expectBondsTopology());
    }
    else {
        throw Exception(tr("Invalid cluster neighbor mode"));
    }

    // Perform the calculation in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            engine = std::move(engine)]() mutable
    {
        // Compute the clustering.
        engine->perform();
        this_task::throwIfCanceled();
        engine->applyResults(state);
        return std::move(state);
    }, true);
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void ClusterAnalysisModifier::ClusterAnalysisEngine::perform()
{
    this_task::setProgressText(tr("Performing cluster analysis"));

    // Initialize.
    particleClusters()->fill<int64_t>(-1);
    std::vector<Point3> centersOfMass;

    // Perform the actual clustering.
    doClustering(centersOfMass);
    this_task::throwIfCanceled();

    // Copy center-of-mass coordinates from local array to output property storage.
    if(_centersOfMass) {
        _centersOfMass->resize(centersOfMass.size(), false);
        std::copy(centersOfMass.cbegin(), centersOfMass.cend(), BufferWriteAccess<Point3, access_mode::discard_write>(_centersOfMass).begin());
    }

    // Compute the radius and tensor of gyration of the clusters.
    if(_radiiOfGyration && _gyrationTensors) {
        _radiiOfGyration->resize(centersOfMass.size(), true);
        _gyrationTensors->resize(centersOfMass.size(), true);
        BufferWriteAccess<FloatType, access_mode::read_write> radiiOfGyration(_radiiOfGyration);
        BufferWriteAccess<FloatType*, access_mode::read_write> gyrationTensors(_gyrationTensors);
        std::vector<FloatType> clusterMass(centersOfMass.size(), 0.0);
        BufferReadAccess<FloatType> particleMassesData(_masses);
        BufferReadAccess<int64_t> particleClusters(this->particleClusters());
        BufferReadAccess<Point3> unwrappedCoordinates(_unwrappedPositions);
        OVITO_ASSERT(unwrappedCoordinates);

        // Visit all input particles again.
        size_t particleCount = positions()->size();
        this_task::setProgressMaximum(particleCount);
        for(size_t particleIndex = 0; particleIndex < particleCount; particleIndex++) {

            // Skip particles that do not belong to any cluster.
            if(particleClusters[particleIndex] == 0)
                continue;

            // Update progress indicator.
            this_task::setProgressValueIntermittent(particleIndex);

            size_t clusterIndex = particleClusters[particleIndex] - 1;

            FloatType mass = particleMassesData ? particleMassesData[particleIndex] : FloatType(1);
            clusterMass[clusterIndex] += mass;

            Vector3 delta = unwrappedCoordinates[particleIndex] - centersOfMass[clusterIndex];
            radiiOfGyration[clusterIndex] += mass * delta.squaredLength();
            gyrationTensors.value(clusterIndex, 0) += mass * delta.x() * delta.x();
            gyrationTensors.value(clusterIndex, 1) += mass * delta.y() * delta.y();
            gyrationTensors.value(clusterIndex, 2) += mass * delta.z() * delta.z();
            gyrationTensors.value(clusterIndex, 3) += mass * delta.x() * delta.y();
            gyrationTensors.value(clusterIndex, 4) += mass * delta.x() * delta.z();
            gyrationTensors.value(clusterIndex, 5) += mass * delta.y() * delta.z();
        }

        auto rg = radiiOfGyration.begin();
        auto gtensor = gyrationTensors.begin();
        for(FloatType M : clusterMass) {
            if(M <= 0) M = 1;
            // Divide by cluster mass and take square root.
            *rg = std::sqrt(*rg / M);
            ++rg;
            // Divide elements of the gyration tensor by cluster mass.
            for(size_t cmpnt = 0; cmpnt < 6; cmpnt++)
                *gtensor++ /= M;
        }
        OVITO_ASSERT(rg == radiiOfGyration.end());
        OVITO_ASSERT(gtensor == gyrationTensors.end());
    }

    // Wrap bonds at periodic cell boundaries after particle coordinates have been unwrapped.
    if(_periodicImageBondProperty && _periodicImageBondProperty->size() == bondTopology()->size()) {
        OVITO_ASSERT(_unwrappedPositions);

        if(!cell() || !cell()->hasPbcCorrected()) {
            // No wrapping of bonds needed if simulation cell is non-periodic.
            _periodicImageBondProperty.reset();
        }
        else {
            const std::array<bool, 3> pbcFlags = cell()->pbcFlagsCorrected();

            // If any particles have been unwrapped by the modifier, update the PBC vectors
            // of the incident bonds accordingly.
            BufferReadAccess<Point3> positionsArray(positions());
            BufferReadAccess<Point3> unwrappedPositionsArray(_unwrappedPositions);
            const AffineTransformation inverseSimCell = cell()->inverseMatrix();
            BufferWriteAccess<Vector3I, access_mode::read_write> pbcArray(_periodicImageBondProperty);
            Vector3I* pbcVec = pbcArray.begin();
            for(const ParticleIndexPair& bond : BufferReadAccess<ParticleIndexPair>(bondTopology())) {
                if((size_t)bond[0] < positionsArray.size() && (size_t)bond[1] < positionsArray.size()) {
                    Vector3 s1 = unwrappedPositionsArray[bond[0]] - positionsArray[bond[0]];
                    Vector3 s2 = unwrappedPositionsArray[bond[1]] - positionsArray[bond[1]];
                    for(size_t dim = 0; dim < 3; dim++) {
                        if(pbcFlags[dim]) {
                            (*pbcVec)[dim] +=
                                    std::lround(inverseSimCell.prodrow(s1, dim)) - std::lround(inverseSimCell.prodrow(s2, dim));
                        }
                    }
                }
                ++pbcVec;
            }
            this_task::throwIfCanceled();
        }
    }

    // Determine cluster sizes.
    _clusterSizes->resize(numClusters(), true);
    BufferWriteAccess<int64_t, access_mode::read_write> clusterSizeArray(_clusterSizes);
    for(auto id : BufferReadAccess<int64_t>(particleClusters())) {
        if(id != 0)
            clusterSizeArray[id-1]++;
    }
    this_task::throwIfCanceled();

    // Create custer ID property.
    _clusterIds->resize(numClusters(), true);
    boost::algorithm::iota_n(BufferWriteAccess<int64_t, access_mode::discard_write>(_clusterIds).begin(), int64_t(1), _clusterIds->size());

    // Sort clusters by size.
    if(_sortBySize && numClusters() != 0) {

        // Determine new cluster ordering.
        std::vector<size_t> mapping(clusterSizeArray.size());
        std::iota(mapping.begin(), mapping.end(), size_t(0));
        std::sort(mapping.begin(), mapping.end(), [&](auto a, auto b) {
            return clusterSizeArray[a] > clusterSizeArray[b];
        });
        std::sort(clusterSizeArray.begin(), clusterSizeArray.end(), std::greater<>());
        setLargestClusterSize(clusterSizeArray[0]);

        // Reorder centers of mass.
        if(_centersOfMass) {
            _centersOfMass->reorderElements(mapping);
        }
        // Reorder radii of gyration.
        if(_radiiOfGyration) {
            _radiiOfGyration->reorderElements(mapping);
        }
        // Reorder gyration tensors.
        if(_gyrationTensors) {
            _gyrationTensors->reorderElements(mapping);
        }

        // Remap cluster IDs of particles.
        std::vector<size_t> inverseMapping(numClusters() + 1);
        inverseMapping[0] = 0;
        for(size_t i = 0; i < numClusters(); i++)
            inverseMapping[mapping[i]+1] = i+1;
        for(auto& id : BufferWriteAccess<int64_t, access_mode::read_write>(particleClusters()))
            id = inverseMapping[id];
    }
}

/******************************************************************************
* Performs the actual clustering algorithm.
******************************************************************************/
void ClusterAnalysisModifier::CutoffClusterAnalysisEngine::doClustering(std::vector<Point3>& centersOfMass)
{
    // Prepare the neighbor finder.
    CutoffNeighborFinder neighborFinder;
    neighborFinder.prepare(cutoff(), positions(), cell(), selection());

    size_t particleCount = positions()->size();
    this_task::setProgressMaximum(particleCount);
    size_t progress = 0;

    BufferWriteAccess<int64_t, access_mode::read_write> particleClusters(this->particleClusters());
    BufferReadAccess<SelectionIntType> selectionData(selection());
    BufferWriteAccess<Point3, access_mode::read_write> unwrappedCoordinates(_unwrappedPositions);
    BufferReadAccess<FloatType> particleMassesData(_masses);

    std::deque<size_t> toProcess;
    for(size_t seedParticleIndex = 0; seedParticleIndex < particleCount; seedParticleIndex++) {

        // Skip unselected particles that are not included in the analysis.
        if(selectionData && !selectionData[seedParticleIndex]) {
            particleClusters[seedParticleIndex] = 0;
            progress++;
            continue;
        }

        // Skip particles that have already been assigned to a cluster.
        if(particleClusters[seedParticleIndex] != -1)
            continue;

        // Start a new cluster.
        setNumClusters(numClusters() + 1);
        int64_t cluster = numClusters();
        particleClusters[seedParticleIndex] = cluster;
        Vector3 centerOfMass = Vector3::Zero();
        FloatType totalWeight = 0;

        // Now recursively iterate over all neighbors of the seed particle and add them to the cluster too.
        OVITO_ASSERT(toProcess.empty());
        toProcess.push_back(seedParticleIndex);

        do {
            this_task::setProgressValueIntermittent(progress++);

            size_t currentParticle = toProcess.front();
            toProcess.pop_front();
            for(CutoffNeighborFinder::Query neighQuery(neighborFinder, currentParticle); !neighQuery.atEnd(); neighQuery.next()) {
                size_t neighborIndex = neighQuery.current();
                if(particleClusters[neighborIndex] == -1) {
                    particleClusters[neighborIndex] = cluster;
                    toProcess.push_back(neighborIndex);
                    if(unwrappedCoordinates) {
                        unwrappedCoordinates[neighborIndex] = unwrappedCoordinates[currentParticle] + neighQuery.delta();
                        FloatType weight = particleMassesData ? particleMassesData[neighborIndex] : FloatType(1);
                        centerOfMass += weight * (unwrappedCoordinates[neighborIndex] - Point3::Origin());
                        totalWeight += weight;
                    }
                }
            }
        }
        while(toProcess.empty() == false);

        if(_centersOfMass || _radiiOfGyration) {
            OVITO_ASSERT(unwrappedCoordinates);
            FloatType weight = particleMassesData ? particleMassesData[seedParticleIndex] : FloatType(1);
            centerOfMass += weight * (unwrappedCoordinates[seedParticleIndex] - Point3::Origin());
            totalWeight += weight;
            if(totalWeight > 0)
                centersOfMass.push_back(Point3::Origin() + (centerOfMass / totalWeight));
            else {
                centersOfMass.push_back(Point3::Origin());
                _hasZeroWeightCluster = true;
            }
        }
    }
}

/******************************************************************************
* Performs the actual clustering algorithm.
******************************************************************************/
void ClusterAnalysisModifier::BondClusterAnalysisEngine::doClustering(std::vector<Point3>& centersOfMass)
{
    size_t particleCount = positions()->size();
    this_task::setProgressMaximum(particleCount);
    size_t progress = 0;

    // Prepare particle bond map.
    ParticleBondMap bondMap(bondTopology());

    BufferWriteAccess<int64_t, access_mode::read_write> particleClusters(this->particleClusters());
    BufferReadAccess<SelectionIntType> selectionData(this->selection());
    BufferReadAccess<ParticleIndexPair> bondTopology(this->bondTopology());
    BufferWriteAccess<Point3, access_mode::read_write> unwrappedCoordinates(_unwrappedPositions);
    BufferReadAccess<FloatType> particleMassesData(_masses);

    std::deque<size_t> toProcess;
    for(size_t seedParticleIndex = 0; seedParticleIndex < particleCount; seedParticleIndex++) {

        // Skip unselected particles that are not included in the analysis.
        if(selectionData && !selectionData[seedParticleIndex]) {
            particleClusters[seedParticleIndex] = 0;
            progress++;
            continue;
        }

        // Skip particles that have already been assigned to a cluster.
        if(particleClusters[seedParticleIndex] != -1)
            continue;

        // Start a new cluster.
        setNumClusters(numClusters() + 1);
        int64_t cluster = numClusters();
        particleClusters[seedParticleIndex] = cluster;
        Vector3 centerOfMass = Vector3::Zero();
        FloatType totalWeight = 0;

        // Now recursively iterate over all neighbors of the seed particle and add them to the cluster too.
        OVITO_ASSERT(toProcess.empty());
        toProcess.push_back(seedParticleIndex);

        do {
            this_task::setProgressValueIntermittent(progress++);

            size_t currentParticle = toProcess.front();
            toProcess.pop_front();

            // Iterate over all bonds of the current particle.
            for(size_t neighborBondIndex : bondMap.bondIndicesOfParticle(currentParticle)) {
                OVITO_ASSERT(bondTopology[neighborBondIndex][0] == currentParticle || bondTopology[neighborBondIndex][1] == currentParticle);
                size_t neighborIndex = bondTopology[neighborBondIndex][0];
                if(neighborIndex == currentParticle)
                    neighborIndex = bondTopology[neighborBondIndex][1];
                if(neighborIndex >= particleCount)
                    continue;
                if(particleClusters[neighborIndex] != -1)
                    continue;
                if(selectionData && !selectionData[neighborIndex])
                    continue;

                particleClusters[neighborIndex] = cluster;
                toProcess.push_back(neighborIndex);

                if(unwrappedCoordinates) {
                    Vector3 delta = unwrappedCoordinates[neighborIndex] - unwrappedCoordinates[currentParticle];
                    if(cell()) delta = cell()->wrapVector(delta);
                    unwrappedCoordinates[neighborIndex] = unwrappedCoordinates[currentParticle] + delta;
                    FloatType weight = particleMassesData ? particleMassesData[neighborIndex] : FloatType(1);
                    centerOfMass += weight * (unwrappedCoordinates[neighborIndex] - Point3::Origin());
                    totalWeight += weight;
                }
            }
        }
        while(toProcess.empty() == false);

        if(_centersOfMass || _radiiOfGyration) {
            OVITO_ASSERT(unwrappedCoordinates);
            FloatType weight = particleMassesData ? particleMassesData[seedParticleIndex] : FloatType(1);
            centerOfMass += weight * (unwrappedCoordinates[seedParticleIndex] - Point3::Origin());
            totalWeight += weight;
            if(totalWeight > 0)
                centersOfMass.push_back(Point3::Origin() + (centerOfMass / totalWeight));
            else {
                centersOfMass.push_back(Point3::Origin());
                _hasZeroWeightCluster = true;
            }
        }
    }
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ClusterAnalysisModifier::ClusterAnalysisEngine::applyResults(PipelineFlowState& state)
{
    Particles* particles = state.expectMutableObject<Particles>();

    // Output the cluster assignment.
    particles->createProperty(particleClusters());

    // Give clusters a random color.
    if(_colorParticlesByCluster) {
        // Assign random colors to clusters.
        std::vector<ColorG> clusterColors(numClusters() + 1);
        std::default_random_engine rng(1);
        std::uniform_real_distribution<FloatType> uniform_dist(0, 1);
        boost::generate(clusterColors, [&]() { return ColorG::fromHSV(static_cast<GraphicsFloatType>(uniform_dist(rng)), 1.0f - static_cast<GraphicsFloatType>(uniform_dist(rng)) * 0.4f, 1.0f - static_cast<GraphicsFloatType>(uniform_dist(rng)) * 0.3f); });
        // Special color for particles not part of any cluster:
        clusterColors[0] = ColorG(0.8f, 0.8f, 0.8f);

        // Assign colors to particles according to the clusters they belong to.
        BufferWriteAccess<ColorG, access_mode::discard_write> colorsArray = particles->createProperty(Particles::ColorProperty);
        boost::transform(BufferReadAccess<int64_t>(particleClusters()), colorsArray.begin(), [&](int64_t cluster) {
            OVITO_ASSERT(cluster >= 0 && (size_t)cluster < clusterColors.size());
            return clusterColors[cluster];
        });
    }

    // Output unwrapped particle coordinates.
    if(_unwrapParticleCoordinates && _unwrappedPositions) {
        particles->createProperty(_unwrappedPositions);

        // Correct the PBC flags of the bonds if particles have been unwrapped.
        if(particles->bonds() && _periodicImageBondProperty && _periodicImageBondProperty->size() == particles->bonds()->elementCount()) {
            particles->makeBondsMutable()->createProperty(_periodicImageBondProperty);
        }
    }

    state.addAttribute(QStringLiteral("ClusterAnalysis.cluster_count"), QVariant::fromValue(numClusters()), createdByNode());
    if(_sortBySize)
        state.addAttribute(QStringLiteral("ClusterAnalysis.largest_size"), QVariant::fromValue(largestClusterSize()), createdByNode());

    // Output a data table with the cluster list.
    DataTable* table = state.createObject<DataTable>(QStringLiteral("clusters"), createdByNode(), DataTable::Scatter, tr("Cluster list"), _clusterSizes, _clusterIds);

    // Output centers of mass.
    if(_centersOfMass)
        table->createProperty(_centersOfMass);

    // Output radii of gyration.
    if(_radiiOfGyration)
        table->createProperty(_radiiOfGyration);

    // Output gyration tensors.
    if(_gyrationTensors)
        table->createProperty(_gyrationTensors);

    PipelineStatus status(
        tr("Found %1 cluster(s).").arg(numClusters()),
        numClusters() == 1 ? tr("1 cluster") : tr("%1 clusters").arg(numClusters()));

    if(_hasZeroWeightCluster) {
        status.setType(PipelineStatus::Warning);
        status.setText(status.text() + tr("\nCould not compute center of mass or radius of gyration of some clusters, because their total mass is zero. "
            "Please make sure particles or particle types have valid masses assigned."));
    }
    state.setStatus(std::move(status));
}

}   // End of namespace
