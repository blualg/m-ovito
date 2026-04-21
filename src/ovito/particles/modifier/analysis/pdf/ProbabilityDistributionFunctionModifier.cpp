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
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ProbabilityDistributionFunctionModifier.h"

namespace Ovito {

namespace {

std::pair<FloatType, FloatType> defaultRangeForMode(ProbabilityDistributionFunctionModifier::DistributionMode mode)
{
    switch(mode) {
    case ProbabilityDistributionFunctionModifier::BondLength:
        return {FloatType(0), FloatType(5)};
    case ProbabilityDistributionFunctionModifier::BondAngle:
        return {FloatType(0), FloatType(180)};
    case ProbabilityDistributionFunctionModifier::DihedralAngle:
        return {FloatType(-180), FloatType(180)};
    }
    OVITO_ASSERT(false);
    return {FloatType(0), FloatType(1)};
}

QString modeTitle(ProbabilityDistributionFunctionModifier::DistributionMode mode)
{
    switch(mode) {
    case ProbabilityDistributionFunctionModifier::BondLength:
        return ProbabilityDistributionFunctionModifier::tr("Bond length PDF");
    case ProbabilityDistributionFunctionModifier::BondAngle:
        return ProbabilityDistributionFunctionModifier::tr("Bond angle PDF");
    case ProbabilityDistributionFunctionModifier::DihedralAngle:
        return ProbabilityDistributionFunctionModifier::tr("Dihedral angle PDF");
    }
    OVITO_ASSERT(false);
    return {};
}

QString modeAxisLabel(ProbabilityDistributionFunctionModifier::DistributionMode mode)
{
    switch(mode) {
    case ProbabilityDistributionFunctionModifier::BondLength:
        return ProbabilityDistributionFunctionModifier::tr("Bond length");
    case ProbabilityDistributionFunctionModifier::BondAngle:
        return ProbabilityDistributionFunctionModifier::tr("Bond angle (degrees)");
    case ProbabilityDistributionFunctionModifier::DihedralAngle:
        return ProbabilityDistributionFunctionModifier::tr("Dihedral angle (degrees)");
    }
    OVITO_ASSERT(false);
    return {};
}

QString missingTopologyMessage(ProbabilityDistributionFunctionModifier::DistributionMode mode)
{
    switch(mode) {
    case ProbabilityDistributionFunctionModifier::BondLength:
        return ProbabilityDistributionFunctionModifier::tr(
            "No bonds are present in the input. Load explicit topology or create bonds upstream before computing the bond length PDF.");
    case ProbabilityDistributionFunctionModifier::BondAngle:
        return ProbabilityDistributionFunctionModifier::tr(
            "No angle topology is present in the input. Load a topology file that defines molecular angles before computing the bond angle PDF.");
    case ProbabilityDistributionFunctionModifier::DihedralAngle:
        return ProbabilityDistributionFunctionModifier::tr(
            "No dihedral topology is present in the input. Load a topology file that defines molecular dihedrals before computing the dihedral angle PDF.");
    }
    OVITO_ASSERT(false);
    return {};
}

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

inline Vector3 minimumImageVector(const Point3& from, const Point3& to, const SimulationCell* cell)
{
    Vector3 delta = to - from;
    if(cell)
        delta = cell->wrapVector(delta);
    return delta;
}

FloatType computeBondLength(const Point3& p1,
                            const Point3& p2,
                            const Vector3I* bondShift,
                            const SimulationCell* cell)
{
    Vector3 delta = p2 - p1;
    if(bondShift) {
        if(!cell && *bondShift != Vector3I::Zero()) {
            throw Exception(ProbabilityDistributionFunctionModifier::tr(
                "Cannot evaluate periodic bonds without a simulation cell."));
        }
        if(cell)
            delta += cell->cellMatrix() * bondShift->toDataType<FloatType>();
    }
    else if(cell) {
        delta = cell->wrapVector(delta);
    }
    return delta.length();
}

FloatType computeBondAngleDegrees(const Point3& p1,
                                  const Point3& p2,
                                  const Point3& p3,
                                  const SimulationCell* cell)
{
    Vector3 v1 = minimumImageVector(p2, p1, cell);
    Vector3 v2 = minimumImageVector(p2, p3, cell);
    FloatType len1 = v1.length();
    FloatType len2 = v2.length();
    if(len1 <= FloatType(0) || len2 <= FloatType(0))
        return std::numeric_limits<FloatType>::quiet_NaN();
    return qRadiansToDegrees(clampedAcos(v1.dot(v2) / (len1 * len2)));
}

FloatType computeDihedralDegrees(const Point3& p1,
                                 const Point3& p2,
                                 const Point3& p3,
                                 const Point3& p4,
                                 const SimulationCell* cell)
{
    Vector3 b1 = minimumImageVector(p1, p2, cell);
    Vector3 b2 = minimumImageVector(p2, p3, cell);
    Vector3 b3 = minimumImageVector(p3, p4, cell);

    Vector3 n1 = b1.cross(b2);
    Vector3 n2 = b2.cross(b3);
    FloatType n1len = n1.length();
    FloatType n2len = n2.length();
    FloatType b2len = b2.length();
    if(n1len <= FloatType(0) || n2len <= FloatType(0) || b2len <= FloatType(0))
        return std::numeric_limits<FloatType>::quiet_NaN();

    Vector3 b2hat = b2 / b2len;
    Vector3 m1 = n1.cross(b2hat);
    FloatType angle = std::atan2(m1.dot(n2), n1.dot(n2));
    return qRadiansToDegrees(angle);
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(ProbabilityDistributionFunctionModifier);
OVITO_CLASSINFO(ProbabilityDistributionFunctionModifier, "DisplayName", "Probability distribution function (PDF)");
OVITO_CLASSINFO(ProbabilityDistributionFunctionModifier, "Description",
                "Compute probability distribution functions for explicit bond, angle, or dihedral topology.");
OVITO_CLASSINFO(ProbabilityDistributionFunctionModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(ProbabilityDistributionFunctionModifier, mode);
DEFINE_PROPERTY_FIELD(ProbabilityDistributionFunctionModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(ProbabilityDistributionFunctionModifier, rangeStart);
DEFINE_PROPERTY_FIELD(ProbabilityDistributionFunctionModifier, rangeEnd);
DEFINE_PROPERTY_FIELD(ProbabilityDistributionFunctionModifier, onlySelected);
SET_PROPERTY_FIELD_LABEL(ProbabilityDistributionFunctionModifier, mode, "Quantity");
SET_PROPERTY_FIELD_LABEL(ProbabilityDistributionFunctionModifier, numberOfBins, "Number of bins");
SET_PROPERTY_FIELD_LABEL(ProbabilityDistributionFunctionModifier, rangeStart, "Range start");
SET_PROPERTY_FIELD_LABEL(ProbabilityDistributionFunctionModifier, rangeEnd, "Range end");
SET_PROPERTY_FIELD_LABEL(ProbabilityDistributionFunctionModifier, onlySelected, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ProbabilityDistributionFunctionModifier, numberOfBins, IntegerParameterUnit, 4);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool ProbabilityDistributionFunctionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called when the value of a property field of this object has changed.
 ******************************************************************************/
void ProbabilityDistributionFunctionModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(ProbabilityDistributionFunctionModifier::mode) && !shouldIgnoreChanges()) {
        const auto [start, end] = defaultRangeForMode(mode());
        setRangeStart(start);
        setRangeEnd(end);
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> ProbabilityDistributionFunctionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<SelectionIntType> selection(onlySelected() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelected() && !selection)
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection. Add a selection modifier upstream or disable this option."));

    const SimulationCell* cell = state.getObject<SimulationCell>();
    const int binCount = std::max(4, numberOfBins());
    const FloatType xStart = rangeStart();
    const FloatType xEnd = rangeEnd();
    if(!(xEnd > xStart))
        throw Exception(tr("The histogram range is invalid. The range end must be greater than the range start."));

    const FloatType binSize = (xEnd - xStart) / binCount;
    const DistributionMode distributionMode = mode();

    DataTable* table = state.createObject<DataTable>(QString(TableIdentifier), request.modificationNode(), DataTable::Histogram, modeTitle(distributionMode));
    table->setAxisLabelX(modeAxisLabel(distributionMode));
    table->setAxisLabelY(tr("Probability density"));
    table->setIntervalStart(xStart);
    table->setIntervalEnd(xEnd);

    return asyncLaunch([
            state = std::move(state),
            particles,
            positions = std::move(positions),
            selection = std::move(selection),
            cell,
            distributionMode,
            binCount,
            xStart,
            xEnd,
            binSize,
            table]() mutable
    {
        std::vector<int64_t> histogram(binCount, 0);
        int64_t includedSampleCount = 0;
        int64_t discardedSampleCount = 0;
        int64_t invalidTopologyCount = 0;

        auto acceptValue = [&](FloatType value) {
            if(!std::isfinite(value))
                return;
            if(value < xStart || value > xEnd) {
                discardedSampleCount++;
                return;
            }
            size_t binIndex = static_cast<size_t>((value - xStart) / binSize);
            if(binIndex >= static_cast<size_t>(binCount))
                binIndex = static_cast<size_t>(binCount - 1);
            histogram[binIndex]++;
            includedSampleCount++;
        };

        switch(distributionMode) {
        case BondLength: {
            const Bonds* bonds = particles->bonds();
            if(!bonds)
                throw Exception(missingTopologyMessage(distributionMode));
            BufferReadAccess<ParticleIndexPair> topology = bonds->expectProperty(Bonds::TopologyProperty);
            BufferReadAccess<Vector3I> periodicImages = bonds->getProperty(Bonds::PeriodicImageProperty);
            for(size_t bondIndex = 0; bondIndex < topology.size(); ++bondIndex) {
                const auto& bond = topology[bondIndex];
                const size_t index1 = static_cast<size_t>(bond[0]);
                const size_t index2 = static_cast<size_t>(bond[1]);
                if(index1 >= positions.size() || index2 >= positions.size()) {
                    invalidTopologyCount++;
                    continue;
                }
                if(selection && (!selection[index1] || !selection[index2]))
                    continue;
                const Vector3I* bondShift = periodicImages ? &periodicImages[bondIndex] : nullptr;
                acceptValue(computeBondLength(positions[index1], positions[index2], bondShift, cell));
            }
            break;
        }
        case BondAngle: {
            const Angles* angles = particles->angles();
            if(!angles)
                throw Exception(missingTopologyMessage(distributionMode));
            BufferReadAccess<ParticleIndexTriplet> topology = angles->expectProperty(Angles::TopologyProperty);
            for(const auto& angle : topology) {
                const size_t index1 = static_cast<size_t>(angle[0]);
                const size_t index2 = static_cast<size_t>(angle[1]);
                const size_t index3 = static_cast<size_t>(angle[2]);
                if(index1 >= positions.size() || index2 >= positions.size() || index3 >= positions.size()) {
                    invalidTopologyCount++;
                    continue;
                }
                if(selection && (!selection[index1] || !selection[index2] || !selection[index3]))
                    continue;
                acceptValue(computeBondAngleDegrees(positions[index1], positions[index2], positions[index3], cell));
            }
            break;
        }
        case DihedralAngle: {
            const Dihedrals* dihedrals = particles->dihedrals();
            if(!dihedrals)
                throw Exception(missingTopologyMessage(distributionMode));
            BufferReadAccess<ParticleIndexQuadruplet> topology = dihedrals->expectProperty(Dihedrals::TopologyProperty);
            for(const auto& dihedral : topology) {
                const size_t index1 = static_cast<size_t>(dihedral[0]);
                const size_t index2 = static_cast<size_t>(dihedral[1]);
                const size_t index3 = static_cast<size_t>(dihedral[2]);
                const size_t index4 = static_cast<size_t>(dihedral[3]);
                if(index1 >= positions.size() || index2 >= positions.size() || index3 >= positions.size() || index4 >= positions.size()) {
                    invalidTopologyCount++;
                    continue;
                }
                if(selection && (!selection[index1] || !selection[index2] || !selection[index3] || !selection[index4]))
                    continue;
                acceptValue(computeDihedralDegrees(positions[index1], positions[index2], positions[index3], positions[index4], cell));
            }
            break;
        }
        }

        if(includedSampleCount == 0) {
            if(discardedSampleCount > 0) {
                throw Exception(tr("No samples fell into the requested histogram range. Adjust the range to include the selected %1 values.")
                                    .arg(modeAxisLabel(distributionMode).toLower()));
            }
            throw Exception(tr("No valid %1 samples are available in the current frame.")
                                .arg(modeAxisLabel(distributionMode).toLower()));
        }

        table->setElementCount(binCount);
        Property* pdfValues = table->createProperty(DataBuffer::Initialized, QStringLiteral("PDF"), Property::FloatDefault);
        BufferWriteAccess<FloatType, access_mode::discard_write> pdf(pdfValues);
        for(int binIndex = 0; binIndex < binCount; ++binIndex)
            pdf[binIndex] = static_cast<FloatType>(histogram[binIndex]) / (static_cast<FloatType>(includedSampleCount) * binSize);
        table->setY(pdfValues);

        QString statusText = tr("Computed %1 from %2 sampled interactions.")
                                 .arg(modeTitle(distributionMode))
                                 .arg(includedSampleCount);
        if(discardedSampleCount > 0)
            statusText += tr(" %1 interactions were outside the histogram range.").arg(discardedSampleCount);
        if(invalidTopologyCount > 0)
            statusText += tr(" %1 topology entries referenced invalid particle indices and were skipped.").arg(invalidTopologyCount);
        state.setStatus(statusText);
        return std::move(state);
    });
}

}  // namespace Ovito
