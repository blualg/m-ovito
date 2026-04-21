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
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/objects/VoxelGridVis.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include "SpatialBinningModifier.h"

namespace Ovito {

namespace {

QString reductionLabel(SpatialBinningModifier::ReductionOperation op)
{
    switch(op) {
    case SpatialBinningModifier::Mean: return SpatialBinningModifier::tr("mean");
    case SpatialBinningModifier::Sum: return SpatialBinningModifier::tr("sum");
    case SpatialBinningModifier::SumDividedByBinVolume: return SpatialBinningModifier::tr("sum divided by bin volume");
    case SpatialBinningModifier::Min: return SpatialBinningModifier::tr("min");
    case SpatialBinningModifier::Max: return SpatialBinningModifier::tr("max");
    }
    OVITO_ASSERT(false);
    return {};
}

Vector3 binNormalX(const SimulationCell* cell, SpatialBinningModifier::BinDirection dir)
{
    switch(dir) {
    case SpatialBinningModifier::CellVector1:
    case SpatialBinningModifier::CellVectors12:
    case SpatialBinningModifier::CellVectors13:
        return cell->cellVector2().cross(cell->cellVector3());
    case SpatialBinningModifier::CellVector2:
    case SpatialBinningModifier::CellVectors23:
        return cell->cellVector3().cross(cell->cellVector1());
    case SpatialBinningModifier::CellVector3:
        return cell->cellVector1().cross(cell->cellVector2());
    }
    OVITO_ASSERT(false);
    return Vector3::Zero();
}

Vector3 binNormalY(const SimulationCell* cell, SpatialBinningModifier::BinDirection dir)
{
    switch(dir) {
    case SpatialBinningModifier::CellVectors12:
        return cell->cellVector3().cross(cell->cellVector1());
    case SpatialBinningModifier::CellVectors13:
    case SpatialBinningModifier::CellVectors23:
        return cell->cellVector1().cross(cell->cellVector2());
    case SpatialBinningModifier::CellVector1:
    case SpatialBinningModifier::CellVector2:
    case SpatialBinningModifier::CellVector3:
        return Vector3(1, 1, 1);
    }
    OVITO_ASSERT(false);
    return Vector3::Zero();
}

FloatType axisExtent(const SimulationCell* cell, const Vector3& normal)
{
    OVITO_ASSERT(normal != Vector3::Zero());
    return (cell->is2D() ? cell->volume2D() : cell->volume3D()) / normal.length();
}

QString outputPropertyName(const Property* property, int vectorComponent)
{
    if(!property)
        return {};
    if(vectorComponent >= 0)
        return property->nameWithComponent(vectorComponent);
    return property->name();
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(SpatialBinningModifier);
OVITO_CLASSINFO(SpatialBinningModifier, "DisplayName", "Bin and reduce");
OVITO_CLASSINFO(SpatialBinningModifier, "Description", "Bin particles along one or two cell vectors and reduce a particle property in each bin.");
OVITO_CLASSINFO(SpatialBinningModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, reductionOperation);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, firstDerivative);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, binDirection);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, numberOfBinsX);
DEFINE_PROPERTY_FIELD(SpatialBinningModifier, numberOfBinsY);
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, sourceProperty, "Particle property");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, reductionOperation, "Reduction operation");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, firstDerivative, "Compute first derivative");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, binDirection, "Binning direction");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, numberOfBinsX, "Number of spatial bins");
SET_PROPERTY_FIELD_LABEL(SpatialBinningModifier, numberOfBinsY, "Number of spatial bins");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialBinningModifier, numberOfBinsX, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialBinningModifier, numberOfBinsY, IntegerParameterUnit, 1);

bool SpatialBinningModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

bool SpatialBinningModifier::is1D() const
{
    return is1D(binDirection());
}

bool SpatialBinningModifier::is1D(BinDirection direction)
{
    return direction == CellVector1 || direction == CellVector2 || direction == CellVector3;
}

int SpatialBinningModifier::binDirectionX(BinDirection direction)
{
    return static_cast<int>(direction) & 3;
}

int SpatialBinningModifier::binDirectionY(BinDirection direction)
{
    return (static_cast<int>(direction) >> 2) & 3;
}

void SpatialBinningModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(!sourceProperty() && this_task::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const Particles* particles = input.getObject<Particles>()) {
            PropertyReference bestProperty;
            for(const Property* property : particles->properties()) {
                if(property->dataType() == Property::Int8 || property->dataType() == Property::Int32 ||
                   property->dataType() == Property::Int64 || property->dataType() == Property::Float32 ||
                   property->dataType() == Property::Float64) {
                    bestProperty = PropertyReference(property);
                }
            }
            if(bestProperty)
                setSourceProperty(bestProperty);
        }
    }
}

Future<PipelineFlowState> SpatialBinningModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    const SimulationCell* cell = state.getObject<SimulationCell>();
    if(!cell)
        throw Exception(tr("Bin and reduce requires a simulation cell."));
    if(cell->isDegenerate())
        throw Exception(tr("Cannot bin particles because the simulation cell is degenerate."));
    if(!sourceProperty())
        throw Exception(tr("No particle property selected."));

    QString propertyError;
    const Property* inputProperty = nullptr;
    int vectorComponent = -1;
    std::tie(inputProperty, vectorComponent) = sourceProperty().findInContainerWithComponent(particles, propertyError, false);
    if(!inputProperty)
        throw Exception(std::move(propertyError));

    const size_t outputComponentCount = (vectorComponent >= 0) ? 1 : inputProperty->componentCount();
    const QStringList outputComponentNames = (vectorComponent >= 0) ? QStringList{} : inputProperty->componentNames();

    const int axisX = binDirectionX(binDirection());
    const int axisY = binDirectionY(binDirection());
    if(cell->is2D() && (!is1D() ? (axisX == 2 || axisY == 2) : axisX == 2))
        throw Exception(tr("The selected binning direction is not available for a two-dimensional simulation cell."));

    const int binCountX = std::max(1, numberOfBinsX());
    const int binCountY = is1D() ? 1 : std::max(1, numberOfBinsY());
    const size_t totalBinCount = size_t(binCountX) * size_t(binCountY);

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    const bool periodicX = cell->hasPbcCorrected(axisX);
    const bool periodicY = !is1D() && cell->hasPbcCorrected(axisY);

    const Vector3 normalX = binNormalX(cell, binDirection());
    const Vector3 normalY = binNormalY(cell, binDirection());
    if(normalX == Vector3::Zero() || (!is1D() && normalY == Vector3::Zero()))
        throw Exception(tr("Simulation cell is degenerate."));

    const FloatType xAxisRangeStart = (cell->cellOrigin() - Point3::Origin()).dot(normalX.normalized());
    const FloatType xAxisRangeEnd = xAxisRangeStart + axisExtent(cell, normalX);
    const FloatType yAxisRangeStart = is1D() ? FloatType(0) : (cell->cellOrigin() - Point3::Origin()).dot(normalY.normalized());
    const FloatType yAxisRangeEnd = is1D() ? FloatType(0) : (yAxisRangeStart + axisExtent(cell, normalY));

    DataTable* table = nullptr;
    VoxelGrid* grid = nullptr;
    if(is1D()) {
        table = state.createObject<DataTable>(QString(OutputIdentifier), request.modificationNode(), DataTable::Line, tr("Bin and reduce"));
        table->setElementCount(binCountX);
        table->setAxisLabelX(tr("Position"));
        table->setAxisLabelY(outputPropertyName(inputProperty, vectorComponent));
        table->setIntervalStart(xAxisRangeStart);
        table->setIntervalEnd(xAxisRangeEnd);
    }
    else {
        grid = state.createObject<VoxelGrid>(request.modificationNode(), QString(OutputIdentifier));
        grid->setTitle(tr("Bin and reduce"));
        grid->setGridType(VoxelGrid::CellData);
        grid->setShape({size_t(binCountX), size_t(binCountY), size_t(1)});
        grid->setElementCount(totalBinCount);
        grid->setDomain(cell);
    }

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            cell,
            inputProperty,
            vectorComponent,
            outputComponentCount,
            outputComponentNames = std::move(outputComponentNames),
            binCountX,
            binCountY,
            totalBinCount,
            axisX,
            axisY,
            xAxisRangeStart,
            xAxisRangeEnd,
            yAxisRangeStart,
            yAxisRangeEnd,
            reductionOperation = reductionOperation(),
            firstDerivative = firstDerivative(),
            periodicX,
            periodicY,
            table,
            grid,
            is1D = is1D()]() mutable
    {
        std::vector<int64_t> counts(totalBinCount, 0);
        std::vector<double> reduced(totalBinCount * outputComponentCount, 0.0);

        auto applyValue = [&](size_t binIndex, size_t component, double value) {
            const size_t index = binIndex * outputComponentCount + component;
            if(reductionOperation == Mean || reductionOperation == Sum || reductionOperation == SumDividedByBinVolume) {
                reduced[index] += value;
            }
            else if(counts[index] == 0) {
                reduced[index] = value;
            }
            else if(reductionOperation == Min) {
                reduced[index] = std::min(reduced[index], value);
            }
            else if(reductionOperation == Max) {
                reduced[index] = std::max(reduced[index], value);
            }
            counts[index]++;
        };

        inputProperty->forAnyType([&](auto _) {
            using T = decltype(_);
            BufferReadAccess<T*> values(inputProperty);

            for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
                const Point3 reducedPos = cell->absoluteToReduced(positions[particleIndex]);
                int ix = static_cast<int>(reducedPos[axisX] * binCountX);
                int iy = is1D ? 0 : static_cast<int>(reducedPos[axisY] * binCountY);

                if(periodicX)
                    ix = SimulationCell::modulo(ix, binCountX);
                if(!is1D && periodicY)
                    iy = SimulationCell::modulo(iy, binCountY);

                if(ix < 0 || ix >= binCountX || iy < 0 || iy >= binCountY)
                    continue;

                const size_t binIndex = size_t(iy) * size_t(binCountX) + size_t(ix);
                for(size_t component = 0; component < outputComponentCount; ++component) {
                    const int sourceComponent = (vectorComponent >= 0) ? vectorComponent : static_cast<int>(component);
                    applyValue(binIndex, component, static_cast<double>(values.get(particleIndex, sourceComponent)));
                }
            }
        });

        const FloatType binVolume = (cell->is2D() ? cell->volume2D() : cell->volume3D()) / FloatType(totalBinCount);
        for(size_t i = 0; i < totalBinCount; ++i) {
            if(counts[i] == 0)
                continue;
            for(size_t component = 0; component < outputComponentCount; ++component) {
                const size_t index = i * outputComponentCount + component;
                if(reductionOperation == Mean)
                    reduced[index] /= static_cast<double>(counts[i]);
                else if(reductionOperation == SumDividedByBinVolume && binVolume > 0)
                    reduced[index] /= static_cast<double>(binVolume);
            }
        }

        if(is1D && firstDerivative && binCountX > 1) {
            const FloatType spacing = (xAxisRangeEnd - xAxisRangeStart) / FloatType(binCountX);
            if(spacing > FloatType(0)) {
                std::vector<double> derivative(binCountX * outputComponentCount, 0.0);
                for(int i = 0; i < binCountX; ++i) {
                    int plus = i + 1;
                    int minus = i - 1;
                    double denom = 2.0 * spacing;
                    if(plus >= binCountX) {
                        plus = periodicX ? 0 : (binCountX - 1);
                        if(!periodicX) denom = spacing;
                    }
                    if(minus < 0) {
                        minus = periodicX ? (binCountX - 1) : 0;
                        if(!periodicX) denom = spacing;
                    }
                    for(size_t component = 0; component < outputComponentCount; ++component) {
                        derivative[i * outputComponentCount + component] =
                            (reduced[plus * outputComponentCount + component] -
                             reduced[minus * outputComponentCount + component]) / denom;
                    }
                }
                reduced.swap(derivative);
            }
        }

        if(table) {
            Property* yProperty = table->createProperty(DataBuffer::Initialized,
                                                        outputPropertyName(inputProperty, vectorComponent),
                                                        Property::FloatDefault,
                                                        outputComponentCount,
                                                        outputComponentNames);
            BufferWriteAccess<FloatType*, access_mode::discard_write> yValues(yProperty);
            for(int i = 0; i < binCountX; ++i) {
                for(size_t component = 0; component < outputComponentCount; ++component)
                    yValues.set(i, component, static_cast<FloatType>(reduced[i * outputComponentCount + component]));
            }
            table->setY(yProperty);
        }
        else if(grid) {
            Property* binnedProperty = grid->createProperty(DataBuffer::Initialized,
                                                            outputPropertyName(inputProperty, vectorComponent),
                                                            Property::FloatDefault,
                                                            outputComponentCount,
                                                            outputComponentNames);
            BufferWriteAccess<FloatType*, access_mode::discard_write> values(binnedProperty);
            for(size_t i = 0; i < totalBinCount; ++i) {
                for(size_t component = 0; component < outputComponentCount; ++component)
                    values.set(i, component, static_cast<FloatType>(reduced[i * outputComponentCount + component]));
            }
            if(VoxelGridVis* vis = grid->visElement<VoxelGridVis>())
                vis->colorMapping()->setSourceProperty(PropertyReference(binnedProperty, outputComponentCount > 1 ? 0 : -1));
        }

        QString statusText = SpatialBinningModifier::tr("Computed %1 over %2 spatial bins using %3.")
            .arg(outputPropertyName(inputProperty, vectorComponent))
            .arg(totalBinCount)
            .arg(reductionLabel(reductionOperation));
        if(is1D && firstDerivative)
            statusText += SpatialBinningModifier::tr("\nComputed first derivative of the 1D profile.");
        state.setStatus(PipelineStatus(std::move(statusText)));
        return std::move(state);
    });
}

}  // namespace Ovito
