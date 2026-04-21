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
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/objects/VoxelGridVis.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "SpatialBinningModifier.h"

namespace Ovito {

namespace {

QString reductionLabel(SpatialBinningModifier::ReductionOperation op)
{
    switch(op) {
    case SpatialBinningModifier::Sum:
        return SpatialBinningModifier::tr("sum");
    case SpatialBinningModifier::Mean:
        return SpatialBinningModifier::tr("mean");
    case SpatialBinningModifier::Min:
        return SpatialBinningModifier::tr("min");
    case SpatialBinningModifier::Max:
        return SpatialBinningModifier::tr("max");
    case SpatialBinningModifier::SumDividedByBinVolume:
        return SpatialBinningModifier::tr("sum / bin volume");
    }
    OVITO_ASSERT(false);
    return {};
}

QString outputPropertyName(const Property* property, int vectorComponent)
{
    if(!property)
        return SpatialBinningModifier::tr("Particle count");
    if(vectorComponent >= 0)
        return property->nameWithComponent(vectorComponent);
    return property->name();
}

QString outputAxisLabel(const SpatialBinningModifier* modifier, const Property* property, int vectorComponent)
{
    return SpatialBinningModifier::tr("%1 (%2)")
        .arg(outputPropertyName(property, vectorComponent), reductionLabel(modifier->reductionOperation()));
}

QString axisTitle(int axis)
{
    return SpatialBinningModifier::tr("Position along cell vector %1").arg(axis + 1);
}

QString outputTitle(int activeAxisCount)
{
    return (activeAxisCount == 1) ? SpatialBinningModifier::tr("Spatial binning profile") : SpatialBinningModifier::tr("Spatial binning");
}

std::array<bool, 3> activeDirections(const SpatialBinningModifier* modifier)
{
    return {modifier->binDirection1(), modifier->binDirection2(), modifier->binDirection3()};
}

int activeDirectionCount(const std::array<bool, 3>& active)
{
    return int(active[0]) + int(active[1]) + int(active[2]);
}

int firstActiveAxis(const std::array<bool, 3>& active)
{
    for(int axis = 0; axis < 3; ++axis) {
        if(active[axis])
            return axis;
    }
    return -1;
}

VoxelGrid::GridDimensions makeGridShape(const SpatialBinningModifier* modifier, const std::array<bool, 3>& active)
{
    return {
        size_t(active[0] ? std::max(1, modifier->numberOfBins1()) : 1),
        size_t(active[1] ? std::max(1, modifier->numberOfBins2()) : 1),
        size_t(active[2] ? std::max(1, modifier->numberOfBins3()) : 1)
    };
}

FloatType activeAxisLength(const SimulationCell* cell, int axis)
{
    OVITO_ASSERT(cell != nullptr);
    switch(axis) {
    case 0:
        return cell->cellVector1().length();
    case 1:
        return cell->cellVector2().length();
    case 2:
        return cell->cellVector3().length();
    default:
        OVITO_ASSERT(false);
        return 0;
    }
}

bool mapParticleToBin(const SimulationCell* cell,
                      const Point3& position,
                      const std::array<bool, 3>& active,
                      const VoxelGrid::GridDimensions& shape,
                      std::array<size_t, 3>& coords)
{
    constexpr FloatType tol = FloatType(1e-6);

    Point3 reduced = cell->absoluteToReduced(position);
    for(int axis = 0; axis < 3; ++axis) {
        if(!active[axis]) {
            coords[axis] = 0;
            continue;
        }

        FloatType u = reduced[axis];
        if(cell->hasPbcCorrected(axis))
            u -= std::floor(u);

        if(u < 0 && std::abs(u) <= tol)
            u = 0;
        if(u > 1 && std::abs(u - FloatType(1)) <= tol)
            u = std::nextafter(FloatType(1), FloatType(0));

        if(u < 0 || u > 1)
            return false;
        if(u == FloatType(1))
            u = std::nextafter(FloatType(1), FloatType(0));

        coords[axis] = std::min(shape[axis] - 1, static_cast<size_t>(std::floor(u * shape[axis])));
    }

    return true;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(SpatialBinningModifier);
OVITO_CLASSINFO(SpatialBinningModifier, "DisplayName", "Spatial binning");
OVITO_CLASSINFO(SpatialBinningModifier, "Description", "Project particle properties onto a regular spatial binning grid.");
OVITO_CLASSINFO(SpatialBinningModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, reductionOperation);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, firstDerivative);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, binDirection1);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, binDirection2);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, binDirection3);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, numberOfBins1);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, numberOfBins2);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, numberOfBins3);
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, sourceProperty, "Input property");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, reductionOperation, "Reduction operation");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, firstDerivative, "Compute first derivative");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, binDirection1, "Cell vector 1");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, binDirection2, "Cell vector 2");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, binDirection3, "Cell vector 3");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, numberOfBins1, "Bins along cell vector 1");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, numberOfBins2, "Bins along cell vector 2");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, numberOfBins3, "Bins along cell vector 3");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialBinningModifier, numberOfBins1, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialBinningModifier, numberOfBins2, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialBinningModifier, numberOfBins3, IntegerParameterUnit, 1);

bool SpatialBinningModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

Future<PipelineFlowState> SpatialBinningModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    const SimulationCell* cell = state.getObject<SimulationCell>();
    if(!cell)
        throw Exception(tr("Spatial binning requires a simulation cell."));
    if(cell->isDegenerate())
        throw Exception(tr("Cannot create spatial bins because the simulation cell is degenerate."));

    const std::array<bool, 3> active = activeDirections(this);
    const int activeCount = activeDirectionCount(active);
    if(activeCount == 0)
        throw Exception(tr("Please activate at least one binning direction."));
    if(cell->is2D() && active[2])
        throw Exception(tr("Cannot bin along cell vector 3 for a two-dimensional simulation cell."));

    const VoxelGrid::GridDimensions gridShape = makeGridShape(this, active);
    const size_t totalBinCount = gridShape[0] * gridShape[1] * gridShape[2];
    if(totalBinCount == 0)
        throw Exception(tr("The bin grid is empty."));

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection)
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection. Add a selection modifier upstream or disable this option."));

    const Property* inputProperty = nullptr;
    int vectorComponent = -1;
    QString propertyError;
    if(sourceProperty()) {
        std::tie(inputProperty, vectorComponent) = sourceProperty().findInContainerWithComponent(particles, propertyError, false);
        if(!inputProperty)
            throw Exception(std::move(propertyError));
    }

    const size_t outputComponentCount = inputProperty ? ((vectorComponent >= 0) ? 1 : inputProperty->componentCount()) : 1;
    QStringList outputComponentNames;
    if(inputProperty && vectorComponent < 0)
        outputComponentNames = inputProperty->componentNames();

    const QString propertyName = outputPropertyName(inputProperty, vectorComponent);
    const QString yAxisLabel = outputAxisLabel(this, inputProperty, vectorComponent);
    const FloatType binVolume = (cell->is2D() ? cell->volume2D() : cell->volume3D()) / FloatType(totalBinCount);

    DataTable* table = nullptr;
    VoxelGrid* grid = nullptr;
    if(activeCount == 1) {
        const int axis = firstActiveAxis(active);
        table = state.createObject<DataTable>(QString(OutputIdentifier), request.modificationNode(), DataTable::Line, outputTitle(activeCount));
        table->setElementCount(gridShape[axis]);
        table->setAxisLabelX(axisTitle(axis));
        table->setAxisLabelY(yAxisLabel);
        table->setIntervalStart(0);
        table->setIntervalEnd(activeAxisLength(cell, axis));
    }
    else {
        grid = state.createObject<VoxelGrid>(request.modificationNode(), QString(OutputIdentifier));
        grid->setTitle(outputTitle(activeCount));
        grid->setGridType(VoxelGrid::CellData);
        grid->setShape(gridShape);
        grid->setElementCount(totalBinCount);
        grid->setDomain(cell);
    }

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            selection = std::move(selection),
            cell,
            inputProperty,
            vectorComponent,
            outputComponentCount,
            outputComponentNames = std::move(outputComponentNames),
            propertyName,
            active,
            activeCount,
            firstDerivative = firstDerivative(),
            gridShape,
            totalBinCount,
            binVolume,
            reductionOperation = reductionOperation(),
            table,
            grid]() mutable
    {
        std::vector<int64_t> counts(totalBinCount, 0);
        std::vector<FloatType> sums(totalBinCount * outputComponentCount, FloatType(0));
        std::vector<FloatType> mins(totalBinCount * outputComponentCount, std::numeric_limits<FloatType>::max());
        std::vector<FloatType> maxs(totalBinCount * outputComponentCount, std::numeric_limits<FloatType>::lowest());

        size_t includedParticleCount = 0;
        size_t skippedOutsideCount = 0;

        auto accumulateValues = [&](size_t particleIndex, auto valueGetter) {
            std::array<size_t, 3> coords{};
            if(!mapParticleToBin(cell, positions[particleIndex], active, gridShape, coords)) {
                skippedOutsideCount++;
                return;
            }

            const size_t binIndex = VoxelGrid::voxelIndex(coords, gridShape);
            counts[binIndex]++;
            includedParticleCount++;

            for(size_t component = 0; component < outputComponentCount; ++component) {
                const FloatType value = valueGetter(component);
                const size_t offset = binIndex * outputComponentCount + component;
                sums[offset] += value;
                mins[offset] = std::min(mins[offset], value);
                maxs[offset] = std::max(maxs[offset], value);
            }
        };

        const SelectionIntType* selectionPtr = selection ? selection.cbegin() : nullptr;
        if(!inputProperty) {
            for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
                if(selectionPtr && !selectionPtr[particleIndex])
                    continue;
                accumulateValues(particleIndex, [](size_t) { return FloatType(1); });
            }
        }
        else {
            inputProperty->forAnyType([&](auto _) {
                using T = decltype(_);
                BufferReadAccess<T*> values(inputProperty);
                for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
                    if(selectionPtr && !selectionPtr[particleIndex])
                        continue;
                    accumulateValues(particleIndex, [&](size_t component) {
                        const int sourceComponent = (vectorComponent >= 0) ? vectorComponent : static_cast<int>(component);
                        return static_cast<FloatType>(values.get(particleIndex, sourceComponent));
                    });
                }
            });
        }

        auto reducedValue = [&](size_t binIndex, size_t component) {
            const size_t offset = binIndex * outputComponentCount + component;
            if(counts[binIndex] == 0)
                return FloatType(0);

            switch(reductionOperation) {
            case Sum:
                return sums[offset];
            case Mean:
                return sums[offset] / FloatType(counts[binIndex]);
            case Min:
                return mins[offset];
            case Max:
                return maxs[offset];
            case SumDividedByBinVolume:
                return (binVolume > FloatType(0)) ? (sums[offset] / binVolume) : FloatType(0);
            }
            OVITO_ASSERT(false);
            return FloatType(0);
        };

        if(table && firstDerivative && table->elementCount() > 1) {
            const int axis = firstActiveAxis(active);
            const FloatType spacing = activeAxisLength(cell, axis) / FloatType(table->elementCount());
            if(spacing > FloatType(0)) {
                std::vector<FloatType> derivativeData(table->elementCount() * outputComponentCount, FloatType(0));
                const bool periodic = cell->hasPbcCorrected(axis);
                for(size_t binIndex = 0; binIndex < table->elementCount(); ++binIndex) {
                    size_t iPlus = binIndex + 1;
                    size_t iMinus = (binIndex == 0) ? 0 : (binIndex - 1);
                    FloatType denom = FloatType(2) * spacing;
                    if(iPlus >= table->elementCount()) {
                        if(periodic) {
                            iPlus = 0;
                        }
                        else {
                            iPlus = table->elementCount() - 1;
                            denom = spacing;
                        }
                    }
                    if(binIndex == 0 && !periodic)
                        denom = spacing;
                    if(binIndex == 0 && periodic)
                        iMinus = table->elementCount() - 1;

                    for(size_t component = 0; component < outputComponentCount; ++component) {
                        const FloatType plusValue = reducedValue(iPlus, component);
                        const FloatType minusValue = reducedValue(iMinus, component);
                        derivativeData[binIndex * outputComponentCount + component] = (plusValue - minusValue) / denom;
                    }
                }
                for(size_t binIndex = 0; binIndex < table->elementCount(); ++binIndex) {
                    for(size_t component = 0; component < outputComponentCount; ++component)
                        sums[binIndex * outputComponentCount + component] = derivativeData[binIndex * outputComponentCount + component];
                }
            }
        }

        if(table) {
            Property* yProperty = table->createProperty(DataBuffer::Initialized,
                                                        propertyName,
                                                        Property::FloatDefault,
                                                        outputComponentCount,
                                                        outputComponentNames);
            BufferWriteAccess<FloatType*, access_mode::discard_write> yValues(yProperty);
            for(size_t binIndex = 0; binIndex < table->elementCount(); ++binIndex) {
                for(size_t component = 0; component < outputComponentCount; ++component)
                    yValues.set(binIndex, component, firstDerivative ? sums[binIndex * outputComponentCount + component] : reducedValue(binIndex, component));
            }
            table->setY(yProperty);
        }
        else if(grid) {
            Property* binnedProperty = grid->createProperty(DataBuffer::Initialized,
                                                            propertyName,
                                                            Property::FloatDefault,
                                                            outputComponentCount,
                                                            outputComponentNames);
            BufferWriteAccess<FloatType*, access_mode::discard_write> values(binnedProperty);
            for(size_t binIndex = 0; binIndex < totalBinCount; ++binIndex) {
                for(size_t component = 0; component < outputComponentCount; ++component)
                    values.set(binIndex, component, reducedValue(binIndex, component));
            }

            if(VoxelGridVis* vis = grid->visElement<VoxelGridVis>()) {
                vis->colorMapping()->setSourceProperty(PropertyReference(binnedProperty, outputComponentCount > 1 ? 0 : -1));
            }
        }

        const size_t activeBins = (activeCount == 1) ? gridShape[firstActiveAxis(active)] : totalBinCount;
        QString statusText = SpatialBinningModifier::tr("Binned %1 particles into %2 spatial bins using %3.")
            .arg(includedParticleCount)
            .arg(activeBins)
            .arg(reductionLabel(reductionOperation));
        if(firstDerivative && table)
            statusText += SpatialBinningModifier::tr("\nComputed the first derivative of the 1D profile.");
        if(skippedOutsideCount != 0) {
            state.setStatus(PipelineStatus(PipelineStatus::Warning,
                statusText + SpatialBinningModifier::tr("\nIgnored %1 particles outside the simulation cell domain.").arg(skippedOutsideCount)));
        }
        else {
            state.setStatus(PipelineStatus(std::move(statusText)));
        }

        return std::move(state);
    });
}

}  // namespace Ovito
