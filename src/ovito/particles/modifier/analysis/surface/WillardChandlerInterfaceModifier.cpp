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
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/mesh/surface/SurfaceMeshRegions.h>
#include <ovito/grid/modifier/MarchingCubes.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "WillardChandlerInterfaceModifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <QRegularExpression>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WillardChandlerInterfaceModifier);
OVITO_CLASSINFO(WillardChandlerInterfaceModifier, "DisplayName", "Willard-Chandler interface");
OVITO_CLASSINFO(WillardChandlerInterfaceModifier, "Description", "Constructs a smooth instantaneous Willard-Chandler interface and identifies vapor, liquid, and interfacial particles.");
OVITO_CLASSINFO(WillardChandlerInterfaceModifier, "ModifierCategory", "Analysis");
DEFINE_REFERENCE_FIELD(WillardChandlerInterfaceModifier, surfaceMeshVis);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, gaussianWidth);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, isoValue);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, gridResolution);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, interfacialThickness);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, selectInterfacialParticles);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, selectVaporParticles);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, extendSelection);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, selectionExpression);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, correctDetachedClusters);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateNormalDirection);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateReferenceSource);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateGapMode);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, detachedClusterGapCutoff);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, detachedClusterBottomPercentile);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateTopPercentile);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, detachedClusterMinimumSupportAtoms);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateTopCoordinate);
DEFINE_PROPERTY_FIELD(WillardChandlerInterfaceModifier, plateSelectionExpression);
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, gaussianWidth, "Gaussian width");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, isoValue, "Density isovalue");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, gridResolution, "Grid resolution");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, interfacialThickness, "Interfacial thickness");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, onlySelectedParticles, "Use only selected particles to build the interface");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, selectInterfacialParticles, "Select interfacial particles");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, selectVaporParticles, "Select vapor particles");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, extendSelection, "Extend existing selection");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, selectionExpression, "Additional selection expression");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, correctDetachedClusters, "Correct detached clusters");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateNormalDirection, "Plate normal");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateReferenceSource, "Plate reference source");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateGapMode, "Plate top reference");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, detachedClusterGapCutoff, "Detached cluster gap cutoff");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, detachedClusterBottomPercentile, "Cluster bottom percentile");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateTopPercentile, "Plate top percentile");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, detachedClusterMinimumSupportAtoms, "Minimum support atoms");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateTopCoordinate, "Plate top coordinate");
SET_PROPERTY_FIELD_LABEL(WillardChandlerInterfaceModifier, plateSelectionExpression, "Plate selection expression");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WillardChandlerInterfaceModifier, gaussianWidth, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WillardChandlerInterfaceModifier, isoValue, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(WillardChandlerInterfaceModifier, gridResolution, IntegerParameterUnit, 8, 600);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WillardChandlerInterfaceModifier, interfacialThickness, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WillardChandlerInterfaceModifier, detachedClusterGapCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(WillardChandlerInterfaceModifier, detachedClusterBottomPercentile, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(WillardChandlerInterfaceModifier, plateTopPercentile, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WillardChandlerInterfaceModifier, detachedClusterMinimumSupportAtoms, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS(WillardChandlerInterfaceModifier, plateTopCoordinate, WorldParameterUnit);

namespace {

std::vector<uint8_t> evaluateSelectionExpressionMask(const PipelineFlowState& state,
                                                     const Particles* particles,
                                                     const QString& expression);

struct WillardChandlerResults
{
    struct CavityRegionInfo {
        int32_t regionId = -1;
        FloatType volume = 0;
        FloatType surfaceArea = 0;
    };
    struct FilledRegionInfo {
        int32_t regionId = -1;
        FloatType volume = 0;
        FloatType surfaceArea = 0;
    };

    PropertyPtr distanceProperty;
    PropertyPtr phaseProperty;
    PropertyPtr selectionProperty;
    std::vector<int32_t> particleRegions;
    std::vector<uint8_t> regionFilledMask;
    FloatType surfaceArea = 0;
    int vaporCount = 0;
    int interfacialCount = 0;
    int liquidCount = 0;
    int liquidRegionCount = 0;
    int vaporRegionCount = 0;
    int detachedRegionCount = 0;
    int detachedRelabeledParticleCount = 0;
    int cavityRegionCount = 0;
    FloatType totalCavityVolume = 0;
    FloatType totalCavitySurfaceArea = 0;
    std::vector<CavityRegionInfo> cavityRegions;
    int nonExteriorFilledRegionCount = 0;
    FloatType totalNonExteriorFilledRegionVolume = 0;
    FloatType totalNonExteriorFilledRegionSurfaceArea = 0;
    std::vector<FilledRegionInfo> filledRegions;

    void recomputePhaseCounts()
    {
        vaporCount = 0;
        interfacialCount = 0;
        liquidCount = 0;
        if(!phaseProperty)
            return;

        BufferReadAccess<int32_t> phases(phaseProperty);
        for(size_t particleIndex = 0; particleIndex < phases.size(); ++particleIndex) {
            switch(phases[particleIndex]) {
            case WillardChandlerInterfaceModifier::Liquid:
                ++liquidCount;
                break;
            case WillardChandlerInterfaceModifier::Interfacial:
                ++interfacialCount;
                break;
            default:
                ++vaporCount;
                break;
            }
        }
    }

    void rebuildSelectionProperty(bool selectInterfacialParticles,
                                  bool selectVaporParticles)
    {
        if(!phaseProperty)
            return;

        selectionProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized,
                                                                        phaseProperty->size(),
                                                                        Particles::SelectionProperty);
        BufferReadAccess<int32_t> phases(phaseProperty);
        BufferWriteAccess<SelectionIntType, access_mode::read_write> selection(selectionProperty);
        for(size_t particleIndex = 0; particleIndex < phases.size(); ++particleIndex) {
            const int32_t phase = phases[particleIndex];
            selection[particleIndex] = (
                (selectInterfacialParticles && phase == WillardChandlerInterfaceModifier::Interfacial) ||
                (selectVaporParticles && phase == WillardChandlerInterfaceModifier::Vapor)
            ) ? 1 : 0;
        }
    }

    void applyResults(PipelineFlowState& state,
                      const OOWeakRef<const PipelineNode>& createdByNode,
                      bool createSelection) const
    {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->verifyIntegrity();
        particles->createProperty(distanceProperty);
        particles->createProperty(phaseProperty);
        if(createSelection && selectionProperty)
            particles->createProperty(selectionProperty);

        PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                3,
                                                                Property::FloatDefault,
                                                                1,
                                                                QStringLiteral("Phase"));
        PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                3,
                                                                Property::FloatDefault,
                                                                1,
                                                                QStringLiteral("Count"));
        BufferWriteAccess<FloatType, access_mode::discard_write> xData(x);
        BufferWriteAccess<FloatType, access_mode::discard_write> yData(y);
        xData[0] = static_cast<FloatType>(WillardChandlerInterfaceModifier::Vapor);
        xData[1] = static_cast<FloatType>(WillardChandlerInterfaceModifier::Interfacial);
        xData[2] = static_cast<FloatType>(WillardChandlerInterfaceModifier::Liquid);
        yData[0] = static_cast<FloatType>(vaporCount);
        yData[1] = static_cast<FloatType>(interfacialCount);
        yData[2] = static_cast<FloatType>(liquidCount);

        DataTable* countsTable = state.createObject<DataTable>(WillardChandlerInterfaceModifier::PhaseCountsTableId.toString(),
                                                               createdByNode,
                                                               DataTable::BarChart,
                                                               QObject::tr("Phase counts"),
                                                               std::move(y),
                                                               std::move(x));
        countsTable->setAxisLabelX(QObject::tr("Phase label (-1 vapor, 0 interfacial, 1 liquid)"));
        countsTable->setAxisLabelY(QObject::tr("Particle count"));

        if(!cavityRegions.empty()) {
            PropertyPtr cavityIndex = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                              cavityRegions.size(),
                                                                              Property::FloatDefault,
                                                                              1,
                                                                              QStringLiteral("Cavity"));
            PropertyPtr cavityMetrics = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                cavityRegions.size(),
                                                                                Property::FloatDefault,
                                                                                2,
                                                                                QStringLiteral("Metric"),
                                                                                0,
                                                                                QStringList{QStringLiteral("Volume"),
                                                                                            QStringLiteral("Surface Area")});
            PropertyPtr cavityRegionIds = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                  cavityRegions.size(),
                                                                                  Property::Int32,
                                                                                  1,
                                                                                  QStringLiteral("Mesh Region ID"));

            BufferWriteAccess<FloatType, access_mode::discard_write> cavityIndexData(cavityIndex);
            BufferWriteAccess<FloatType, access_mode::discard_write> cavityMetricData(cavityMetrics);
            BufferWriteAccess<int32_t, access_mode::discard_write> cavityRegionIdData(cavityRegionIds);
            for(size_t cavityIndexValue = 0; cavityIndexValue < cavityRegions.size(); ++cavityIndexValue) {
                cavityIndexData[cavityIndexValue] = static_cast<FloatType>(cavityIndexValue + 1);
                cavityMetricData[cavityIndexValue * 2] = cavityRegions[cavityIndexValue].volume;
                cavityMetricData[cavityIndexValue * 2 + 1] = cavityRegions[cavityIndexValue].surfaceArea;
                cavityRegionIdData[cavityIndexValue] = cavityRegions[cavityIndexValue].regionId;
            }

            DataTable* cavityTable = state.createObject<DataTable>(WillardChandlerInterfaceModifier::CavityTableId.toString(),
                                                                   createdByNode,
                                                                   DataTable::Scatter,
                                                                   QObject::tr("Cavity regions"),
                                                                   std::move(cavityMetrics),
                                                                   std::move(cavityIndex));
            cavityTable->setAxisLabelX(QObject::tr("Cavity index"));
            cavityTable->setAxisLabelY(QObject::tr("Cavity metric"));
            cavityTable->createProperty(std::move(cavityRegionIds));
        }

        if(!filledRegions.empty()) {
            PropertyPtr filledIndex = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                              filledRegions.size(),
                                                                              Property::FloatDefault,
                                                                              1,
                                                                              QStringLiteral("Filled Region"));
            PropertyPtr filledMetrics = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                filledRegions.size(),
                                                                                Property::FloatDefault,
                                                                                2,
                                                                                QStringLiteral("Metric"),
                                                                                0,
                                                                                QStringList{QStringLiteral("Volume"),
                                                                                            QStringLiteral("Surface Area")});
            PropertyPtr filledRegionIds = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                  filledRegions.size(),
                                                                                  Property::Int32,
                                                                                  1,
                                                                                  QStringLiteral("Mesh Region ID"));

            BufferWriteAccess<FloatType, access_mode::discard_write> filledIndexData(filledIndex);
            BufferWriteAccess<FloatType, access_mode::discard_write> filledMetricData(filledMetrics);
            BufferWriteAccess<int32_t, access_mode::discard_write> filledRegionIdData(filledRegionIds);
            for(size_t filledIndexValue = 0; filledIndexValue < filledRegions.size(); ++filledIndexValue) {
                filledIndexData[filledIndexValue] = static_cast<FloatType>(filledIndexValue + 1);
                filledMetricData[filledIndexValue * 2] = filledRegions[filledIndexValue].volume;
                filledMetricData[filledIndexValue * 2 + 1] = filledRegions[filledIndexValue].surfaceArea;
                filledRegionIdData[filledIndexValue] = filledRegions[filledIndexValue].regionId;
            }

            DataTable* filledTable = state.createObject<DataTable>(WillardChandlerInterfaceModifier::FilledRegionTableId.toString(),
                                                                   createdByNode,
                                                                   DataTable::Scatter,
                                                                   QObject::tr("Filled regions"),
                                                                   std::move(filledMetrics),
                                                                   std::move(filledIndex));
            filledTable->setAxisLabelX(QObject::tr("Filled-region index"));
            filledTable->setAxisLabelY(QObject::tr("Filled-region metric"));
            filledTable->createProperty(std::move(filledRegionIds));
        }

        state.addAttribute(QStringLiteral("WillardChandler.surface_area"), QVariant::fromValue(surfaceArea), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.vapor_count"), QVariant::fromValue(vaporCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.interfacial_count"), QVariant::fromValue(interfacialCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.liquid_count"), QVariant::fromValue(liquidCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.vapor_region_count"), QVariant::fromValue(vaporRegionCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.liquid_region_count"), QVariant::fromValue(liquidRegionCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.cavity_region_count"), QVariant::fromValue(cavityRegionCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.cavity_volume"), QVariant::fromValue(totalCavityVolume), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.cavity_surface_area"), QVariant::fromValue(totalCavitySurfaceArea), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.non_exterior_filled_region_count"), QVariant::fromValue(nonExteriorFilledRegionCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.non_exterior_filled_region_volume"), QVariant::fromValue(totalNonExteriorFilledRegionVolume), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.non_exterior_filled_region_surface_area"), QVariant::fromValue(totalNonExteriorFilledRegionSurfaceArea), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.detached_region_count"), QVariant::fromValue(detachedRegionCount), createdByNode);
        state.addAttribute(QStringLiteral("WillardChandler.detached_reclassified_count"), QVariant::fromValue(detachedRelabeledParticleCount), createdByNode);

        QString statusText =
            QObject::tr("Willard-Chandler identified %1 liquid, %2 interfacial, and %3 vapor particle(s).")
                .arg(liquidCount)
                .arg(interfacialCount)
                .arg(vaporCount);
        if(cavityRegionCount > 0) {
            statusText += QObject::tr(" Detected %1 cavity region(s) with total volume %2 and total surface area %3.")
                              .arg(cavityRegionCount)
                              .arg(totalCavityVolume)
                              .arg(totalCavitySurfaceArea);
        }
        if(nonExteriorFilledRegionCount > 0) {
            statusText += QObject::tr(" Detected %1 non-exterior filled region(s) with total volume %2 and total surface area %3.")
                              .arg(nonExteriorFilledRegionCount)
                              .arg(totalNonExteriorFilledRegionVolume)
                              .arg(totalNonExteriorFilledRegionSurfaceArea);
        }
        if(detachedRelabeledParticleCount > 0) {
            statusText += QObject::tr(" Detached-cluster correction relabeled %1 particle(s) in %2 detached region(s) to vapor.")
                              .arg(detachedRelabeledParticleCount)
                              .arg(detachedRegionCount);
        }
        state.setStatus(PipelineStatus(PipelineStatus::Success, statusText));
    }
};

struct PlateNormalInfo
{
    int axis = 2;
    int lateralAxis1 = 0;
    int lateralAxis2 = 1;
    FloatType sign = 1;
};

PlateNormalInfo plateNormalInfo(WillardChandlerInterfaceModifier::PlateNormalDirection direction)
{
    using PlateNormalDirection = WillardChandlerInterfaceModifier::PlateNormalDirection;
    switch(direction) {
    case PlateNormalDirection::PositiveX:
        return { 0, 1, 2, FloatType(1) };
    case PlateNormalDirection::NegativeX:
        return { 0, 1, 2, FloatType(-1) };
    case PlateNormalDirection::PositiveY:
        return { 1, 0, 2, FloatType(1) };
    case PlateNormalDirection::NegativeY:
        return { 1, 0, 2, FloatType(-1) };
    case PlateNormalDirection::PositiveZ:
        return { 2, 0, 1, FloatType(1) };
    case PlateNormalDirection::NegativeZ:
        return { 2, 0, 1, FloatType(-1) };
    }
    return {};
}

FloatType projectOntoPlateNormal(const Point3& position, const PlateNormalInfo& info)
{
    return position[info.axis] * info.sign;
}

FloatType percentileValue(std::vector<FloatType> values, FloatType percentile)
{
    OVITO_ASSERT(!values.empty());
    const FloatType clampedPercentile = std::clamp(percentile, FloatType(0), FloatType(100));
    const size_t lastIndex = values.size() - 1;
    const size_t targetIndex = std::min(
        lastIndex,
        static_cast<size_t>(std::floor((clampedPercentile / FloatType(100)) * static_cast<FloatType>(lastIndex))));
    std::nth_element(values.begin(), values.begin() + targetIndex, values.end());
    return values[targetIndex];
}

void applyDetachedClusterCorrection(WillardChandlerResults& results,
                                    const PipelineFlowState& state,
                                    WillardChandlerInterfaceModifier::PlateNormalDirection plateNormalDirection,
                                    WillardChandlerInterfaceModifier::PlateReferenceSource plateReferenceSource,
                                    WillardChandlerInterfaceModifier::PlateGapMode plateGapMode,
                                    FloatType detachedClusterGapCutoff,
                                    FloatType detachedClusterBottomPercentile,
                                    FloatType plateTopPercentile,
                                    int detachedClusterMinimumSupportAtoms,
                                    FloatType plateTopCoordinate,
                                    FloatType interfacialShellThickness,
                                    const QString& plateSelectionExpression,
                                    FloatType localFootprintMargin,
                                    bool selectInterfacialParticles,
                                    bool selectVaporParticles)
{
    const Particles* particles = state.expectObject<Particles>();
    if(!particles || particles->elementCount() == 0 || !results.phaseProperty)
        return;

    const Property* positionProperty = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<Point3> positions(positionProperty);
    BufferWriteAccess<int32_t, access_mode::read_write> mutablePhases(results.phaseProperty);
    const PlateNormalInfo normalInfo = plateNormalInfo(plateNormalDirection);

    std::vector<size_t> plateAtomIndices;
    plateAtomIndices.reserve(positions.size());
    std::vector<FloatType> plateProjections;
    plateProjections.reserve(positions.size());
    FloatType globalPlateTop = plateTopCoordinate * normalInfo.sign;

    if(plateReferenceSource == WillardChandlerInterfaceModifier::PlateAtomsExpression) {
        const QString trimmedPlateExpression = plateSelectionExpression.trimmed();
        if(trimmedPlateExpression.isEmpty())
            throw Exception(QObject::tr("Detached-cluster correction is enabled, but no plate selection expression was specified."));

        const std::vector<uint8_t> plateMask = evaluateSelectionExpressionMask(state, particles, trimmedPlateExpression);
        for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
            if(!plateMask[particleIndex])
                continue;
            plateAtomIndices.push_back(particleIndex);
            plateProjections.push_back(projectOntoPlateNormal(positions[particleIndex], normalInfo));
        }
        if(plateAtomIndices.empty())
            throw Exception(QObject::tr("Detached-cluster correction could not identify any plate atoms from the plate selection expression."));
        globalPlateTop = percentileValue(plateProjections, plateTopPercentile);
    }

    const size_t regionCount = results.regionFilledMask.size();
    if(regionCount == 0 || results.particleRegions.empty())
        return;

    std::vector<int> regionParticleCounts(regionCount, 0);
    std::vector<FloatType> regionBottomProjection(regionCount, FLOATTYPE_MAX);
    std::vector<FloatType> regionMaxProjection(regionCount, -FLOATTYPE_MAX);
    std::vector<FloatType> regionMinLateral1(regionCount, FLOATTYPE_MAX);
    std::vector<FloatType> regionMaxLateral1(regionCount, -FLOATTYPE_MAX);
    std::vector<FloatType> regionMinLateral2(regionCount, FLOATTYPE_MAX);
    std::vector<FloatType> regionMaxLateral2(regionCount, -FLOATTYPE_MAX);
    std::vector<std::vector<FloatType>> regionProjections(regionCount);

    for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
        const int32_t region = results.particleRegions[particleIndex];
        if(region < 0 || region >= static_cast<int32_t>(regionCount) || !results.regionFilledMask[region])
            continue;
        ++regionParticleCounts[region];
        const Point3 position = positions[particleIndex];
        const FloatType projection = projectOntoPlateNormal(position, normalInfo);
        regionProjections[region].push_back(projection);
        regionMaxProjection[region] = std::max(regionMaxProjection[region], projection);
        regionMinLateral1[region] = std::min(regionMinLateral1[region], position[normalInfo.lateralAxis1]);
        regionMaxLateral1[region] = std::max(regionMaxLateral1[region], position[normalInfo.lateralAxis1]);
        regionMinLateral2[region] = std::min(regionMinLateral2[region], position[normalInfo.lateralAxis2]);
        regionMaxLateral2[region] = std::max(regionMaxLateral2[region], position[normalInfo.lateralAxis2]);
    }

    int primaryFilledRegion = -1;
    FloatType primaryGap = FLOATTYPE_MAX;
    for(size_t regionIndex = 0; regionIndex < regionCount; ++regionIndex) {
        if(!results.regionFilledMask[regionIndex] || regionParticleCounts[regionIndex] == 0)
            continue;
        const FloatType regionBottom = percentileValue(regionProjections[regionIndex], detachedClusterBottomPercentile);
        regionBottomProjection[regionIndex] = regionBottom;
        const FloatType gap = regionBottom - globalPlateTop;
        if(primaryFilledRegion < 0 || gap < primaryGap ||
           (gap == primaryGap && regionParticleCounts[regionIndex] > regionParticleCounts[primaryFilledRegion])) {
            primaryFilledRegion = static_cast<int>(regionIndex);
            primaryGap = gap;
        }
    }
    if(primaryFilledRegion < 0)
        return;

    auto computeLocalPlateTop = [&](size_t regionIndex) {
        if(plateReferenceSource == WillardChandlerInterfaceModifier::FixedCoordinate)
            return globalPlateTop;

        const FloatType minLateral1 = regionMinLateral1[regionIndex] - localFootprintMargin;
        const FloatType maxLateral1 = regionMaxLateral1[regionIndex] + localFootprintMargin;
        const FloatType minLateral2 = regionMinLateral2[regionIndex] - localFootprintMargin;
        const FloatType maxLateral2 = regionMaxLateral2[regionIndex] + localFootprintMargin;

        std::vector<FloatType> localProjections;
        for(size_t plateIndex : plateAtomIndices) {
            const Point3 position = positions[plateIndex];
            const FloatType lateral1 = position[normalInfo.lateralAxis1];
            const FloatType lateral2 = position[normalInfo.lateralAxis2];
            if(lateral1 < minLateral1 || lateral1 > maxLateral1 ||
               lateral2 < minLateral2 || lateral2 > maxLateral2) {
                continue;
            }
            localProjections.push_back(projectOntoPlateNormal(position, normalInfo));
        }
        if(localProjections.empty())
            return globalPlateTop;
        return percentileValue(std::move(localProjections), plateTopPercentile);
    };

    std::vector<uint8_t> relabelRegion(regionCount, 0);
    for(size_t regionIndex = 0; regionIndex < regionCount; ++regionIndex) {
        if(static_cast<int>(regionIndex) == primaryFilledRegion || !results.regionFilledMask[regionIndex] || regionParticleCounts[regionIndex] == 0)
            continue;

        const FloatType plateTop = (plateGapMode == WillardChandlerInterfaceModifier::LocalPlateTop)
            ? computeLocalPlateTop(regionIndex)
            : globalPlateTop;
        const FloatType regionBottom = regionBottomProjection[regionIndex];
        const FloatType gap = regionBottom - plateTop;
        int supportCount = 0;
        for(FloatType projection : regionProjections[regionIndex]) {
            if(projection <= plateTop + detachedClusterGapCutoff)
                ++supportCount;
        }
        if(gap > detachedClusterGapCutoff || supportCount < detachedClusterMinimumSupportAtoms)
            relabelRegion[regionIndex] = 1;
    }

    results.detachedRegionCount = 0;
    results.detachedRelabeledParticleCount = 0;
    for(size_t regionIndex = 0; regionIndex < regionCount; ++regionIndex) {
        if(relabelRegion[regionIndex])
            ++results.detachedRegionCount;
    }
    if(results.detachedRegionCount == 0)
        return;

    const FloatType shellThickness = std::max(interfacialShellThickness, FloatType(0));
    for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
        bool shouldRelabel = false;
        const int32_t region = results.particleRegions[particleIndex];
        if(region >= 0 && region < static_cast<int32_t>(regionCount) && relabelRegion[region]) {
            shouldRelabel = true;
        }
        else {
            const Point3 position = positions[particleIndex];
            const FloatType projection = projectOntoPlateNormal(position, normalInfo);
            const FloatType lateral1 = position[normalInfo.lateralAxis1];
            const FloatType lateral2 = position[normalInfo.lateralAxis2];

            for(size_t regionIndex = 0; regionIndex < regionCount; ++regionIndex) {
                if(!relabelRegion[regionIndex])
                    continue;
                if(projection < regionBottomProjection[regionIndex] - shellThickness ||
                   projection > regionMaxProjection[regionIndex] + shellThickness ||
                   lateral1 < regionMinLateral1[regionIndex] - shellThickness ||
                   lateral1 > regionMaxLateral1[regionIndex] + shellThickness ||
                   lateral2 < regionMinLateral2[regionIndex] - shellThickness ||
                   lateral2 > regionMaxLateral2[regionIndex] + shellThickness) {
                    continue;
                }
                shouldRelabel = true;
                break;
            }
        }

        if(shouldRelabel && mutablePhases[particleIndex] != WillardChandlerInterfaceModifier::Vapor) {
            mutablePhases[particleIndex] = WillardChandlerInterfaceModifier::Vapor;
            ++results.detachedRelabeledParticleCount;
        }
    }

    results.recomputePhaseCounts();
    results.rebuildSelectionProperty(selectInterfacialParticles, selectVaporParticles);
}

std::vector<uint8_t> evaluateSelectionExpressionMask(const PipelineFlowState& state,
                                                     const Particles* particles,
                                                     const QString& expression)
{
    const QString trimmedExpression = expression.trimmed();
    std::vector<uint8_t> mask(particles->elementCount(), 0);
    if(trimmedExpression.isEmpty())
        return mask;

    static const QRegularExpression assignmentRegex(QStringLiteral("[^=!><]=(?!=)"));
    if(trimmedExpression.contains(assignmentRegex)) {
        throw Exception(QObject::tr(
            "The additional selection expression contains the assignment operator '='. "
            "Please use the comparison operator '==' instead."));
    }

    ParticleExpressionEvaluator evaluator;
    ConstDataObjectPath containerPath = state.data()->expectObject(DataObjectReference(&Particles::OOClass()));
    const int animationFrame = state.data() ? std::max(0, state.data()->sourceFrame()) : 0;
    evaluator.initialize(QStringList(trimmedExpression), state, containerPath, animationFrame);

    PropertyExpressionEvaluator::Worker worker(evaluator);
    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
        if(worker.evaluate(particleIndex, 0))
            mask[particleIndex] = 1;
    }
    return mask;
}

class WillardChandlerEngine
{
public:
    WillardChandlerEngine(ConstPropertyPtr positions,
                          ConstPropertyPtr selection,
                          SurfaceMesh* mesh,
                          const SimulationCell* domain,
                          FloatType gaussianWidth,
                          FloatType isoValue,
                          int gridResolution,
                          FloatType interfacialThickness,
                          bool selectInterfacialParticles,
                          bool selectVaporParticles,
                          OOWeakRef<const PipelineNode> createdByNode)
        : _positions(std::move(positions)),
          _selection(std::move(selection)),
          _mesh(std::move(mesh)),
          _domain(domain),
          _gaussianWidth(gaussianWidth),
          _isoValue(isoValue),
          _gridResolution(gridResolution),
          _interfacialThickness(interfacialThickness),
          _selectInterfacialParticles(selectInterfacialParticles),
          _selectVaporParticles(selectVaporParticles),
          _createdByNode(std::move(createdByNode))
    {
    }

    WillardChandlerResults perform() const
    {
        TaskProgress progress(this_task::ui());
        progress.setText(QObject::tr("Constructing Willard-Chandler interface"));

        if(!_domain || _domain->isDegenerate())
            throw Exception(QObject::tr("Willard-Chandler interface construction requires a valid simulation cell."));
        if(_gaussianWidth <= 0)
            throw Exception(QObject::tr("Gaussian width must be positive."));
        if(_isoValue <= 0)
            throw Exception(QObject::tr("Density isovalue must be positive."));
        if(_gridResolution < 2)
            throw Exception(QObject::tr("Grid resolution must be at least 2."));

        const size_t particleCount = _positions->size();
        BufferReadAccess<Point3> positions(_positions);
        BufferReadAccess<SelectionIntType> selection(_selection);

        std::vector<size_t> sourceParticleIndices;
        sourceParticleIndices.reserve(particleCount);
        for(size_t index = 0; index < particleCount; ++index) {
            if(selection && !selection[index])
                continue;
            sourceParticleIndices.push_back(index);
        }
        if(sourceParticleIndices.empty())
            throw Exception(QObject::tr("No particles are available to construct the Willard-Chandler interface."));

        progress.beginSubSteps({ 1, 24, 20, 6, 4, 10, 1 });

        const FloatType cutoffSize = FloatType(3) * _gaussianWidth;

        AffineTransformation gridBoundaries = _domain->matrix();
        const AffineTransformation inverseCellMatrix = _domain->inverseMatrix();
        for(size_t dim = 0; dim < 3; ++dim) {
            if(!_domain->hasPbc(dim)) {
                FloatType xmin = FLOATTYPE_MAX;
                FloatType xmax = -FLOATTYPE_MAX;
                for(size_t particleIndex : sourceParticleIndices) {
                    const FloatType reduced = inverseCellMatrix.prodrow(positions[particleIndex], dim);
                    xmin = std::min(xmin, reduced);
                    xmax = std::max(xmax, reduced);
                }

                const FloatType margin = cutoffSize / std::max(gridBoundaries.column(dim).length(), FloatType(1e-12));
                xmin -= margin;
                xmax += margin;
                gridBoundaries.column(3) += xmin * gridBoundaries.column(dim);
                gridBoundaries.column(dim) *= (xmax - xmin);
            }
        }
        progress.nextSubStep();

        size_t gridDims[3];
        const FloatType voxelSizeX = gridBoundaries.column(0).length() / _gridResolution;
        const FloatType voxelSizeY = gridBoundaries.column(1).length() / _gridResolution;
        const FloatType voxelSizeZ = gridBoundaries.column(2).length() / _gridResolution;
        const FloatType voxelSize = std::max(voxelSizeX, std::max(voxelSizeY, voxelSizeZ));
        gridDims[0] = std::max<size_t>(2, static_cast<size_t>(gridBoundaries.column(0).length() / voxelSize));
        gridDims[1] = std::max<size_t>(2, static_cast<size_t>(gridBoundaries.column(1).length() / voxelSize));
        gridDims[2] = std::max<size_t>(2, static_cast<size_t>(gridBoundaries.column(2).length() / voxelSize));

        std::vector<FloatType> densityData(gridDims[0] * gridDims[1] * gridDims[2], FloatType(0));
        CutoffNeighborFinder neighFinder(cutoffSize, _positions, _domain, _selection);
        progress.nextSubStep();

        AffineTransformation gridToCartesian = gridBoundaries;
        gridToCartesian.column(0) /= gridDims[0] - (_domain->hasPbc(0) ? 0 : 1);
        gridToCartesian.column(1) /= gridDims[1] - (_domain->hasPbc(1) ? 0 : 1);
        gridToCartesian.column(2) /= gridDims[2] - (_domain->hasPbc(2) ? 0 : 1);

        const FloatType sigma = _gaussianWidth;
        const FloatType sigmaSquared = sigma * sigma;
        const FloatType normalization = FloatType(1) /
            std::pow(FloatType(2) * std::numbers::pi_v<FloatType> * sigmaSquared, FloatType(1.5));

        parallelFor(densityData.size(), 4096, progress, [&](size_t voxelIndex) {
            const size_t ix = voxelIndex % gridDims[0];
            const size_t iy = (voxelIndex / gridDims[0]) % gridDims[1];
            const size_t iz = voxelIndex / (gridDims[0] * gridDims[1]);
            const Point3 voxelCenter = gridToCartesian * Point3(ix, iy, iz);

            FloatType density = 0;
            for(CutoffNeighborFinder::Query neighQuery(neighFinder, voxelCenter); !neighQuery.atEnd(); neighQuery.next()) {
                density += normalization * std::exp(-neighQuery.distanceSquared() / (FloatType(2) * sigmaSquared));
            }
            densityData[voxelIndex] = density;
        });
        progress.nextSubStep();

        auto getFieldValue = [
            data = densityData.data(),
            pbcFlags = _domain->pbcFlags(),
            gridShape = std::array<size_t, 3>{gridDims[0], gridDims[1], gridDims[2]}
        ](int i, int j, int k) -> FloatType {
            if(pbcFlags[0]) {
                if(i == static_cast<int>(gridShape[0])) i = 0;
            }
            else {
                if(i == 0 || i == static_cast<int>(gridShape[0]) + 1) return std::numeric_limits<FloatType>::lowest();
                --i;
            }
            if(pbcFlags[1]) {
                if(j == static_cast<int>(gridShape[1])) j = 0;
            }
            else {
                if(j == 0 || j == static_cast<int>(gridShape[1]) + 1) return std::numeric_limits<FloatType>::lowest();
                --j;
            }
            if(pbcFlags[2]) {
                if(k == static_cast<int>(gridShape[2])) k = 0;
            }
            else {
                if(k == 0 || k == static_cast<int>(gridShape[2]) + 1) return std::numeric_limits<FloatType>::lowest();
                --k;
            }
            OVITO_ASSERT(i >= 0 && i < static_cast<int>(gridShape[0]));
            OVITO_ASSERT(j >= 0 && j < static_cast<int>(gridShape[1]));
            OVITO_ASSERT(k >= 0 && k < static_cast<int>(gridShape[2]));
            return data[i + j * gridShape[0] + k * gridShape[0] * gridShape[1]];
        };

        DataOORef<const SimulationCell> originalDomain = _mesh->domain();
        if(_mesh->domain()->cellMatrix() != gridBoundaries) {
            auto newCell = DataOORef<SimulationCell>::makeCopy(_mesh->domain());
            newCell->setCellMatrix(gridBoundaries);
            _mesh->setDomain(std::move(newCell));
        }

        SurfaceMeshBuilder meshBuilder(_mesh);
        meshBuilder.createFaceProperty(DataBuffer::Uninitialized, SurfaceMeshFaces::RegionProperty);
        {
            MarchingCubes mc(meshBuilder, gridDims[0], gridDims[1], gridDims[2], false, std::move(getFieldValue));
            mc.generateIsosurface(_isoValue, progress);
        }
        progress.nextSubStep();

        meshBuilder.transformVertices(gridToCartesian);
        this_task::throwIfCanceled();

        const FloatType determinant = gridToCartesian.determinant();
        BufferWriteAccess<FloatType, access_mode::read_write> regionVolumes = meshBuilder.mutableRegionProperty(SurfaceMeshRegions::VolumeProperty);
        for(SurfaceMesh::region_index region : meshBuilder.regionsRange())
            regionVolumes[region] *= determinant;
        regionVolumes.reset();

        if(determinant < 0)
            meshBuilder.flipFaces();

        meshBuilder.setDomain(std::move(originalDomain));
        if(meshBuilder.faceCount() != 0 && !meshBuilder.connectOppositeHalfedges())
            throw Exception(QObject::tr("The generated Willard-Chandler surface mesh is not closed."));
        meshBuilder.setExternalRegionVolumeInfinityIfNonPeriodic();

        const FloatType surfaceArea = meshBuilder.computeSurfaceAreaWithRegions();
        progress.nextSubStep();

        WillardChandlerResults results;
        results.surfaceArea = surfaceArea;
        results.distanceProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                           particleCount,
                                                                           Property::FloatDefault,
                                                                           1,
                                                                           WillardChandlerInterfaceModifier::DistancePropertyName);
        results.phaseProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                        particleCount,
                                                                        DataBuffer::Int32,
                                                                        1,
                                                                        WillardChandlerInterfaceModifier::PhasePropertyName);
        results.selectionProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized,
                                                                                particleCount,
                                                                                Particles::SelectionProperty);
        results.particleRegions.assign(particleCount, -1);

        BufferWriteAccess<FloatType, access_mode::read_write> distances(results.distanceProperty);
        BufferWriteAccess<int32_t, access_mode::read_write> phases(results.phaseProperty);
        BufferWriteAccess<SelectionIntType, access_mode::read_write> interfacialSelection(results.selectionProperty);
        BufferReadAccess<SelectionIntType> regionFilled(meshBuilder.expectRegionProperty(SurfaceMeshRegions::IsFilledProperty));
        BufferReadAccess<SelectionIntType> regionExterior(meshBuilder.expectRegionProperty(SurfaceMeshRegions::IsExteriorProperty));
        BufferReadAccess<FloatType> regionVolumeValues(meshBuilder.expectRegionProperty(SurfaceMeshRegions::VolumeProperty));
        BufferReadAccess<FloatType> regionSurfaceAreaValues(meshBuilder.expectRegionProperty(SurfaceMeshRegions::SurfaceAreaProperty));
        results.regionFilledMask.resize(regionFilled.size(), 0);
        for(size_t regionIndex = 0; regionIndex < regionFilled.size(); ++regionIndex)
            results.regionFilledMask[regionIndex] = regionFilled[regionIndex] ? 1 : 0;

        for(SurfaceMesh::region_index region : meshBuilder.regionsRange()) {
            if(regionFilled[region]) {
                ++results.liquidRegionCount;
                if(!regionExterior[region]) {
                    results.filledRegions.push_back(WillardChandlerResults::FilledRegionInfo{
                        static_cast<int32_t>(region),
                        regionVolumeValues[region],
                        regionSurfaceAreaValues[region]
                    });
                    ++results.nonExteriorFilledRegionCount;
                    results.totalNonExteriorFilledRegionVolume += regionVolumeValues[region];
                    results.totalNonExteriorFilledRegionSurfaceArea += regionSurfaceAreaValues[region];
                }
            }
            else if(regionExterior[region] || region == meshBuilder.spaceFillingRegion())
                ++results.vaporRegionCount;
            else {
                results.cavityRegions.push_back(WillardChandlerResults::CavityRegionInfo{
                    static_cast<int32_t>(region),
                    regionVolumeValues[region],
                    regionSurfaceAreaValues[region]
                });
                ++results.cavityRegionCount;
                results.totalCavityVolume += regionVolumeValues[region];
                results.totalCavitySurfaceArea += regionSurfaceAreaValues[region];
            }
        }
        std::sort(results.cavityRegions.begin(), results.cavityRegions.end(),
                  [](const WillardChandlerResults::CavityRegionInfo& left,
                     const WillardChandlerResults::CavityRegionInfo& right) {
                      if(left.volume != right.volume)
                          return left.volume > right.volume;
                      return left.regionId < right.regionId;
                  });
        std::sort(results.filledRegions.begin(), results.filledRegions.end(),
                  [](const WillardChandlerResults::FilledRegionInfo& left,
                     const WillardChandlerResults::FilledRegionInfo& right) {
                      if(left.volume != right.volume)
                          return left.volume > right.volume;
                      return left.regionId < right.regionId;
                  });

        if(meshBuilder.faceCount() == 0) {
            const bool filled = regionFilled[meshBuilder.spaceFillingRegion()];
            for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                distances[particleIndex] = 0;
                phases[particleIndex] = filled ? WillardChandlerInterfaceModifier::Liquid : WillardChandlerInterfaceModifier::Vapor;
                results.particleRegions[particleIndex] = meshBuilder.spaceFillingRegion();
                interfacialSelection[particleIndex] = (
                    (_selectVaporParticles && phases[particleIndex] == WillardChandlerInterfaceModifier::Vapor) ||
                    (_selectInterfacialParticles && phases[particleIndex] == WillardChandlerInterfaceModifier::Interfacial)
                ) ? 1 : 0;
            }
            if(filled)
                results.liquidCount = static_cast<int>(particleCount);
            else
                results.vaporCount = static_cast<int>(particleCount);
            return results;
        }

        progress.setText(QObject::tr("Classifying particles relative to the interface"));
        parallelFor(particleCount, 256, progress, [&](size_t particleIndex) {
            const auto location = positions[particleIndex];
            const auto located = meshBuilder.locatePoint(location, 0.0);
            if(!located) {
                distances[particleIndex] = 0;
                phases[particleIndex] = WillardChandlerInterfaceModifier::Interfacial;
                results.particleRegions[particleIndex] = -1;
                interfacialSelection[particleIndex] = 1;
                return;
            }

            const auto [region, distance] = *located;
            results.particleRegions[particleIndex] = region;
            distances[particleIndex] = distance;

            int32_t phase = regionFilled[region] ? WillardChandlerInterfaceModifier::Liquid : WillardChandlerInterfaceModifier::Vapor;
            if(distance <= _interfacialThickness)
                phase = WillardChandlerInterfaceModifier::Interfacial;
            phases[particleIndex] = phase;
            interfacialSelection[particleIndex] = (
                (_selectInterfacialParticles && phase == WillardChandlerInterfaceModifier::Interfacial) ||
                (_selectVaporParticles && phase == WillardChandlerInterfaceModifier::Vapor)
            ) ? 1 : 0;
        });
        progress.nextSubStep();

        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            switch(phases[particleIndex]) {
            case WillardChandlerInterfaceModifier::Liquid:
                ++results.liquidCount;
                break;
            case WillardChandlerInterfaceModifier::Interfacial:
                ++results.interfacialCount;
                break;
            default:
                ++results.vaporCount;
                break;
            }
        }

        progress.endSubSteps();
        return results;
    }

private:
    ConstPropertyPtr _positions;
    ConstPropertyPtr _selection;
    SurfaceMesh* _mesh;
    const SimulationCell* _domain;
    FloatType _gaussianWidth;
    FloatType _isoValue;
    int _gridResolution;
    FloatType _interfacialThickness;
    bool _selectInterfacialParticles;
    bool _selectVaporParticles;
    OOWeakRef<const PipelineNode> _createdByNode;
};

} // namespace

/******************************************************************************
* Constructor.
******************************************************************************/
void WillardChandlerInterfaceModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject))
        setSurfaceMeshVis(OORef<SurfaceMeshVis>::create(flags));
}

/******************************************************************************
* Replaces a visual element owned by the modifier.
******************************************************************************/
void WillardChandlerInterfaceModifier::replaceVisualElement(DataVis* visElement,
                                                            const std::function<OORef<DataVis>(const QString&)>& getReplacement)
{
    if(surfaceMeshVis() == visElement)
        setSurfaceMeshVis(static_object_cast<SurfaceMeshVis>(getReplacement({})));
}

/******************************************************************************
* Checks if the modifier can be applied to the input data.
******************************************************************************/
bool WillardChandlerInterfaceModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Evaluates the modifier.
******************************************************************************/
Future<PipelineFlowState> WillardChandlerInterfaceModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);
    const Property* selProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    const SimulationCell* simCell = state.getObject<SimulationCell>();
    if(simCell && simCell->is2D())
        throw Exception(tr("The Willard-Chandler interface modifier does not support 2D simulation cells."));

    SurfaceMesh* mesh = state.createObjectWithVis<SurfaceMesh>(SurfaceMeshObjectId.toString(),
                                                               request.modificationNode(),
                                                               surfaceMeshVis(),
                                                               tr("Willard-Chandler interface"));
    if(simCell) {
        mesh->setDomain(simCell);
    }
    else {
        SimulationCellData cellData(posProperty);
        mesh->setDomain(DataOORef<SimulationCell>::create(ObjectInitializationFlag::DontCreateVisElement,
                                                          cellData.cellMatrix(),
                                                          false,
                                                          false,
                                                          false));
    }

    auto engine = std::make_unique<WillardChandlerEngine>(posProperty,
                                                          selProperty,
                                                          mesh,
                                                          simCell,
                                                          gaussianWidth(),
                                                          isoValue(),
                                                          gridResolution(),
                                                          interfacialThickness(),
                                                          selectInterfacialParticles(),
                                                          selectVaporParticles(),
                                                          request.modificationNode());

    const QString selectionExpressionText = selectionExpression().trimmed();
    const QString plateSelectionExpressionText = plateSelectionExpression().trimmed();
    const bool extendExistingSelection = extendSelection();
    const bool correctDetachedClusterClassification = correctDetachedClusters();
    const PlateNormalDirection currentPlateNormalDirection = plateNormalDirection();
    const PlateReferenceSource currentPlateReferenceSource = plateReferenceSource();
    const PlateGapMode currentPlateGapMode = plateGapMode();
    const FloatType currentDetachedClusterGapCutoff = detachedClusterGapCutoff();
    const FloatType currentDetachedClusterBottomPercentile = detachedClusterBottomPercentile();
    const FloatType currentPlateTopPercentile = plateTopPercentile();
    const int currentDetachedClusterMinimumSupportAtoms = detachedClusterMinimumSupportAtoms();
    const FloatType currentPlateTopCoordinate = plateTopCoordinate();
    const FloatType currentInterfacialShellThickness = interfacialThickness();
    const FloatType localFootprintMargin = gaussianWidth();
    const bool createSelection = selectInterfacialParticles() || selectVaporParticles()
                              || extendExistingSelection || !selectionExpressionText.isEmpty();

    return asyncLaunch([
        state = std::move(state),
        engine = std::move(engine),
        createdByNode = OOWeakRef<const PipelineNode>(request.modificationNode()),
        createSelection,
        extendExistingSelection,
        selectionExpressionText,
        correctDetachedClusterClassification,
        currentPlateNormalDirection,
        currentPlateReferenceSource,
        currentPlateGapMode,
        currentDetachedClusterGapCutoff,
        currentDetachedClusterBottomPercentile,
        currentPlateTopPercentile,
        currentDetachedClusterMinimumSupportAtoms,
        currentPlateTopCoordinate,
        currentInterfacialShellThickness,
        plateSelectionExpressionText,
        localFootprintMargin,
        selectInterfacialParticles = selectInterfacialParticles(),
        selectVaporParticles = selectVaporParticles()
    ]() mutable {
        WillardChandlerResults results = engine->perform();
        this_task::throwIfCanceled();

        if(correctDetachedClusterClassification) {
            applyDetachedClusterCorrection(results,
                                           state,
                                           currentPlateNormalDirection,
                                           currentPlateReferenceSource,
                                           currentPlateGapMode,
                                           currentDetachedClusterGapCutoff,
                                           currentDetachedClusterBottomPercentile,
                                           currentPlateTopPercentile,
                                           currentDetachedClusterMinimumSupportAtoms,
                                           currentPlateTopCoordinate,
                                           currentInterfacialShellThickness,
                                           plateSelectionExpressionText,
                                           localFootprintMargin,
                                           selectInterfacialParticles,
                                           selectVaporParticles);
        }

        if(createSelection) {
            const Particles* particles = state.expectObject<Particles>();
            const size_t particleCount = particles->elementCount();
            std::vector<uint8_t> mergedSelectionMask(particleCount, 0);

            if(results.selectionProperty) {
                BufferReadAccess<SelectionIntType> wcSelection(results.selectionProperty);
                for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                    if(wcSelection[particleIndex])
                        mergedSelectionMask[particleIndex] = 1;
                }
            }

            if(extendExistingSelection) {
                if(const Property* existingSelectionProperty = particles->getProperty(Particles::SelectionProperty)) {
                    BufferReadAccess<SelectionIntType> existingSelection(existingSelectionProperty);
                    for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                        if(existingSelection[particleIndex])
                            mergedSelectionMask[particleIndex] = 1;
                    }
                }
            }

            if(!selectionExpressionText.isEmpty()) {
                const std::vector<uint8_t> expressionMask =
                    evaluateSelectionExpressionMask(state, particles, selectionExpressionText);
                for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                    if(expressionMask[particleIndex])
                        mergedSelectionMask[particleIndex] = 1;
                }
            }

            results.selectionProperty = createSelectionPropertyFromMask(mergedSelectionMask);
        }

        results.applyResults(state, createdByNode, createSelection);
        return std::move(state);
    });
}

}   // End of namespace
