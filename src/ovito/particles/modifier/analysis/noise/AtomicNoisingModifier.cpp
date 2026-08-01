////////////////////////////////////////////////////////////////////////////////////////
//
//  Atomic noising modifier for m-ovito.
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "AtomicNoisingModifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <unordered_map>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AtomicNoisingModifier);
OVITO_CLASSINFO(AtomicNoisingModifier, "Description", "Add controlled Gaussian positional noise to particles.");
OVITO_CLASSINFO(AtomicNoisingModifier, "DisplayName", "Atomic noising");
OVITO_CLASSINFO(AtomicNoisingModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, scaleMode);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, noiseTensorMode);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, amplitude);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, amplitudeY);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, amplitudeZ);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, nearestNeighborDistance);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, sigmaSamplingMode);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, particleCouplingMode);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, spatialCorrelation);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, correlationLength);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, randomSeed);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, frameDependentSeed);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, onlySelected);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, wrapIntoCell);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, preserveCenterOfMass);
DEFINE_PROPERTY_FIELD(AtomicNoisingModifier, writeNoiseProperties);
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, scaleMode, "Noise scale");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, noiseTensorMode, "Noise tensor");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, amplitude, "Amplitude X / isotropic");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, amplitudeY, "Amplitude Y");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, amplitudeZ, "Amplitude Z");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, nearestNeighborDistance, "Nearest-neighbor distance");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, sigmaSamplingMode, "Sigma sampling");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, particleCouplingMode, "Particle coupling");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, spatialCorrelation, "Spatially correlate noise");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, correlationLength, "Correlation length");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, randomSeed, "Random seed");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, frameDependentSeed, "Frame-dependent seed");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, onlySelected, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, wrapIntoCell, "Wrap into periodic cell");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, preserveCenterOfMass, "Preserve center of mass");
SET_PROPERTY_FIELD_LABEL(AtomicNoisingModifier, writeNoiseProperties, "Write noise properties");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, amplitude, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, amplitudeY, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, amplitudeZ, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, nearestNeighborDistance, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, correlationLength, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AtomicNoisingModifier, randomSeed, IntegerParameterUnit, 0);

namespace {

constexpr int NeighborCountForScaleEstimate = 1;
constexpr double SqrtThree = 1.7320508075688772935;

QString scaleModeName(AtomicNoisingModifier::ScaleMode mode)
{
    switch(mode) {
    case AtomicNoisingModifier::AbsoluteCoordinateSigma:
        return AtomicNoisingModifier::tr("absolute coordinate sigma");
    case AtomicNoisingModifier::FractionOfNearestNeighbor:
        return AtomicNoisingModifier::tr("fraction of nearest-neighbor distance");
    case AtomicNoisingModifier::LindemannRmsFraction:
        return AtomicNoisingModifier::tr("Lindemann RMS fraction");
    }
    return {};
}

QString sigmaSamplingModeName(AtomicNoisingModifier::SigmaSamplingMode mode)
{
    switch(mode) {
    case AtomicNoisingModifier::FixedSigma:
        return AtomicNoisingModifier::tr("fixed sigma");
    case AtomicNoisingModifier::UniformZeroToSigma:
        return AtomicNoisingModifier::tr("uniform 0..max sigma");
    }
    return {};
}

QString noiseTensorModeName(AtomicNoisingModifier::NoiseTensorMode mode)
{
    switch(mode) {
    case AtomicNoisingModifier::IsotropicNoise:
        return AtomicNoisingModifier::tr("isotropic");
    case AtomicNoisingModifier::CartesianDiagonalNoise:
        return AtomicNoisingModifier::tr("diagonal XYZ");
    case AtomicNoisingModifier::CellAxisDiagonalNoise:
        return AtomicNoisingModifier::tr("diagonal cell axes");
    }
    return {};
}

QString particleCouplingModeName(AtomicNoisingModifier::ParticleCouplingMode mode)
{
    switch(mode) {
    case AtomicNoisingModifier::IndependentParticles:
        return AtomicNoisingModifier::tr("independent particles");
    case AtomicNoisingModifier::RigidMolecules:
        return AtomicNoisingModifier::tr("rigid molecules");
    }
    return {};
}

struct MoleculeGroup
{
    IdentifierIntType moleculeId = 0;
    std::vector<size_t> indices;
    bool selected = false;
};

struct NoiseEntity
{
    IdentifierIntType moleculeId = 0;
    Point3 center = Point3::Origin();
    std::vector<size_t> indices;
    bool selected = false;
};

double estimateNearestNeighborDistance(const Particles* particles,
                                       const SimulationCell* simulationCell,
                                       const Property* selectionProperty)
{
    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<SelectionIntType> selection(selectionProperty);

    NearestNeighborFinder neighborFinder(NeighborCountForScaleEstimate, positions, simulationCell, {});
    NearestNeighborFinder::Query<NeighborCountForScaleEstimate> query(neighborFinder);

    double distanceSum = 0.0;
    size_t distanceCount = 0;
    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
        this_task::throwIfCanceled();
        if(selection && !selection[particleIndex])
            continue;

        query.findNeighbors(particleIndex);
        for(const NearestNeighborFinder::Neighbor& neighbor : query.results()) {
            distanceSum += std::sqrt(static_cast<double>(neighbor.distanceSq));
            distanceCount++;
        }
    }

    if(distanceCount == 0)
        throw Exception(AtomicNoisingModifier::tr("Unable to estimate a nearest-neighbor distance from the selected particles."));
    return distanceSum / static_cast<double>(distanceCount);
}

uint64_t effectiveSeed(int baseSeed, bool frameDependentSeed, AnimationTime time)
{
    uint64_t seed = static_cast<uint64_t>(std::max(baseSeed, 0));
    if(frameDependentSeed) {
        const uint64_t frameKey = static_cast<uint64_t>(time.ticks());
        seed ^= frameKey + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }
    return seed;
}

double computeCoordinateSigmaComponent(double amplitude,
                                       double scaleDistance,
                                       AtomicNoisingModifier::ScaleMode scaleMode)
{
    switch(scaleMode) {
    case AtomicNoisingModifier::AbsoluteCoordinateSigma:
        return amplitude;
    case AtomicNoisingModifier::FractionOfNearestNeighbor:
        return amplitude * scaleDistance;
    case AtomicNoisingModifier::LindemannRmsFraction:
        return amplitude * scaleDistance / SqrtThree;
    }
    return 0.0;
}

Vector3 computeCoordinateSigmaVector(double amplitudeX,
                                     double amplitudeY,
                                     double amplitudeZ,
                                     double scaleDistance,
                                     AtomicNoisingModifier::ScaleMode scaleMode,
                                     AtomicNoisingModifier::NoiseTensorMode noiseTensorMode)
{
    if(noiseTensorMode == AtomicNoisingModifier::IsotropicNoise) {
        const FloatType sigma = static_cast<FloatType>(computeCoordinateSigmaComponent(amplitudeX, scaleDistance, scaleMode));
        return Vector3(sigma, sigma, sigma);
    }

    return Vector3(
        static_cast<FloatType>(computeCoordinateSigmaComponent(amplitudeX, scaleDistance, scaleMode)),
        static_cast<FloatType>(computeCoordinateSigmaComponent(amplitudeY, scaleDistance, scaleMode)),
        static_cast<FloatType>(computeCoordinateSigmaComponent(amplitudeZ, scaleDistance, scaleMode)));
}

Vector3 normalizedCellVector(const SimulationCell* simulationCell, int column)
{
    Vector3 axis = simulationCell->cellMatrix().column(column);
    const FloatType length = axis.length();
    if(length <= FloatType(0))
        throw Exception(AtomicNoisingModifier::tr("Cannot use cell-axis noise with a degenerate simulation cell vector."));
    return axis / length;
}

Vector3 sampleNoiseVector(const Vector3& sigmaVector,
                          AtomicNoisingModifier::NoiseTensorMode noiseTensorMode,
                          const SimulationCell* simulationCell,
                          std::normal_distribution<double>& normalDistribution,
                          std::mt19937_64& rng)
{
    const FloatType x = static_cast<FloatType>(sigmaVector.x() * normalDistribution(rng));
    const FloatType y = static_cast<FloatType>(sigmaVector.y() * normalDistribution(rng));
    const FloatType z = static_cast<FloatType>(sigmaVector.z() * normalDistribution(rng));

    if(noiseTensorMode == AtomicNoisingModifier::CellAxisDiagonalNoise) {
        return x * normalizedCellVector(simulationCell, 0)
             + y * normalizedCellVector(simulationCell, 1)
             + z * normalizedCellVector(simulationCell, 2);
    }
    return Vector3(x, y, z);
}

Point3 moleculeCenter(const MoleculeGroup& group,
                      const std::vector<Point3>& originalPositions,
                      const SimulationCell* simulationCell)
{
    if(group.indices.empty())
        return Point3::Origin();

    const Point3& referencePosition = originalPositions[group.indices.front()];
    Vector3 offsetSum = Vector3::Zero();
    for(size_t particleIndex : group.indices) {
        Vector3 delta = originalPositions[particleIndex] - referencePosition;
        if(simulationCell && simulationCell->hasPbcCorrected())
            delta = simulationCell->wrapVector(delta);
        offsetSum += delta;
    }

    return referencePosition + offsetSum / static_cast<FloatType>(group.indices.size());
}

std::vector<MoleculeGroup> buildMoleculeGroups(const Particles* particles,
                                               const Property* moleculeProperty,
                                               const BufferReadAccess<SelectionIntType>& selection,
                                               bool selectedOnly)
{
    BufferReadAccess<IdentifierIntType> moleculeIds(moleculeProperty);
    std::unordered_map<IdentifierIntType, size_t> groupLookup;
    groupLookup.reserve(moleculeIds.size());
    std::vector<MoleculeGroup> groups;
    groups.reserve(moleculeIds.size());

    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
        const IdentifierIntType moleculeId = moleculeIds[particleIndex];
        auto [iter, inserted] = groupLookup.try_emplace(moleculeId, groups.size());
        if(inserted) {
            groups.push_back(MoleculeGroup{});
            groups.back().moleculeId = moleculeId;
        }

        MoleculeGroup& group = groups[iter->second];
        group.indices.push_back(particleIndex);
        if(!selectedOnly || (selection && selection[particleIndex]))
            group.selected = true;
    }

    return groups;
}

std::vector<NoiseEntity> buildNoiseEntities(const Particles* particles,
                                            const std::vector<Point3>& originalPositions,
                                            const Property* moleculeProperty,
                                            const SimulationCell* simulationCell,
                                            const BufferReadAccess<SelectionIntType>& selection,
                                            bool selectedOnly,
                                            AtomicNoisingModifier::ParticleCouplingMode particleCouplingMode)
{
    std::vector<NoiseEntity> entities;

    if(particleCouplingMode == AtomicNoisingModifier::RigidMolecules) {
        const std::vector<MoleculeGroup> moleculeGroups =
            buildMoleculeGroups(particles, moleculeProperty, selection, selectedOnly);
        entities.reserve(moleculeGroups.size());
        for(const MoleculeGroup& group : moleculeGroups) {
            NoiseEntity entity;
            entity.moleculeId = group.moleculeId;
            entity.center = moleculeCenter(group, originalPositions, simulationCell);
            entity.indices = group.indices;
            entity.selected = !selectedOnly || group.selected;
            entities.push_back(std::move(entity));
        }
    }
    else {
        entities.reserve(particles->elementCount());
        for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
            NoiseEntity entity;
            entity.center = originalPositions[particleIndex];
            entity.indices.push_back(particleIndex);
            entity.selected = !selectedOnly || (selection && selection[particleIndex]);
            entities.push_back(std::move(entity));
        }
    }

    return entities;
}

void spatiallyCorrelateEntityDisplacements(std::vector<Vector3>& entityDisplacements,
                                           const std::vector<NoiseEntity>& entities,
                                           const SimulationCell* simulationCell,
                                           double correlationLength,
                                           TaskProgress& progress)
{
    if(!(correlationLength > 0.0))
        throw Exception(AtomicNoisingModifier::tr("The spatial correlation length must be positive."));
    if(!simulationCell || simulationCell->isDegenerate())
        throw Exception(AtomicNoisingModifier::tr("Spatially correlated noise requires a non-degenerate simulation cell."));

    const FloatType cutoff = static_cast<FloatType>(3.0 * correlationLength);
    const double twoXiSquared = 2.0 * correlationLength * correlationLength;

    PropertyPtr centerProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, entities.size(), Particles::PositionProperty);
    PropertyPtr selectionProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, entities.size(), Particles::SelectionProperty);
    {
        BufferWriteAccess<Point3, access_mode::discard_write> centers(centerProperty);
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> selected(selectionProperty);
        for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            centers[entityIndex] = entities[entityIndex].center;
            selected[entityIndex] = entities[entityIndex].selected ? 1 : 0;
        }
    }

    BufferReadAccess<Point3> centers(centerProperty);
    BufferReadAccess<SelectionIntType> selected(selectionProperty);
    CutoffNeighborFinder neighborFinder(cutoff, centers, simulationCell, selected);

    std::vector<Vector3> smoothed(entityDisplacements.size(), Vector3::Zero());
    progress.setText(AtomicNoisingModifier::tr("Spatially correlating atomic noise"));
    progress.setMaximum(static_cast<qlonglong>(std::max<size_t>(entities.size(), 1)));

    for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
        this_task::throwIfCanceled();
        progress.setValueIntermittent(static_cast<qlonglong>(entityIndex));

        if(!entities[entityIndex].selected)
            continue;

        Vector3 weightedDisplacement = entityDisplacements[entityIndex];
        double weightSum = 1.0;
        for(CutoffNeighborFinder::Query query(neighborFinder, entityIndex); !query.atEnd(); query.next()) {
            const double weight = std::exp(-static_cast<double>(query.distanceSquared()) / twoXiSquared);
            weightedDisplacement += entityDisplacements[query.current()] * static_cast<FloatType>(weight);
            weightSum += weight;
        }

        smoothed[entityIndex] = weightedDisplacement / static_cast<FloatType>(weightSum);
    }

    entityDisplacements.swap(smoothed);
}

void renormalizeEntityDisplacements(std::vector<Vector3>& entityDisplacements,
                                    const std::vector<NoiseEntity>& entities,
                                    const Vector3& coordinateSigma)
{
    const double targetRms = std::sqrt(static_cast<double>(coordinateSigma.squaredLength()));
    if(!(targetRms > 0.0))
        return;

    double squaredDisplacementSum = 0.0;
    size_t particleCount = 0;
    for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
        if(!entities[entityIndex].selected)
            continue;
        squaredDisplacementSum += static_cast<double>(entityDisplacements[entityIndex].squaredLength()) *
                                  static_cast<double>(entities[entityIndex].indices.size());
        particleCount += entities[entityIndex].indices.size();
    }

    if(particleCount == 0)
        return;

    const double currentRms = std::sqrt(squaredDisplacementSum / static_cast<double>(particleCount));
    if(!(currentRms > 0.0))
        return;

    const FloatType scale = static_cast<FloatType>(targetRms / currentRms);
    for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
        if(entities[entityIndex].selected)
            entityDisplacements[entityIndex] *= scale;
    }
}

void writeVectorPropertyComponent(BufferWriteAccess<FloatType*, access_mode::discard_write>& access,
                                  size_t particleIndex,
                                  const Vector3& vector)
{
    access.set(particleIndex, 0, vector.x());
    access.set(particleIndex, 1, vector.y());
    access.set(particleIndex, 2, vector.z());
}

void writePointPropertyComponent(BufferWriteAccess<FloatType*, access_mode::discard_write>& access,
                                 size_t particleIndex,
                                 const Point3& point)
{
    access.set(particleIndex, 0, point.x());
    access.set(particleIndex, 1, point.y());
    access.set(particleIndex, 2, point.z());
}

}  // namespace

bool AtomicNoisingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void AtomicNoisingModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                TimeInterval& validityInterval) const
{
    evaluationTypes = request.interactiveMode() ? PipelineEvaluationResult::EvaluationType::Interactive
                                                : PipelineEvaluationResult::EvaluationType::Noninteractive;
    if(frameDependentSeed())
        validityInterval.intersect(request.time());
}

QVariant AtomicNoisingModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    return tr("%1, amplitude %2").arg(scaleModeName(scaleMode())).arg(amplitude());
}

Future<PipelineFlowState> AtomicNoisingModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                  PipelineFlowState&& state)
{
    if(amplitude() < 0)
        throw Exception(tr("The noise amplitude must be non-negative."));
    if(amplitudeY() < 0 || amplitudeZ() < 0)
        throw Exception(tr("The anisotropic noise amplitudes must be non-negative."));
    if(nearestNeighborDistance() < 0)
        throw Exception(tr("The nearest-neighbor distance must be zero or positive."));
    if(spatialCorrelation() && correlationLength() <= 0)
        throw Exception(tr("The spatial correlation length must be positive."));

    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();
    const Property* positionProperty = particles->expectProperty(Particles::PositionProperty);
    const Property* selectionProperty = onlySelected() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;
    const Property* moleculeProperty = particleCouplingMode() == RigidMolecules
                                           ? particles->expectProperty(Particles::MoleculeProperty)
                                           : nullptr;
    const SimulationCell* simulationCell = state.getObject<SimulationCell>();

    if(scaleMode() != AbsoluteCoordinateSigma || wrapIntoCell() || noiseTensorMode() == CellAxisDiagonalNoise || spatialCorrelation()) {
        if(!simulationCell)
            throw Exception(tr("This noising mode requires a simulation cell."));
        if(simulationCell->isDegenerate())
            throw Exception(tr("This noising mode requires a non-degenerate simulation cell."));
    }

    return asyncLaunch([state = std::move(state),
                        particles,
                        positionProperty,
                        selectionProperty,
                        moleculeProperty,
                        simulationCell,
                        scaleMode = scaleMode(),
                        noiseTensorMode = noiseTensorMode(),
                        requestedAmplitude = static_cast<double>(amplitude()),
                        requestedAmplitudeY = static_cast<double>(amplitudeY()),
                        requestedAmplitudeZ = static_cast<double>(amplitudeZ()),
                        requestedScaleDistance = static_cast<double>(nearestNeighborDistance()),
                        sigmaSamplingMode = sigmaSamplingMode(),
                        particleCouplingMode = particleCouplingMode(),
                        correlateSpatially = spatialCorrelation(),
                        spatialCorrelationLength = static_cast<double>(correlationLength()),
                        seedValue = effectiveSeed(randomSeed(), frameDependentSeed(), request.time()),
                        selectedOnly = onlySelected(),
                        wrap = wrapIntoCell(),
                        preserveCom = preserveCenterOfMass(),
                        writeProperties = writeNoiseProperties(),
                        createdByNode = request.modificationNodeWeak()]() mutable {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Applying atomic noise"));

        const size_t particleCount = particles->elementCount();
        if(particleCount == 0)
            throw Exception(tr("The input contains no particles."));

        BufferReadAccess<SelectionIntType> selection(selectionProperty);
        if(selectedOnly && selectionProperty->nonzeroCount() == 0) {
            state.combineStatus(PipelineStatus::Warning, tr("No selected particles; atomic noising was skipped."));
            return std::move(state);
        }

        double scaleDistance = requestedScaleDistance;
        if(scaleMode != AbsoluteCoordinateSigma && !(scaleDistance > 0.0))
            scaleDistance = estimateNearestNeighborDistance(particles, simulationCell, selectionProperty);

        Vector3 coordinateSigmaMax = computeCoordinateSigmaVector(requestedAmplitude,
                                                                 requestedAmplitudeY,
                                                                 requestedAmplitudeZ,
                                                                 scaleDistance,
                                                                 scaleMode,
                                                                 noiseTensorMode);
        if(!std::isfinite(static_cast<double>(coordinateSigmaMax.x())) ||
           !std::isfinite(static_cast<double>(coordinateSigmaMax.y())) ||
           !std::isfinite(static_cast<double>(coordinateSigmaMax.z())))
            throw Exception(tr("The computed coordinate sigma is invalid."));

        std::mt19937_64 rng(seedValue);
        std::normal_distribution<double> normalDistribution(0.0, 1.0);
        std::uniform_real_distribution<double> sigmaScaleDistribution(0.0, 1.0);
        const double sigmaScale = sigmaSamplingMode == UniformZeroToSigma
                                      ? sigmaScaleDistribution(rng)
                                      : 1.0;
        const Vector3 coordinateSigma = coordinateSigmaMax * static_cast<FloatType>(sigmaScale);

        BufferReadAccess<Point3> inputPositions(positionProperty);
        std::vector<Point3> originalPositions(particleCount);
        std::copy(inputPositions.begin(), inputPositions.end(), originalPositions.begin());
        inputPositions.reset();

        std::vector<NoiseEntity> entities = buildNoiseEntities(particles,
                                                               originalPositions,
                                                               moleculeProperty,
                                                               simulationCell,
                                                               selection,
                                                               selectedOnly,
                                                               particleCouplingMode);
        std::vector<Vector3> entityDisplacements(entities.size(), Vector3::Zero());
        size_t noisedParticleCount = 0;
        size_t noisedMoleculeCount = 0;

        progress.setMaximum(static_cast<qlonglong>(std::max<size_t>(entities.size(), 1)));
        for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            this_task::throwIfCanceled();
            progress.setValueIntermittent(static_cast<qlonglong>(entityIndex));

            if(!entities[entityIndex].selected)
                continue;

            entityDisplacements[entityIndex] = sampleNoiseVector(coordinateSigma,
                                                                 noiseTensorMode,
                                                                 simulationCell,
                                                                 normalDistribution,
                                                                 rng);
            noisedParticleCount += entities[entityIndex].indices.size();
            if(particleCouplingMode == RigidMolecules)
                noisedMoleculeCount++;
        }

        if(noisedParticleCount == 0) {
            state.combineStatus(PipelineStatus::Warning, tr("No particles were eligible for atomic noising."));
            return std::move(state);
        }

        if(correlateSpatially) {
            spatiallyCorrelateEntityDisplacements(entityDisplacements,
                                                  entities,
                                                  simulationCell,
                                                  spatialCorrelationLength,
                                                  progress);
            renormalizeEntityDisplacements(entityDisplacements, entities, coordinateSigma);
        }

        std::vector<Vector3> displacements(particleCount, Vector3::Zero());
        Vector3 meanDisplacement = Vector3::Zero();
        for(size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            if(!entities[entityIndex].selected)
                continue;
            for(size_t particleIndex : entities[entityIndex].indices) {
                displacements[particleIndex] = entityDisplacements[entityIndex];
                meanDisplacement += entityDisplacements[entityIndex];
            }
        }

        if(preserveCom) {
            meanDisplacement /= static_cast<FloatType>(noisedParticleCount);
            for(Vector3& displacement : displacements) {
                if(displacement != Vector3::Zero())
                    displacement -= meanDisplacement;
            }
        }
        else {
            meanDisplacement = Vector3::Zero();
        }

        Property* displacementProperty = nullptr;
        Property* magnitudeProperty = nullptr;
        Property* originalPositionProperty = nullptr;
        if(writeProperties) {
            displacementProperty = particles->createProperty(DataBuffer::Initialized,
                                                             QStringLiteral("Noise Displacement"),
                                                             Property::FloatDefault,
                                                             3,
                                                             QStringList{tr("X"), tr("Y"), tr("Z")});
            magnitudeProperty = particles->createProperty(DataBuffer::Initialized,
                                                          QStringLiteral("Noise Magnitude"),
                                                          Property::FloatDefault);
            originalPositionProperty = particles->createProperty(DataBuffer::Initialized,
                                                                 QStringLiteral("Position Before Noising"),
                                                                 Property::FloatDefault,
                                                                 3,
                                                                 QStringList{tr("X"), tr("Y"), tr("Z")});
        }

        BufferWriteAccess<Point3, access_mode::read_write> outputPositions =
            particles->expectMutableProperty(Particles::PositionProperty);
        BufferWriteAccess<FloatType*, access_mode::discard_write> displacementAccess(displacementProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> magnitudeAccess(magnitudeProperty);
        BufferWriteAccess<FloatType*, access_mode::discard_write> originalPositionAccess(originalPositionProperty);

        double squaredDisplacementSum = 0.0;
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            this_task::throwIfCanceled();
            const Point3& originalPosition = originalPositions[particleIndex];
            const Vector3 displacement = displacements[particleIndex];
            const double squaredLength = static_cast<double>(displacement.squaredLength());
            const double magnitude = std::sqrt(squaredLength);

            if(displacement != Vector3::Zero()) {
                Point3 noisyPosition = originalPosition + displacement;
                if(wrap && simulationCell && simulationCell->hasPbcCorrected())
                    noisyPosition = simulationCell->wrapPoint(noisyPosition);
                outputPositions[particleIndex] = noisyPosition;
                squaredDisplacementSum += squaredLength;
            }

            if(writeProperties) {
                writeVectorPropertyComponent(displacementAccess, particleIndex, displacement);
                magnitudeAccess[particleIndex] = static_cast<FloatType>(magnitude);
                writePointPropertyComponent(originalPositionAccess, particleIndex, originalPosition);
            }
        }

        const double rmsDisplacement = noisedParticleCount > 0
                                           ? std::sqrt(squaredDisplacementSum / static_cast<double>(noisedParticleCount))
                                           : 0.0;
        const double coordinateSigmaRms = std::sqrt(
            (static_cast<double>(coordinateSigma.x()) * static_cast<double>(coordinateSigma.x()) +
             static_cast<double>(coordinateSigma.y()) * static_cast<double>(coordinateSigma.y()) +
             static_cast<double>(coordinateSigma.z()) * static_cast<double>(coordinateSigma.z())) / 3.0);

        state.setAttribute(QStringLiteral("AtomicNoising.scale_mode"), scaleModeName(scaleMode), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.noise_tensor"), noiseTensorModeName(noiseTensorMode), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.sigma_sampling"), sigmaSamplingModeName(sigmaSamplingMode), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.particle_coupling"), particleCouplingModeName(particleCouplingMode), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.spatial_correlation"), correlateSpatially, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.correlation_length"), spatialCorrelationLength, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.amplitude"), requestedAmplitude, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.amplitude_y"), requestedAmplitudeY, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.amplitude_z"), requestedAmplitudeZ, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.scale_distance"), scaleDistance, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.coordinate_sigma"), coordinateSigmaRms, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.coordinate_sigma_x"), coordinateSigma.x(), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.coordinate_sigma_y"), coordinateSigma.y(), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.coordinate_sigma_z"), coordinateSigma.z(), createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.rms_displacement"), rmsDisplacement, createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.particle_count"),
                           QVariant::fromValue(static_cast<qlonglong>(noisedParticleCount)),
                           createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.molecule_count"),
                           QVariant::fromValue(static_cast<qlonglong>(noisedMoleculeCount)),
                           createdByNode);
        state.setAttribute(QStringLiteral("AtomicNoising.seed"),
                           QVariant::fromValue(static_cast<qlonglong>(seedValue & 0x7fffffffffffffffULL)),
                           createdByNode);

        state.combineStatus(PipelineStatus::Success,
                            tr("Atomic noising complete: %1 particles displaced; sigma (%2, %3, %4); RMS displacement %5%6.")
                                .arg(noisedParticleCount)
                                .arg(coordinateSigma.x(), 0, 'g', 6)
                                .arg(coordinateSigma.y(), 0, 'g', 6)
                                .arg(coordinateSigma.z(), 0, 'g', 6)
                                .arg(rmsDisplacement, 0, 'g', 6)
                                .arg(correlateSpatially ? tr("; spatial correlation length %1").arg(spatialCorrelationLength, 0, 'g', 6) : QString()));
        return std::move(state);
    });
}

}  // namespace Ovito
