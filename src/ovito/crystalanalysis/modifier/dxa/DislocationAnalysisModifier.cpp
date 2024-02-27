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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/MicrostructurePhase.h>
#include <ovito/crystalanalysis/objects/DislocationVis.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "DislocationAnalysisModifier.h"
#include "DislocationAnalysisEngine.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationAnalysisModifier);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, inputCrystalStructure);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, maxTrialCircuitSize);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, circuitStretchability);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, outputInterfaceMesh);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, onlyPerfectDislocations);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, defectMeshSmoothingLevel);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, lineSmoothingEnabled);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, lineSmoothingLevel);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, lineCoarseningEnabled);
DEFINE_PROPERTY_FIELD(DislocationAnalysisModifier, linePointInterval);
DEFINE_REFERENCE_FIELD(DislocationAnalysisModifier, dislocationVis);
DEFINE_REFERENCE_FIELD(DislocationAnalysisModifier, defectMeshVis);
DEFINE_REFERENCE_FIELD(DislocationAnalysisModifier, interfaceMeshVis);
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, inputCrystalStructure, "Input crystal structure");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, maxTrialCircuitSize, "Trial circuit length");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, circuitStretchability, "Circuit stretchability");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, outputInterfaceMesh, "Output interface mesh");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, onlyPerfectDislocations, "Generate perfect dislocations");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, defectMeshSmoothingLevel, "Surface smoothing level");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, lineSmoothingEnabled, "Line smoothing");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, lineSmoothingLevel, "Smoothing level");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, lineCoarseningEnabled, "Line coarsening");
SET_PROPERTY_FIELD_LABEL(DislocationAnalysisModifier, linePointInterval, "Point separation");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationAnalysisModifier, maxTrialCircuitSize, IntegerParameterUnit, 3);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationAnalysisModifier, circuitStretchability, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationAnalysisModifier, defectMeshSmoothingLevel, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationAnalysisModifier, lineSmoothingLevel, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DislocationAnalysisModifier, linePointInterval, FloatParameterUnit, 0);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
DislocationAnalysisModifier::DislocationAnalysisModifier(ObjectInitializationFlags flags) : StructureIdentificationModifier(flags),
    _inputCrystalStructure(StructureAnalysis::LATTICE_FCC),
    _maxTrialCircuitSize(14),
    _circuitStretchability(9),
    _outputInterfaceMesh(false),
    _onlyPerfectDislocations(false),
    _defectMeshSmoothingLevel(8),
    _lineSmoothingEnabled(true),
    _lineCoarseningEnabled(true),
    _lineSmoothingLevel(1),
    _linePointInterval(2.5)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the vis elements.
        setDislocationVis(OORef<DislocationVis>::create(flags));

        setDefectMeshVis(OORef<SurfaceMeshVis>::create(flags));
        defectMeshVis()->setShowCap(true);
        defectMeshVis()->setSmoothShading(true);
        defectMeshVis()->setReverseOrientation(true);
        defectMeshVis()->setCapTransparency(0.5);
        defectMeshVis()->setObjectTitle(tr("Defect mesh"));

        setInterfaceMeshVis(OORef<SurfaceMeshVis>::create(flags));
        interfaceMeshVis()->setShowCap(false);
        interfaceMeshVis()->setSmoothShading(false);
        interfaceMeshVis()->setReverseOrientation(true);
        interfaceMeshVis()->setCapTransparency(0.5);
        interfaceMeshVis()->setObjectTitle(tr("Interface mesh"));

        // Create the structure types.
        ParticleType::PredefinedStructureType predefTypes[] = {
                ParticleType::PredefinedStructureType::OTHER,
                ParticleType::PredefinedStructureType::FCC,
                ParticleType::PredefinedStructureType::HCP,
                ParticleType::PredefinedStructureType::BCC,
                ParticleType::PredefinedStructureType::CUBIC_DIAMOND,
                ParticleType::PredefinedStructureType::HEX_DIAMOND
        };
        OVITO_STATIC_ASSERT(sizeof(predefTypes)/sizeof(predefTypes[0]) == StructureAnalysis::NUM_LATTICE_TYPES);
        for(int id = 0; id < StructureAnalysis::NUM_LATTICE_TYPES; id++) {
            DataOORef<MicrostructurePhase> stype = DataOORef<MicrostructurePhase>::create(flags);
            stype->setNumericId(id);
            stype->setDimensionality(MicrostructurePhase::Dimensionality::Volumetric);
            stype->setName(ParticleType::getPredefinedStructureTypeName(predefTypes[id]));
            stype->setColor(ElementType::getDefaultColor(ParticlePropertyReference(Particles::StructureTypeProperty), stype->name(), id));
            addStructureType(std::move(stype));
        }

        // Create Burgers vector families.

        MicrostructurePhase* fccPattern = structureTypeById(StructureAnalysis::LATTICE_FCC);
        fccPattern->setCrystalSymmetryClass(MicrostructurePhase::CrystalSymmetryClass::CubicSymmetry);
        fccPattern->setShortName(QStringLiteral("fcc"));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 1, tr("1/2<110> (Perfect)"), Vector3(1.0/2.0, 1.0/2.0, 0.0), Color(0.2,0.2,1)));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 2, tr("1/6<112> (Shockley)"), Vector3(1.0/6.0, 1.0/6.0, 2.0f/6.0f), Color(0,1,0)));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 3, tr("1/6<110> (Stair-rod)"), Vector3(1.0/6.0, 1.0/6.0, 0.0/6.0f), Color(1,0,1)));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 4, tr("1/3<100> (Hirth)"), Vector3(1.0/3.0, 0.0, 0.0), Color(1,1,0)));
        fccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 5, tr("1/3<111> (Frank)"), Vector3(1.0/3.0, 1.0/3.0, 1.0/3.0), Color(0,1,1)));

        MicrostructurePhase* bccPattern = structureTypeById(StructureAnalysis::LATTICE_BCC);
        bccPattern->setCrystalSymmetryClass(MicrostructurePhase::CrystalSymmetryClass::CubicSymmetry);
        bccPattern->setShortName(QStringLiteral("bcc"));
        bccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
        bccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 11, tr("1/2<111>"), Vector3(1.0/2.0, 1.0/2.0, 1.0/2.0), Color(0,1,0)));
        bccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 12, tr("<100>"), Vector3(1.0, 0.0, 0.0), Color(1, 0.3f, 0.8f)));
        bccPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 13, tr("<110>"), Vector3(1.0, 1.0, 0.0), Color(0.2, 0.5, 1.0)));

        MicrostructurePhase* hcpPattern = structureTypeById(StructureAnalysis::LATTICE_HCP);
        hcpPattern->setShortName(QStringLiteral("hcp"));
        hcpPattern->setCrystalSymmetryClass(MicrostructurePhase::CrystalSymmetryClass::HexagonalSymmetry);
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 21, tr("1/3<1-210>"), Vector3(sqrt(0.5), 0.0, 0.0), Color(0,1,0)));
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 22, tr("<0001>"), Vector3(0.0, 0.0, sqrt(4.0/3.0)), Color(0.2,0.2,1)));
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 23, tr("<1-100>"), Vector3(0.0, sqrt(3.0/2.0f), 0.0), Color(1,0,1)));
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 24, tr("1/3<1-100>"), Vector3(0.0, sqrt(3.0/2.0f)/3.0, 0.0), Color(1,0.5,0)));
        hcpPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 25, tr("1/3<1-213>"), Vector3(sqrt(0.5), 0.0, sqrt(4.0/3.0)), Color(1,1,0)));

        MicrostructurePhase* cubicDiaPattern = structureTypeById(StructureAnalysis::LATTICE_CUBIC_DIAMOND);
        cubicDiaPattern->setShortName(QStringLiteral("diamond"));
        cubicDiaPattern->setCrystalSymmetryClass(MicrostructurePhase::CrystalSymmetryClass::CubicSymmetry);
        cubicDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
        cubicDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 31, tr("1/2<110>"), Vector3(1.0/2.0, 1.0/2.0, 0.0), Color(0.2,0.2,1)));
        cubicDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 32, tr("1/6<112>"), Vector3(1.0/6.0, 1.0/6.0, 2.0/6.0), Color(0,1,0)));
        cubicDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 33, tr("1/6<110>"), Vector3(1.0/6.0, 1.0/6.0, 0.0), Color(1,0,1)));
        cubicDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 34, tr("1/3<111>"), Vector3(1.0/3.0, 1.0/3.0, 1.0/3.0), Color(0,1,1)));

        MicrostructurePhase* hexDiaPattern = structureTypeById(StructureAnalysis::LATTICE_HEX_DIAMOND);
        hexDiaPattern->setShortName(QStringLiteral("hex_diamond"));
        hexDiaPattern->setCrystalSymmetryClass(MicrostructurePhase::CrystalSymmetryClass::HexagonalSymmetry);
        hexDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
        hexDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 41, tr("1/3<1-210>"), Vector3(sqrt(0.5), 0.0, 0.0), Color(0,1,0)));
        hexDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 42, tr("<0001>"), Vector3(0.0, 0.0, sqrt(4.0/3.0)), Color(0.2,0.2,1)));
        hexDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 43, tr("<1-100>"), Vector3(0.0, sqrt(3.0/2.0), 0.0), Color(1,0,1)));
        hexDiaPattern->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags, 44, tr("1/3<1-100>"), Vector3(0.0, sqrt(3.0/2.0)/3.0, 0.0), Color(1,0.5,0)));
    }
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the modifier's results.
******************************************************************************/
Future<ModifierEnginePtr> DislocationAnalysisModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // If pipeline is in interactive mode, skip the long-running computation step.
    if(request.interactiveMode())
        return {};

    // Get modifier inputs.
    const Particles* particles = input.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);
    const SimulationCell* simCell = input.expectObject<SimulationCell>();
    if(simCell->is2D())
        throw Exception(tr("The DXA modifier does not support 2d simulation cells."));

    // Get particle selection.
    const Property* selectionProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Build list of preferred crystal orientations.
    std::vector<Matrix3> preferredCrystalOrientations;
    if(inputCrystalStructure() == StructureAnalysis::LATTICE_FCC || inputCrystalStructure() == StructureAnalysis::LATTICE_BCC || inputCrystalStructure() == StructureAnalysis::LATTICE_CUBIC_DIAMOND) {
        preferredCrystalOrientations.push_back(Matrix3::Identity());
    }

    // Get grain id property created by the GrainSegmentationModifier.
    const Property* grainProperty = particles->getProperty(QStringLiteral("Grain"));
    if(grainProperty && (grainProperty->dataType() != DataBuffer::Int64 || grainProperty->componentCount() != 1))
        grainProperty = nullptr;

    // Create an empty surface mesh object.
    DataOORef<SurfaceMesh> defectMesh = DataOORef<SurfaceMesh>::create(ObjectInitializationFlag::DontCreateVisElement, tr("Defect mesh"));
    defectMesh->setIdentifier(input.generateUniqueIdentifier<SurfaceMesh>(QStringLiteral("dxa-defect-mesh")));
    defectMesh->setCreatedByNode(request.modificationNode());
    defectMesh->setDomain(simCell);
    defectMesh->setVisElement(defectMeshVis());

    // Create an empty surface mesh object for the optional interface mesh.
    DataOORef<SurfaceMesh> interfaceMesh;
    if(outputInterfaceMesh()) {
        interfaceMesh = DataOORef<SurfaceMesh>::create(ObjectInitializationFlag::DontCreateVisElement, tr("Interface mesh"));
        interfaceMesh->setIdentifier(input.generateUniqueIdentifier<SurfaceMesh>(QStringLiteral("dxa-interface-mesh")));
        interfaceMesh->setCreatedByNode(request.modificationNode());
        interfaceMesh->setDomain(simCell);
        interfaceMesh->setVisElement(interfaceMeshVis());
    }

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    return std::make_shared<DislocationAnalysisEngine>(
            request,
            particles,
            posProperty,
            simCell,
            structureTypes(),
            inputCrystalStructure(),
            maxTrialCircuitSize(),
            circuitStretchability(),
            selectionProperty,
            grainProperty,
            std::move(preferredCrystalOrientations),
            onlyPerfectDislocations(),
            defectMeshSmoothingLevel(),
            std::move(defectMesh),
            std::move(interfaceMesh),
            lineSmoothingEnabled() ? lineSmoothingLevel() : 0,
            lineCoarseningEnabled() ? linePointInterval() : 0);
}

}   // End of namespace
