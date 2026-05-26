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
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "InterfacialIdentificationModifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(InterfacialIdentificationModifier);
OVITO_CLASSINFO(InterfacialIdentificationModifier, "DisplayName", "Identify interfacial particles");
OVITO_CLASSINFO(InterfacialIdentificationModifier, "Description", "Identifies interfacial particles using the ITIM or GITIM algorithms.");
OVITO_CLASSINFO(InterfacialIdentificationModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, method);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, probeSphereRadius);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, meshSpacing);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, radiusScale);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, maxLayers);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, normalAxis);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(InterfacialIdentificationModifier, selectInterfacialParticles);
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, method, "Method");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, probeSphereRadius, "Probe sphere radius");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, meshSpacing, "Mesh spacing");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, radiusScale, "Radius scaling");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, maxLayers, "Maximum layers");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, normalAxis, "Interface normal");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(InterfacialIdentificationModifier, selectInterfacialParticles, "Select interfacial particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(InterfacialIdentificationModifier, probeSphereRadius, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(InterfacialIdentificationModifier, meshSpacing, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(InterfacialIdentificationModifier, radiusScale, PercentParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(InterfacialIdentificationModifier, maxLayers, IntegerParameterUnit, 1);

namespace {

constexpr double TwoPi = 6.283185307179586476925286766559;

int axisToIndex(InterfacialIdentificationModifier::InterfaceNormalAxis axis)
{
    switch(axis) {
    case InterfacialIdentificationModifier::XAxis: return 0;
    case InterfacialIdentificationModifier::YAxis: return 1;
    case InterfacialIdentificationModifier::ZAxis: return 2;
    }
    return 2;
}

size_t positiveModulo(int value, size_t modulus)
{
    OVITO_ASSERT(modulus != 0);
    const int wrapped = value % static_cast<int>(modulus);
    return static_cast<size_t>(wrapped < 0 ? wrapped + static_cast<int>(modulus) : wrapped);
}

FloatType wrappedDistance1D(FloatType a, FloatType b, FloatType length, bool periodic)
{
    FloatType delta = a - b;
    if(periodic && length > FloatType(0)) {
        delta -= std::round(delta / length) * length;
    }
    return delta;
}

Point3 wrappedReducedCoordinates(const SimulationCell& cell, const Point3& position)
{
    Point3 reduced = cell.absoluteToReduced(position);
    for(size_t dim = 0; dim < 3; ++dim) {
        if(cell.hasPbcCorrected(dim))
            reduced[dim] -= std::floor(reduced[dim]);
    }
    return reduced;
}

double circularMeanCoordinate(const std::vector<Point3>& reducedCoordinates,
                              const std::vector<size_t>& candidateIndices,
                              int normalAxis,
                              bool periodic)
{
    if(candidateIndices.empty())
        return 0.5;

    if(!periodic) {
        double sum = 0.0;
        for(size_t index : candidateIndices)
            sum += reducedCoordinates[index][normalAxis];
        return sum / static_cast<double>(candidateIndices.size());
    }

    double sumSin = 0.0;
    double sumCos = 0.0;
    for(size_t index : candidateIndices) {
        const double angle = TwoPi * reducedCoordinates[index][normalAxis];
        sumSin += std::sin(angle);
        sumCos += std::cos(angle);
    }

    if(std::abs(sumSin) < 1e-12 && std::abs(sumCos) < 1e-12)
        return 0.5;

    double mean = std::atan2(sumSin, sumCos) / TwoPi;
    if(mean < 0.0)
        mean += 1.0;
    return mean;
}

double centeredReducedCoordinate(double reducedCoordinate, double centerCoordinate, bool periodic)
{
    double centered = reducedCoordinate - centerCoordinate;
    if(periodic)
        centered -= std::floor(centered + 0.5);
    return centered;
}

bool solveLinearSystem3x3(const double M[3][3], const double b[3], double x[3])
{
    const double det =
        M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
        M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
        M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);

    if(std::abs(det) < 1e-20)
        return false;

    const double invDet = 1.0 / det;
    const double inv[3][3] = {
        {
            (M[1][1] * M[2][2] - M[1][2] * M[2][1]) * invDet,
            (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * invDet,
            (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * invDet,
        },
        {
            (M[1][2] * M[2][0] - M[1][0] * M[2][2]) * invDet,
            (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * invDet,
            (M[0][2] * M[1][0] - M[0][0] * M[1][2]) * invDet,
        },
        {
            (M[1][0] * M[2][1] - M[1][1] * M[2][0]) * invDet,
            (M[0][1] * M[2][0] - M[0][0] * M[2][1]) * invDet,
            (M[0][0] * M[1][1] - M[0][1] * M[1][0]) * invDet,
        },
    };

    for(int row = 0; row < 3; ++row) {
        x[row] = inv[row][0] * b[0] + inv[row][1] * b[1] + inv[row][2] * b[2];
    }
    return true;
}

double weightedTouchingSphereRadius(const std::array<Point3, 4>& positions, const std::array<FloatType, 4>& radii)
{
    double r[4][3];
    double positionSquaredNorm[4] = {0.0, 0.0, 0.0, 0.0};
    double radiiValue[4];
    double radiiSquared[4];

    for(int i = 0; i < 4; ++i) {
        radiiValue[i] = radii[i];
        radiiSquared[i] = radiiValue[i] * radiiValue[i];
        for(int j = 0; j < 3; ++j) {
            r[i][j] = positions[i][j];
            positionSquaredNorm[i] += r[i][j] * r[i][j];
        }
    }

    double M[3][3];
    double d[3];
    double s[3];
    for(int i = 0; i < 3; ++i) {
        d[i] = radiiValue[0] - radiiValue[i + 1];
        s[i] = 0.5 * (positionSquaredNorm[0] - positionSquaredNorm[i + 1] - radiiSquared[0] + radiiSquared[i + 1]);
        for(int j = 0; j < 3; ++j) {
            M[j][i] = r[0][j] - r[i + 1][j];
        }
    }

    double u[3];
    double w[3];
    if(!solveLinearSystem3x3(M, d, u) || !solveLinearSystem3x3(M, s, w))
        return 0.0;

    double u2 = 0.0;
    double v2 = 0.0;
    double uv = 0.0;
    for(int i = 0; i < 3; ++i) {
        const double v = r[0][i] - w[i];
        v2 += v * v;
        u2 += u[i] * u[i];
        uv += u[i] * v;
    }

    const double A = radiiValue[0] - uv;
    const double discriminant = A * A - (u2 - 1.0) * (v2 - radiiSquared[0]);
    if(discriminant < 0.0)
        return 0.0;

    const double B = std::sqrt(discriminant);
    const double C = u2 - 1.0;
    if(std::abs(C) < 1e-20)
        return 0.0;

    const double root1 = (A + B) / C;
    const double root2 = (A - B) / C;

    double best = 0.0;
    if(root1 >= 0.0 && root2 >= 0.0)
        best = std::min(root1, root2);
    else if(root1 >= 0.0)
        best = root1;
    else if(root2 >= 0.0)
        best = root2;
    return best;
}

struct InterfacialIdentificationResults
{
    PropertyPtr layerProperty;
    PropertyPtr sideProperty;
    PropertyPtr selectionProperty;
    std::vector<int> upperCounts;
    std::vector<int> lowerCounts;
    std::vector<int> totalCounts;
    int interfacialCount = 0;
    int populatedLayers = 0;
    PipelineStatus status = PipelineStatus::Success;

    void applyResults(PipelineFlowState& state,
                      OOWeakRef<const PipelineNode> createdByNode,
                      bool createSelection) const
    {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(layerProperty);
        particles->createProperty(sideProperty);
        if(createSelection && selectionProperty)
            particles->createProperty(selectionProperty);

        if(populatedLayers > 0 && !totalCounts.empty()) {
            const int rowCount = populatedLayers;
            const bool planar = !upperCounts.empty() || !lowerCounts.empty();
            const int componentCount = planar ? 3 : 1;
            QStringList componentNames;
            if(planar)
                componentNames = {QObject::tr("Upper"), QObject::tr("Lower"), QObject::tr("Total")};
            else
                componentNames = {QObject::tr("Count")};

            PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                    rowCount,
                                                                    Property::FloatDefault,
                                                                    componentCount,
                                                                    QStringLiteral("Count"),
                                                                    0,
                                                                    componentNames);
            PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                    rowCount,
                                                                    Property::FloatDefault,
                                                                    1,
                                                                    QStringLiteral("Layer"));

            BufferWriteAccess<FloatType, access_mode::discard_write> xData(x);
            BufferWriteAccess<FloatType, access_mode::discard_write> yData(y);
            for(int row = 0; row < rowCount; ++row) {
                xData[row] = static_cast<FloatType>(row + 1);
                if(planar) {
                    yData[row * componentCount + 0] = static_cast<FloatType>(upperCounts[row]);
                    yData[row * componentCount + 1] = static_cast<FloatType>(lowerCounts[row]);
                    yData[row * componentCount + 2] = static_cast<FloatType>(totalCounts[row]);
                }
                else {
                    yData[row] = static_cast<FloatType>(totalCounts[row]);
                }
            }

            DataTable* table = state.createObject<DataTable>(InterfacialIdentificationModifier::LayerCountsTableId.toString(),
                                                             createdByNode,
                                                             DataTable::Line,
                                                             QObject::tr("Interfacial layer counts"),
                                                             std::move(y),
                                                             std::move(x));
            table->setAxisLabelX(QObject::tr("Layer"));
            table->setAxisLabelY(QObject::tr("Interfacial particles"));
        }

        state.addAttribute(QStringLiteral("InterfacialIdentification.interfacial_count"), QVariant::fromValue(interfacialCount), createdByNode);
        state.addAttribute(QStringLiteral("InterfacialIdentification.layer_count"), QVariant::fromValue(populatedLayers), createdByNode);
        state.setStatus(status);
    }
};

class InterfacialIdentificationEngine
{
public:
    InterfacialIdentificationEngine(const Property* positions,
                                    const Property* selection,
                                    ConstPropertyPtr radii,
                                    const SimulationCell* simCell,
                                    InterfacialIdentificationModifier::InterfaceMethod method,
                                    FloatType probeSphereRadius,
                                    FloatType meshSpacing,
                                    FloatType radiusScale,
                                    int maxLayers,
                                    InterfacialIdentificationModifier::InterfaceNormalAxis normalAxis)
        : _positions(positions),
          _selection(selection),
          _radii(std::move(radii)),
          _simCell(simCell),
          _method(method),
          _probeSphereRadius(probeSphereRadius),
          _meshSpacing(meshSpacing),
          _radiusScale(radiusScale),
          _maxLayers(maxLayers),
          _normalAxis(normalAxis)
    {
    }

    InterfacialIdentificationResults perform() const
    {
        TaskProgress progress(this_task::ui());
        progress.setText(QObject::tr("Identifying interfacial particles"));

        const size_t particleCount = _positions->size();
        InterfacialIdentificationResults results;
        results.layerProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                        particleCount,
                                                                        DataBuffer::Int32,
                                                                        1,
                                                                        InterfacialIdentificationModifier::InterfaceLayerPropertyName);
        results.sideProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                       particleCount,
                                                                       DataBuffer::Int32,
                                                                       1,
                                                                       InterfacialIdentificationModifier::InterfaceSidePropertyName);
        results.selectionProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized,
                                                                                particleCount,
                                                                                Particles::SelectionProperty);

        BufferReadAccess<Point3> positions(_positions);
        BufferReadAccess<GraphicsFloatType> radii(_radii);
        BufferReadAccess<SelectionIntType> selection(_selection);
        BufferWriteAccess<int32_t, access_mode::read_write> layers(results.layerProperty);
        BufferWriteAccess<int32_t, access_mode::read_write> sides(results.sideProperty);
        BufferWriteAccess<SelectionIntType, access_mode::read_write> selected(results.selectionProperty);

        std::vector<size_t> candidateIndices;
        candidateIndices.reserve(particleCount);
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            if(selection && !selection[particleIndex])
                continue;
            candidateIndices.push_back(particleIndex);
        }

        if(candidateIndices.empty()) {
            results.status = PipelineStatus(PipelineStatus::Warning, QObject::tr("No candidate particles available for interface identification."));
            return results;
        }

        if(!_simCell || _simCell->isDegenerate())
            throw Exception(QObject::tr("The interface-identification modifier requires a valid simulation cell."));

        if(_method == InterfacialIdentificationModifier::ITIM)
            runITIM(candidateIndices, positions, radii, layers, sides, progress, results);
        else
            runGITIM(candidateIndices, positions, radii, layers, sides, progress, results);

        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            if(layers[particleIndex] > 0) {
                selected[particleIndex] = 1;
                ++results.interfacialCount;
            }
        }

        while(!results.totalCounts.empty() && results.totalCounts.back() == 0) {
            results.totalCounts.pop_back();
            if(!results.upperCounts.empty())
                results.upperCounts.pop_back();
            if(!results.lowerCounts.empty())
                results.lowerCounts.pop_back();
        }
        results.populatedLayers = static_cast<int>(results.totalCounts.size());

        if(_method == InterfacialIdentificationModifier::ITIM) {
            const int upperFirst = !results.upperCounts.empty() ? results.upperCounts.front() : 0;
            const int lowerFirst = !results.lowerCounts.empty() ? results.lowerCounts.front() : 0;
            results.status = PipelineStatus(PipelineStatus::Success,
                                            QObject::tr("ITIM identified %1 interfacial particle(s) across %2 layer(s) (%3 upper + %4 lower in the first layer).")
                                                .arg(results.interfacialCount)
                                                .arg(results.populatedLayers)
                                                .arg(upperFirst)
                                                .arg(lowerFirst));
        }
        else {
            const int firstLayer = !results.totalCounts.empty() ? results.totalCounts.front() : 0;
            results.status = PipelineStatus(PipelineStatus::Success,
                                            QObject::tr("GITIM identified %1 interfacial particle(s) across %2 layer(s) (%3 in the first layer).")
                                                .arg(results.interfacialCount)
                                                .arg(results.populatedLayers)
                                                .arg(firstLayer));
        }

        return results;
    }

private:

    void runITIM(const std::vector<size_t>& candidateIndices,
                 const BufferReadAccess<Point3>& positions,
                 const BufferReadAccess<GraphicsFloatType>& radii,
                 BufferWriteAccess<int32_t, access_mode::read_write>& layers,
                 BufferWriteAccess<int32_t, access_mode::read_write>& sides,
                 TaskProgress& progress,
                 InterfacialIdentificationResults& results) const
    {
        if(!_simCell->isAxisAligned())
            throw Exception(QObject::tr("The ITIM implementation currently requires an orthorhombic simulation cell."));
        if(_meshSpacing <= 0)
            throw Exception(QObject::tr("The ITIM mesh spacing must be positive."));

        const int normalDim = axisToIndex(_normalAxis);
        const int lateralDim1 = (normalDim + 1) % 3;
        const int lateralDim2 = (normalDim + 2) % 3;
        const std::array<FloatType, 3> cellLengths = {
            _simCell->cellVector1().length(),
            _simCell->cellVector2().length(),
            _simCell->cellVector3().length(),
        };
        if(cellLengths[lateralDim1] <= 0 || cellLengths[lateralDim2] <= 0 || cellLengths[normalDim] <= 0)
            throw Exception(QObject::tr("The simulation cell dimensions must be positive."));

        const size_t particleCount = _positions->size();
        std::vector<Point3> reducedCoordinates(particleCount);
        std::vector<FloatType> centeredNormalCoordinate(particleCount, 0);
        std::vector<FloatType> lateralX(particleCount, 0);
        std::vector<FloatType> lateralY(particleCount, 0);
        std::vector<FloatType> scaledRadii(particleCount, 0);

        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
            reducedCoordinates[particleIndex] = wrappedReducedCoordinates(*_simCell, positions[particleIndex]);

        const double slabCenter = circularMeanCoordinate(reducedCoordinates, candidateIndices, normalDim, _simCell->hasPbcCorrected(normalDim));
        for(size_t particleIndex : candidateIndices) {
            centeredNormalCoordinate[particleIndex] =
                static_cast<FloatType>(centeredReducedCoordinate(reducedCoordinates[particleIndex][normalDim], slabCenter, _simCell->hasPbcCorrected(normalDim)) * cellLengths[normalDim]);
            lateralX[particleIndex] = reducedCoordinates[particleIndex][lateralDim1] * cellLengths[lateralDim1];
            lateralY[particleIndex] = reducedCoordinates[particleIndex][lateralDim2] * cellLengths[lateralDim2];
            scaledRadii[particleIndex] = std::max(FloatType(0), static_cast<FloatType>(_radiusScale * radii[particleIndex]));
        }

        const size_t nx = std::max<size_t>(1, static_cast<size_t>(std::floor(cellLengths[lateralDim1] / _meshSpacing + FloatType(0.5))));
        const size_t ny = std::max<size_t>(1, static_cast<size_t>(std::floor(cellLengths[lateralDim2] / _meshSpacing + FloatType(0.5))));
        const FloatType dx = cellLengths[lateralDim1] / static_cast<FloatType>(nx);
        const FloatType dy = cellLengths[lateralDim2] / static_cast<FloatType>(ny);
        const bool pbcX = _simCell->hasPbcCorrected(lateralDim1);
        const bool pbcY = _simCell->hasPbcCorrected(lateralDim2);

        std::vector<size_t> upperSide;
        std::vector<size_t> lowerSide;
        upperSide.reserve(candidateIndices.size());
        lowerSide.reserve(candidateIndices.size());
        for(size_t particleIndex : candidateIndices) {
            if(centeredNormalCoordinate[particleIndex] >= 0)
                upperSide.push_back(particleIndex);
            else
                lowerSide.push_back(particleIndex);
        }

        std::ranges::sort(upperSide, [&](size_t left, size_t right) {
            return centeredNormalCoordinate[left] + scaledRadii[left] > centeredNormalCoordinate[right] + scaledRadii[right];
        });
        std::ranges::sort(lowerSide, [&](size_t left, size_t right) {
            return centeredNormalCoordinate[left] - scaledRadii[left] < centeredNormalCoordinate[right] - scaledRadii[right];
        });

        auto assignSide = [&](const std::vector<size_t>& sortedIndices, int32_t sideValue, std::vector<int>& counts) {
            counts.assign(_maxLayers, 0);
            for(int layer = 0; layer < _maxLayers; ++layer) {
                std::vector<unsigned char> touched(nx * ny, 0);
                size_t touchedCount = 0;
                int assignedThisLayer = 0;
                for(size_t particleIndex : sortedIndices) {
                    if(layers[particleIndex] != 0)
                        continue;

                    const FloatType touchingRadius = _probeSphereRadius + scaledRadii[particleIndex];
                    if(touchingRadius <= 0)
                        continue;

                    const int ixMin = static_cast<int>(std::floor((lateralX[particleIndex] - touchingRadius) / dx));
                    const int ixMax = static_cast<int>(std::floor((lateralX[particleIndex] + touchingRadius) / dx));
                    const int iyMin = static_cast<int>(std::floor((lateralY[particleIndex] - touchingRadius) / dy));
                    const int iyMax = static_cast<int>(std::floor((lateralY[particleIndex] + touchingRadius) / dy));

                    bool touchedNewLine = false;
                    for(int iy = iyMin; iy <= iyMax; ++iy) {
                        const bool validY = (iy >= 0 && static_cast<size_t>(iy) < ny);
                        if(!pbcY && !validY)
                            continue;
                        const size_t wrappedY = pbcY ? positiveModulo(iy, ny) : static_cast<size_t>(iy);
                        const FloatType gridY = static_cast<FloatType>(wrappedY) * dy;
                        const FloatType deltaY = wrappedDistance1D(lateralY[particleIndex], gridY, cellLengths[lateralDim2], pbcY);
                        for(int ix = ixMin; ix <= ixMax; ++ix) {
                            const bool validX = (ix >= 0 && static_cast<size_t>(ix) < nx);
                            if(!pbcX && !validX)
                                continue;
                            const size_t wrappedX = pbcX ? positiveModulo(ix, nx) : static_cast<size_t>(ix);
                            const FloatType gridX = static_cast<FloatType>(wrappedX) * dx;
                            const FloatType deltaX = wrappedDistance1D(lateralX[particleIndex], gridX, cellLengths[lateralDim1], pbcX);
                            if(deltaX * deltaX + deltaY * deltaY > touchingRadius * touchingRadius)
                                continue;
                            const size_t lineIndex = wrappedY * nx + wrappedX;
                            if(!touched[lineIndex]) {
                                touched[lineIndex] = 1;
                                ++touchedCount;
                                touchedNewLine = true;
                            }
                        }
                    }

                    if(!touchedNewLine)
                        continue;

                    layers[particleIndex] = layer + 1;
                    sides[particleIndex] = sideValue;
                    ++assignedThisLayer;
                    if(touchedCount == touched.size())
                        break;
                }

                counts[layer] = assignedThisLayer;
                if(assignedThisLayer == 0 || touchedCount < touched.size())
                    break;
            }
        };

        progress.setMaximum(candidateIndices.size());
        assignSide(upperSide, 1, results.upperCounts);
        assignSide(lowerSide, -1, results.lowerCounts);

        const size_t rowCount = std::max(results.upperCounts.size(), results.lowerCounts.size());
        results.totalCounts.assign(rowCount, 0);
        for(size_t row = 0; row < rowCount; ++row) {
            const int upper = row < results.upperCounts.size() ? results.upperCounts[row] : 0;
            const int lower = row < results.lowerCounts.size() ? results.lowerCounts[row] : 0;
            results.totalCounts[row] = upper + lower;
        }
    }

    void runGITIM(const std::vector<size_t>& candidateIndices,
                  const BufferReadAccess<Point3>& positions,
                  const BufferReadAccess<GraphicsFloatType>& radii,
                  BufferWriteAccess<int32_t, access_mode::read_write>& layers,
                  BufferWriteAccess<int32_t, access_mode::read_write>& sides,
                  TaskProgress& progress,
                  InterfacialIdentificationResults& results) const
    {
        constexpr FloatType BufferFactor = FloatType(3.5);
        results.totalCounts.assign(_maxLayers, 0);

        const size_t particleCount = _positions->size();
        std::vector<Point3> wrappedPositions(particleCount);
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
            wrappedPositions[particleIndex] = _simCell->hasPbcCorrected() ? _simCell->wrapPoint(positions[particleIndex]) : positions[particleIndex];

        for(int layer = 0; layer < _maxLayers; ++layer) {
            std::vector<size_t> activeParticles;
            activeParticles.reserve(candidateIndices.size());
            for(size_t particleIndex : candidateIndices) {
                if(layers[particleIndex] == 0)
                    activeParticles.push_back(particleIndex);
            }
            if(activeParticles.size() < 4)
                break;

            progress.setText(QObject::tr("Identifying GITIM layer %1").arg(layer + 1));

            std::vector<Point3> layerPositions;
            layerPositions.reserve(activeParticles.size());
            std::vector<FloatType> layerRadii;
            layerRadii.reserve(activeParticles.size());
            for(size_t particleIndex : activeParticles) {
                layerPositions.push_back(wrappedPositions[particleIndex]);
                layerRadii.push_back(std::max(FloatType(0), static_cast<FloatType>(_radiusScale * radii[particleIndex])));
            }

            DelaunayTessellation tessellation;
            tessellation.generateTessellation(_simCell,
                                             layerPositions.data(),
                                             layerPositions.size(),
                                             _probeSphereRadius * BufferFactor,
                                             false,
                                             nullptr,
                                             progress);
            this_task::throwIfCanceled();

            const auto cellCount = tessellation.numberOfTetrahedra();
            std::vector<unsigned char> validCells(cellCount, 0);
            std::vector<unsigned char> accessibleCells(cellCount, 0);
            std::vector<std::array<size_t, 4>> cellLocalIndices(cellCount);

            for(auto cell : tessellation.cells()) {
                if(!tessellation.isFiniteCell(cell) || tessellation.isGhostCell(cell))
                    continue;

                std::array<size_t, 4> localIndices{};
                std::array<Point3, 4> tetrahedronPositions{};
                std::array<FloatType, 4> tetrahedronRadii{};
                bool skipCell = false;
                for(int vertexIndex = 0; vertexIndex < 4; ++vertexIndex) {
                    const auto vertex = tessellation.cellVertex(cell, vertexIndex);
                    const size_t localPointIndex = tessellation.inputPointIndex(vertex);
                    if(localPointIndex == std::numeric_limits<size_t>::max() || localPointIndex >= activeParticles.size()) {
                        skipCell = true;
                        break;
                    }
                    for(int other = 0; other < vertexIndex; ++other) {
                        if(localIndices[other] == localPointIndex) {
                            skipCell = true;
                            break;
                        }
                    }
                    if(skipCell)
                        break;
                    localIndices[vertexIndex] = localPointIndex;
                    tetrahedronPositions[vertexIndex] = tessellation.vertexPosition(vertex);
                    tetrahedronRadii[vertexIndex] = layerRadii[localPointIndex];
                }
                if(skipCell)
                    continue;

                validCells[cell] = 1;
                cellLocalIndices[cell] = localIndices;
                if(weightedTouchingSphereRadius(tetrahedronPositions, tetrahedronRadii) >= _probeSphereRadius)
                    accessibleCells[cell] = 1;
            }

            std::vector<unsigned char> markedLocal(activeParticles.size(), 0);
            for(auto cell : tessellation.cells()) {
                if(!validCells[cell] || !accessibleCells[cell])
                    continue;

                for(int facetIndex = 0; facetIndex < 4; ++facetIndex) {
                    const auto adjacentCell = tessellation.cellAdjacent(cell, facetIndex);
                    bool boundaryFacet = false;
                    if(adjacentCell < 0 || adjacentCell >= tessellation.numberOfTetrahedra()) {
                        boundaryFacet = true;
                    }
                    else if(tessellation.isGhostCell(adjacentCell) || !tessellation.isFiniteCell(adjacentCell)) {
                        boundaryFacet = true;
                    }
                    else if(!validCells[adjacentCell] || !accessibleCells[adjacentCell]) {
                        boundaryFacet = true;
                    }
                    if(!boundaryFacet)
                        continue;

                    for(int facetVertex = 0; facetVertex < 3; ++facetVertex) {
                        const int localVertexIndex = DelaunayTessellation::cellFacetVertexIndex(facetIndex, facetVertex);
                        markedLocal[cellLocalIndices[cell][localVertexIndex]] = 1;
                    }
                }
            }

            int assignedThisLayer = 0;
            for(size_t localIndex = 0; localIndex < activeParticles.size(); ++localIndex) {
                if(!markedLocal[localIndex])
                    continue;
                const size_t particleIndex = activeParticles[localIndex];
                layers[particleIndex] = layer + 1;
                sides[particleIndex] = 1;
                ++assignedThisLayer;
            }

            results.totalCounts[layer] = assignedThisLayer;
            if(assignedThisLayer == 0)
                break;
        }
    }

    const Property* _positions;
    const Property* _selection;
    ConstPropertyPtr _radii;
    const SimulationCell* _simCell;
    InterfacialIdentificationModifier::InterfaceMethod _method;
    FloatType _probeSphereRadius;
    FloatType _meshSpacing;
    FloatType _radiusScale;
    int _maxLayers;
    InterfacialIdentificationModifier::InterfaceNormalAxis _normalAxis;
};

}   // End of anonymous namespace

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool InterfacialIdentificationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void InterfacialIdentificationModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                            PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                            TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> InterfacialIdentificationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                particles->tryToAdoptProperties(cachedParticles, {
                    cachedParticles->getProperty(InterfaceLayerPropertyName),
                    cachedParticles->getProperty(InterfaceSidePropertyName),
                    selectInterfacialParticles() ? cachedParticles->getProperty(Particles::SelectionProperty) : nullptr,
                }, {particles});
            }
            if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), LayerCountsTableId.toString()))
                state.addObject(cachedTable);
            state.adoptAttributesFrom(cachedState, request.modificationNode());
            state.setStatus(cachedState.status());
        }
        return std::move(state);
    }

    const Property* positionProperty = particles->expectProperty(Particles::PositionProperty);
    const Property* selectionProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;
    ConstPropertyPtr radii = particles->inputParticleRadii();
    const SimulationCell* simCell = state.getObject<SimulationCell>();

    InterfacialIdentificationEngine engine(positionProperty,
                                           selectionProperty,
                                           std::move(radii),
                                           simCell,
                                           method(),
                                           probeSphereRadius(),
                                           meshSpacing(),
                                           radiusScale(),
                                           maxLayers(),
                                           normalAxis());

    return asyncLaunch([
            state = std::move(state),
            engine = std::move(engine),
            modificationNode = request.modificationNodeWeak(),
            createSelection = selectInterfacialParticles()]() mutable
    {
        InterfacialIdentificationResults results = engine.perform();
        this_task::throwIfCanceled();
        results.applyResults(state, modificationNode, createSelection);
        return std::move(state);
    });
}

}   // End of namespace
