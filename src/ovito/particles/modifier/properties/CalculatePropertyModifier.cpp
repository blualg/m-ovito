////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "CalculatePropertyModifier.h"

#include <unordered_map>

namespace Ovito {

namespace {

struct MoleculeGroup
{
    std::vector<size_t> indices;
    bool anySelected = false;
};

QString propertyTypeLabel(CalculatePropertyModifier::PropertyType propertyType)
{
    switch(propertyType) {
    case CalculatePropertyModifier::DipoleDirection:
        return CalculatePropertyModifier::tr("dipole direction");
    }
    OVITO_ASSERT(false);
    return {};
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(CalculatePropertyModifier);
OVITO_CLASSINFO(CalculatePropertyModifier, "DisplayName", "Calculate property");
OVITO_CLASSINFO(CalculatePropertyModifier, "Description", "Calculate predefined derived particle properties.");
OVITO_CLASSINFO(CalculatePropertyModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, propertyType);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, propertyType, "Property");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, onlySelectedParticles, "Use only selected particles");

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool CalculatePropertyModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> CalculatePropertyModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<FloatType> charges = particles->getProperty(Particles::ChargeProperty);
    if(!charges) {
        throw Exception(tr("The selected property calculation requires the particle property 'Charge'."));
    }

    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds) {
        throw Exception(tr("The selected property calculation requires the particle property 'Molecule Identifier'. Load molecular topology or create that property upstream first."));
    }

    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection) {
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection. Add a selection modifier upstream or disable this option."));
    }

    PropertyPtr dipoleDirections =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleOrientationProperty);
    PropertyPtr dipoleMagnitudes =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleMagnitudeProperty);

    const SimulationCell* cell = state.getObject<SimulationCell>();
    const PropertyType recipe = propertyType();
    const bool selectedOnly = onlySelectedParticles();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            charges = std::move(charges),
            moleculeIds = std::move(moleculeIds),
            selection = std::move(selection),
            cell,
            recipe,
            selectedOnly,
            dipoleDirections = std::move(dipoleDirections),
            dipoleMagnitudes = std::move(dipoleMagnitudes)]() mutable
    {
        std::unordered_map<IdentifierIntType, size_t> groupLookup;
        groupLookup.reserve(positions.size());
        std::vector<MoleculeGroup> molecules;
        molecules.reserve(positions.size());

        for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
            const IdentifierIntType moleculeId = moleculeIds[particleIndex];
            auto [iter, inserted] = groupLookup.try_emplace(moleculeId, molecules.size());
            if(inserted)
                molecules.emplace_back();

            MoleculeGroup& group = molecules[iter->second];
            group.indices.push_back(particleIndex);
            if(selection && selection[particleIndex])
                group.anySelected = true;
        }

        BufferWriteAccess<Vector3, access_mode::discard_write> directionAcc(dipoleDirections);
        BufferWriteAccess<FloatType, access_mode::discard_write> magnitudeAcc(dipoleMagnitudes);

        std::vector<Point3> moleculePositions;
        size_t processedMoleculeCount = 0;
        size_t zeroMagnitudeCount = 0;

        for(const MoleculeGroup& group : molecules) {
            if(group.indices.empty())
                continue;
            if(selectedOnly && !group.anySelected)
                continue;

            moleculePositions.clear();
            moleculePositions.reserve(group.indices.size());

            const Point3 reference = positions[group.indices.front()];
            moleculePositions.push_back(reference);
            for(size_t atomListIndex = 1; atomListIndex < group.indices.size(); ++atomListIndex) {
                const Point3 current = positions[group.indices[atomListIndex]];
                Vector3 delta = current - reference;
                if(cell)
                    delta = cell->wrapVector(delta);
                moleculePositions.push_back(reference + delta);
            }

            Vector3 centerOffsetSum = Vector3::Zero();
            for(const Point3& position : moleculePositions)
                centerOffsetSum += (position - reference);
            const Point3 center = reference + centerOffsetSum / static_cast<FloatType>(moleculePositions.size());

            Vector3 dipoleVector = Vector3::Zero();
            for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                dipoleVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - center);
            }

            const FloatType dipoleMagnitude = dipoleVector.length();
            const Vector3 dipoleDirection = (dipoleMagnitude > FloatType(0)) ? (dipoleVector / dipoleMagnitude) : Vector3::Zero();
            if(dipoleMagnitude <= FloatType(0))
                zeroMagnitudeCount++;

            for(size_t particleIndex : group.indices) {
                directionAcc[particleIndex] = dipoleDirection;
                magnitudeAcc[particleIndex] = dipoleMagnitude;
            }

            processedMoleculeCount++;
        }

        if(selectedOnly && processedMoleculeCount == 0) {
            throw Exception(tr("No molecules were selected. This modifier promotes the particle selection to whole molecules, so at least one particle in a molecule must be selected."));
        }

        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->createProperty(std::move(dipoleDirections));
        outputParticles->createProperty(std::move(dipoleMagnitudes));

        QString statusText;
        switch(recipe) {
        case DipoleDirection:
            if(selectedOnly)
                statusText = tr("Computed %1 for %2 selected molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
            else
                statusText = tr("Computed %1 for %2 molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
            if(zeroMagnitudeCount > 0)
                statusText += tr(" %1 molecules had zero dipole magnitude.").arg(zeroMagnitudeCount);
            break;
        }
        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(processedMoleculeCount)));
        return std::move(state);
    });
}

}  // namespace Ovito
