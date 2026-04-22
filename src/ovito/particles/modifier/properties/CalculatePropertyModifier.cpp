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
    case CalculatePropertyModifier::ManualMolecularDirection:
        return CalculatePropertyModifier::tr("molecular direction");
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
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, fromTypeId);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, toTypeId);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, propertyType, "Property");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, fromTypeId, "From atom type");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, toTypeId, "To atom type");
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
    BufferReadAccess<FloatType> charges(propertyType() == DipoleDirection ? particles->getProperty(Particles::ChargeProperty) : nullptr);
    if(propertyType() == DipoleDirection && !charges) {
        throw Exception(tr("The selected property calculation requires the particle property 'Charge'."));
    }

    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds) {
        throw Exception(tr("The selected property calculation requires the particle property 'Molecule Identifier'. Load molecular topology or create that property upstream first."));
    }

    BufferReadAccess<int32_t> particleTypes(propertyType() == ManualMolecularDirection ? particles->getProperty(Particles::TypeProperty) : nullptr);
    if(propertyType() == ManualMolecularDirection && !particleTypes) {
        throw Exception(tr("The manual molecular direction mode requires the particle property 'Particle Type'."));
    }
    if(propertyType() == ManualMolecularDirection && fromTypeId() == toTypeId()) {
        throw Exception(tr("The manual molecular direction mode requires two different particle types."));
    }

    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection) {
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection. Add a selection modifier upstream or disable this option."));
    }

    PropertyPtr vectorProperty;
    PropertyPtr magnitudeProperty;
    if(propertyType() == DipoleDirection) {
        vectorProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleOrientationProperty);
        magnitudeProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleMagnitudeProperty);
    }
    else {
        vectorProperty = PropertyPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 3,
                                             QStringLiteral("Molecular Direction"), 0, QStringList() << "X" << "Y" << "Z");
        if(vectorProperty->visElements().empty()) {
            OORef<VectorVis> vis = OORef<VectorVis>::create();
            vis->setObjectTitle(tr("Molecular Direction"));
            vis->setEnabled(false);
            vis->setReverseArrowDirection(false);
            vis->setArrowPosition(VectorVis::Center);
            vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
            vectorProperty->addVisElement(std::move(vis));
        }

        magnitudeProperty = PropertyPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 1,
                                                QStringLiteral("Molecular Direction Magnitude"));
    }

    const SimulationCell* cell = state.getObject<SimulationCell>();
    const PropertyType recipe = propertyType();
    const bool selectedOnly = onlySelectedParticles();
    const int fromParticleType = fromTypeId();
    const int toParticleType = toTypeId();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            charges = std::move(charges),
            moleculeIds = std::move(moleculeIds),
            particleTypes = std::move(particleTypes),
            selection = std::move(selection),
            cell,
            recipe,
            selectedOnly,
            fromParticleType,
            toParticleType,
            vectorProperty = std::move(vectorProperty),
            magnitudeProperty = std::move(magnitudeProperty)]() mutable
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

        BufferWriteAccess<Vector3, access_mode::discard_write> directionAcc(vectorProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> magnitudeAcc(magnitudeProperty);

        std::vector<Point3> moleculePositions;
        size_t processedMoleculeCount = 0;
        size_t zeroMagnitudeCount = 0;
        size_t skippedMissingTypeCount = 0;

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

            Vector3 orientationVector = Vector3::Zero();
            switch(recipe) {
            case DipoleDirection:
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    orientationVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - center);
                }
                break;
            case ManualMolecularDirection: {
                Vector3 fromCentroidOffset = Vector3::Zero();
                Vector3 toCentroidOffset = Vector3::Zero();
                size_t fromCount = 0;
                size_t toCount = 0;
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const size_t particleIndex = group.indices[atomListIndex];
                    const Point3& position = moleculePositions[atomListIndex];
                    if(particleTypes[particleIndex] == fromParticleType) {
                        fromCentroidOffset += (position - reference);
                        fromCount++;
                    }
                    if(particleTypes[particleIndex] == toParticleType) {
                        toCentroidOffset += (position - reference);
                        toCount++;
                    }
                }
                if(fromCount == 0 || toCount == 0) {
                    skippedMissingTypeCount++;
                    continue;
                }
                const Point3 fromCentroid = reference + fromCentroidOffset / static_cast<FloatType>(fromCount);
                const Point3 toCentroid = reference + toCentroidOffset / static_cast<FloatType>(toCount);
                orientationVector = toCentroid - fromCentroid;
                break;
            }
            }

            const FloatType orientationMagnitude = orientationVector.length();
            const Vector3 orientationDirection = (orientationMagnitude > FloatType(0)) ? (orientationVector / orientationMagnitude) : Vector3::Zero();
            if(orientationMagnitude <= FloatType(0))
                zeroMagnitudeCount++;

            for(size_t particleIndex : group.indices) {
                directionAcc[particleIndex] = orientationDirection;
                magnitudeAcc[particleIndex] = orientationMagnitude;
            }

            processedMoleculeCount++;
        }

        if(selectedOnly && processedMoleculeCount == 0) {
            if(recipe == ManualMolecularDirection && skippedMissingTypeCount > 0) {
                throw Exception(tr("The selected molecules do not contain both requested atom types. The manual molecular direction uses the centroid of the chosen source type and the centroid of the chosen target type within each molecule."));
            }
            throw Exception(tr("No molecules were selected. This modifier promotes the particle selection to whole molecules, so at least one particle in a molecule must be selected."));
        }
        if(recipe == ManualMolecularDirection && processedMoleculeCount == 0)
            throw Exception(tr("No molecules contained both selected atom types. The manual molecular direction uses the centroid of the chosen source type and the centroid of the chosen target type within each molecule."));

        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->createProperty(std::move(vectorProperty));
        outputParticles->createProperty(std::move(magnitudeProperty));

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
        case ManualMolecularDirection:
            if(selectedOnly)
                statusText = tr("Computed %1 for %2 selected molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
            else
                statusText = tr("Computed %1 for %2 molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
            if(skippedMissingTypeCount > 0)
                statusText += tr(" %1 molecules were skipped because they did not contain both selected atom types.").arg(skippedMissingTypeCount);
            if(zeroMagnitudeCount > 0)
                statusText += tr(" %1 molecules had zero direction magnitude.").arg(zeroMagnitudeCount);
            break;
        }
        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(processedMoleculeCount)));
        return std::move(state);
    });
}

}  // namespace Ovito
