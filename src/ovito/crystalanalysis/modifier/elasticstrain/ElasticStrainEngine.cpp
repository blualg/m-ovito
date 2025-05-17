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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/ClusterGraph.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include "ElasticStrainEngine.h"
#include "ElasticStrainModifier.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ElasticStrainEngine::ElasticStrainEngine(
        PropertyPtr structures,
        size_t particleCount,
        int inputCrystalStructure,
        std::vector<Matrix3> preferredCrystalOrientations,
        bool calculateDeformationGradients,
        bool calculateStrainTensors,
        FloatType latticeConstant,
        FloatType caRatio,
        bool pushStrainTensorsForward) :
    StructureIdentificationModifier::Algorithm(std::move(structures)),
    _inputCrystalStructure(inputCrystalStructure),
    _latticeConstant(latticeConstant),
    _pushStrainTensorsForward(pushStrainTensorsForward),
    _preferredCrystalOrientations(std::move(preferredCrystalOrientations)),
    _volumetricStrains(Particles::OOClass().createUserProperty(DataBuffer::Uninitialized, particleCount, DataBuffer::FloatDefault, 1, QStringLiteral("Volumetric Strain"))),
    _strainTensors(calculateStrainTensors ? Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized,particleCount, Particles::ElasticStrainTensorProperty) : nullptr),
    _deformationGradients(calculateDeformationGradients ? Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particleCount, Particles::ElasticDeformationGradientProperty) : nullptr)
{
    if(inputCrystalStructure == StructureAnalysis::LATTICE_FCC || inputCrystalStructure == StructureAnalysis::LATTICE_BCC || inputCrystalStructure == StructureAnalysis::LATTICE_CUBIC_DIAMOND) {
        // Cubic crystal structures always have a c/a ratio of one.
        _axialScaling = 1;
    }
    else {
        // Convert to internal units.
        _latticeConstant *= std::sqrt(2.0);
        _axialScaling = caRatio / std::sqrt(8.0/3.0);
    }
}

/******************************************************************************
* Performs the actual analysis.
******************************************************************************/
void ElasticStrainEngine::identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection)
{
    if(!simulationCell || simulationCell->is2D())
        throw Exception(ElasticStrainModifier::tr("The elastic strain calculation requires a 3d simulation cell."));

    TaskProgress progress(this_task::ui());
    progress.setText(ElasticStrainModifier::tr("Calculating elastic strain tensors"));

    const Property* positions = particles->expectProperty(Particles::PositionProperty);
    _structureAnalysis.emplace(positions, simulationCell, (StructureAnalysis::LatticeStructureType)_inputCrystalStructure, selection, clusterGraph(), structures(), std::move(_preferredCrystalOrientations));
    setAtomClusters(_structureAnalysis->atomClusters());

    progress.beginSubSteps({ 35, 6, 1, 1, 20 });
    _structureAnalysis->identifyStructures(progress, simulationCell);

    progress.nextSubStep();
    _structureAnalysis->buildClusters(progress);

    progress.nextSubStep();
    _structureAnalysis->connectClusters(progress);

    progress.nextSubStep();
    _structureAnalysis->formSuperClusters();

    progress.nextSubStep();

    BufferReadAccess<Point3> positionsArray(positions);
    BufferWriteAccess<Matrix3, access_mode::discard_write> deformationGradientsArray(deformationGradients());
    BufferWriteAccess<SymmetricTensor2, access_mode::discard_write> strainTensorsArray(strainTensors());
    BufferWriteAccess<FloatType, access_mode::discard_write> volumetricStrainsArray(volumetricStrains());

    parallelFor(positions->size(), 1024, progress, [&](size_t particleIndex) {

        Cluster* localCluster = _structureAnalysis->atomCluster(particleIndex);
        if(localCluster->id != 0) {

            // The shape of the ideal unit cell.
            Matrix3 idealUnitCellTM(_latticeConstant, 0, 0,
                                    0, _latticeConstant, 0,
                                    0, 0, _latticeConstant * _axialScaling);

            // If the cluster is a defect (stacking fault), find the parent crystal cluster.
            Cluster* parentCluster = nullptr;
            if(localCluster->parentTransition != nullptr) {
                parentCluster = localCluster->parentTransition->cluster2;
                idealUnitCellTM = idealUnitCellTM * localCluster->parentTransition->tm;
            }
            else if(localCluster->structure == _inputCrystalStructure) {
                parentCluster = localCluster;
            }

            if(parentCluster != nullptr) {
                OVITO_ASSERT(parentCluster->structure == _inputCrystalStructure);

                // For calculating the cluster orientation.
                Matrix_3<double> orientationV = Matrix_3<double>::Zero();
                Matrix_3<double> orientationW = Matrix_3<double>::Zero();

                int numneigh = _structureAnalysis->numberOfNeighbors(particleIndex);
                for(int n = 0; n < numneigh; n++) {
                    int neighborAtomIndex = _structureAnalysis->getNeighbor(particleIndex, n);
                    // Add vector pair to matrices for computing the elastic deformation gradient.
                    Vector3 latticeVector = idealUnitCellTM * _structureAnalysis->neighborLatticeVector(particleIndex, n);
                    Vector3 spatialVector = positionsArray[neighborAtomIndex] - positionsArray[particleIndex];
                    if(simulationCell)
                        spatialVector = simulationCell->wrapVector(spatialVector);
                    for(size_t i = 0; i < 3; i++) {
                        for(size_t j = 0; j < 3; j++) {
                            orientationV(i,j) += (double)(latticeVector[j] * latticeVector[i]);
                            orientationW(i,j) += (double)(latticeVector[j] * spatialVector[i]);
                        }
                    }
                }

                // Calculate deformation gradient tensor.
                Matrix_3<double> elasticF = orientationW * orientationV.inverse();
                if(deformationGradientsArray)
                    deformationGradientsArray[particleIndex] = elasticF.toDataType<FloatType>();

                // Calculate strain tensor.
                SymmetricTensor2T<double> elasticStrain;
                if(!_pushStrainTensorsForward) {
                    // Compute Green strain tensor in material frame.
                    elasticStrain = (Product_AtA(elasticF) - SymmetricTensor2T<double>::Identity()) * 0.5;
                }
                else {
                    // Compute Euler strain tensor in spatial frame.
                    Matrix_3<double> inverseF;
                    if(!elasticF.inverse(inverseF))
                        throw Exception(ElasticStrainModifier::tr("Cannot compute strain tensor in spatial reference frame, because the elastic deformation gradient at atom index %1 is singular.").arg(particleIndex+1));
                    elasticStrain = (SymmetricTensor2T<double>::Identity() - Product_AtA(inverseF)) * 0.5;
                }

                // Store strain tensor in output property.
                if(strainTensorsArray)
                    strainTensorsArray[particleIndex] = (SymmetricTensor2)elasticStrain;

                // Calculate volumetric strain component.
                double volumetricStrain = (elasticStrain(0,0) + elasticStrain(1,1) + elasticStrain(2,2)) / 3.0;
                OVITO_ASSERT(std::isfinite(volumetricStrain));
                volumetricStrainsArray[particleIndex] = static_cast<FloatType>(volumetricStrain);

                return;
            }
        }

        // Mark atom as invalid.
        volumetricStrainsArray[particleIndex] = 0;
        if(strainTensorsArray)
            strainTensorsArray[particleIndex] = SymmetricTensor2::Zero();
        if(deformationGradientsArray)
            deformationGradientsArray[particleIndex] = Matrix3::Zero();
    });

    progress.endSubSteps();

    // Release data that is no longer needed.
    _structureAnalysis.reset();
}

/******************************************************************************
* Computes the structure identification statistics.
******************************************************************************/
std::vector<int64_t> ElasticStrainEngine::computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const
{
    std::vector<int64_t> typeCounts = StructureIdentificationModifier::Algorithm::computeStructureStatistics(structures, state, createdByNode, modifierParameters);

    // Output cluster graph.
    state.addObject(clusterGraph());

    // Output particle properties.
    Particles* particles = state.expectMutableObject<Particles>();
    particles->createProperty(atomClusters());
    if(strainTensors())
        particles->createProperty(strainTensors());

    if(deformationGradients())
        particles->createProperty(deformationGradients());

    if(volumetricStrains())
        particles->createProperty(volumetricStrains());

    return typeCounts;
}

}   // End of namespace
