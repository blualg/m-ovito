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

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/table/DataTable.h>

namespace Ovito::OrientationTrajectoryAnalysis {

enum class DescriptorMode {
    DipoleVector,
    AtomTypeCentroidVector,
    MatchingPairVector
};

enum class VectorSubsetMode {
    AllElements,
    SelectedAtTimeOrigin,
    SelectedAtBothTimes
};

struct ReferenceShellDescriptorRequest {
    DescriptorMode descriptorMode = DescriptorMode::DipoleVector;
    int fromTypeId = 0;
    QString fromExpression;
    int toTypeId = 0;
    QString toExpression;
    QString referenceTypes;
    QString referenceExpression;
    QString anchorTypes;
    QString anchorExpression;
    FloatType cutoff = 5;
    bool onlySelectedParticles = false;
};

struct ReferenceShellMembershipRequest {
    QString referenceTypes;
    QString referenceExpression;
    QString anchorTypes;
    QString anchorExpression;
    FloatType cutoff = 5;
    bool onlySelectedParticles = false;
};

struct VectorAccumulator {
    QString targetLabel;
    QString elementDescriptionName;
    size_t itemCount = 0;
    int componentCount = 0;
    std::vector<IdentifierIntType> referenceIds;
    std::vector<std::vector<double>> frames;
    std::vector<std::vector<uint8_t>> selectionFrames;
};

struct MembershipAccumulator {
    QString targetLabel;
    QString elementDescriptionName;
    size_t itemCount = 0;
    std::vector<IdentifierIntType> referenceIds;
    std::vector<std::vector<uint8_t>> membershipFrames;
};

struct CorrelationCurves {
    std::vector<double> lagFrames;
    std::vector<double> overall;
};

[[nodiscard]] bool isNumericDataType(int dataType);
[[nodiscard]] QString vectorSubsetModeLabel(VectorSubsetMode mode);
[[nodiscard]] std::vector<std::vector<int>> buildFrameBatches(const std::vector<int>& frames, size_t batchSize);
[[nodiscard]] QString descriptorModeLabel(DescriptorMode mode);

void appendVectorSample(const PropertyContainerReference& propertyContainer,
                        const PropertyReference& property,
                        VectorSubsetMode selectionMode,
                        VectorAccumulator& accumulator,
                        const PipelineFlowState& sampleState,
                        const QString& analysisLabel);

void appendDescriptorVectorSample(const ReferenceShellDescriptorRequest& request,
                                  VectorAccumulator& accumulator,
                                  const PipelineFlowState& sampleState,
                                  const QString& analysisLabel);

void appendMembershipSample(const PropertyContainerReference& propertyContainer,
                            const PropertyReference& property,
                            MembershipAccumulator& accumulator,
                            const PipelineFlowState& sampleState,
                            const QString& analysisLabel);

void appendReferenceShellMembershipSample(const ReferenceShellMembershipRequest& request,
                                          MembershipAccumulator& accumulator,
                                          const PipelineFlowState& sampleState,
                                          const QString& analysisLabel);

[[nodiscard]] CorrelationCurves computeVectorReorientationCurves(const VectorAccumulator& samples,
                                                                 const std::vector<int>& sampledFrameNumbers,
                                                                 int legendreOrder,
                                                                 VectorSubsetMode selectionMode,
                                                                 int maxLag,
                                                                 const QString& analysisLabel);

[[nodiscard]] CorrelationCurves computeSurvivalProbabilityCurves(const MembershipAccumulator& samples,
                                                                 const std::vector<int>& sampledFrameNumbers,
                                                                 int intermittency,
                                                                 int maxLag,
                                                                 const QString& analysisLabel);

DataTable* createLineTable(DataCollection* collection,
                           const QStringView identifier,
                           const QString& title,
                           const std::vector<double>& xValues,
                           const std::vector<std::vector<double>>& columns,
                           QStringList componentNames,
                           const QString& axisLabelX,
                           const QString& axisLabelY,
                           const OOWeakRef<const PipelineNode>& createdByNode);

}  // namespace Ovito::OrientationTrajectoryAnalysis
