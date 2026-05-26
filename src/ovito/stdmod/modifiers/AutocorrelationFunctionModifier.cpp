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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "AutocorrelationFunctionModifier.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <unordered_map>

namespace Ovito {

namespace {

struct SignalSamples {
    QString targetLabel;
    QStringList componentNames;
    size_t itemCount = 0;
    int componentCount = 0;
    std::vector<std::vector<double>> frames;
};

struct SignalAccumulator {
    SignalSamples signal;
    std::vector<IdentifierIntType> referenceIds;
    std::vector<double> referenceTableXValues;
    std::array<bool, 3> referenceCellPbcFlags{false, false, false};
    size_t referenceTableElementCount = 0;
    double referenceTableIntervalStart = 0.0;
    double referenceTableIntervalEnd = 0.0;
    QString elementDescriptionName;
    bool referenceTableHasX = false;
    bool referenceCellIs2D = false;
};

struct CorrelationCurves {
    std::vector<double> lagFrames;
    std::vector<double> overall;
    std::vector<std::vector<double>> perComponent;
};

struct CorrelationComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

std::vector<std::vector<int>> buildFrameBatches(const std::vector<int>& frames, size_t batchSize)
{
    OVITO_ASSERT(batchSize > 0);

    std::vector<std::vector<int>> batches;
    batches.reserve((frames.size() + batchSize - 1) / batchSize);
    for(size_t begin = 0; begin < frames.size(); begin += batchSize) {
        const size_t end = std::min(begin + batchSize, frames.size());
        batches.emplace_back(frames.begin() + static_cast<ptrdiff_t>(begin), frames.begin() + static_cast<ptrdiff_t>(end));
    }
    return batches;
}

bool isNumericDataType(int dataType)
{
    return dataType == DataBuffer::Float32
        || dataType == DataBuffer::Float64
        || dataType == DataBuffer::Int8
        || dataType == DataBuffer::Int32
        || dataType == DataBuffer::Int64;
}

double attributeValueToDouble(const QVariant& value, const QString& attributeName)
{
    bool ok = false;
    const double numericValue = value.toDouble(&ok);
    if(!ok) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Global attribute '%1' is not numeric and cannot be used for autocorrelation analysis.")
            .arg(attributeName));
    }
    return numericValue;
}

const Property* identifierProperty(const PropertyContainer* container)
{
    if(!container)
        return nullptr;
    if(!container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        return nullptr;
    return container->getProperty(Property::GenericIdentifierProperty);
}

bool identifierBuffersMatch(const BufferReadAccess<IdentifierIntType>& lhs, const BufferReadAccess<IdentifierIntType>& rhs)
{
    if(!lhs || !rhs)
        return false;
    return lhs.size() == rhs.size() && std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin());
}

std::vector<size_t> buildIndexMapping(const BufferReadAccess<IdentifierIntType>& destinationIds,
                                      const BufferReadAccess<IdentifierIntType>& sourceIds,
                                      const QString& elementName,
                                      const QString& destinationLabel,
                                      const QString& sourceLabel)
{
    OVITO_ASSERT(destinationIds);
    OVITO_ASSERT(sourceIds);

    std::unordered_map<IdentifierIntType, size_t> sourceMap;
    sourceMap.reserve(sourceIds.size());

    size_t index = 0;
    for(const IdentifierIntType id : sourceIds) {
        if(!sourceMap.insert({id, index}).second) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Detected duplicate %1 ID %2 in the %3 configuration. Autocorrelation analysis cannot match elements in this case.")
                .arg(elementName)
                .arg(id)
                .arg(sourceLabel));
        }
        index++;
    }

    std::vector<size_t> mapping(destinationIds.size());
    size_t destinationIndex = 0;
    for(const IdentifierIntType id : destinationIds) {
        auto iter = sourceMap.find(id);
        if(iter == sourceMap.end()) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "The %1 ID %2 exists in the %3 configuration but not in the %4 configuration. "
                "Autocorrelation analysis currently requires a stable set of element IDs.")
                .arg(elementName)
                .arg(id)
                .arg(destinationLabel)
                .arg(sourceLabel));
        }
        mapping[destinationIndex++] = iter->second;
    }

    return mapping;
}

std::vector<size_t> buildIndexMapping(const std::vector<IdentifierIntType>& destinationIds,
                                      const BufferReadAccess<IdentifierIntType>& sourceIds,
                                      const QString& elementName,
                                      const QString& destinationLabel,
                                      const QString& sourceLabel)
{
    OVITO_ASSERT(sourceIds);

    std::unordered_map<IdentifierIntType, size_t> sourceMap;
    sourceMap.reserve(sourceIds.size());

    size_t index = 0;
    for(const IdentifierIntType id : sourceIds) {
        if(!sourceMap.insert({id, index}).second) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Detected duplicate %1 ID %2 in the %3 configuration. Autocorrelation analysis cannot match elements in this case.")
                .arg(elementName)
                .arg(id)
                .arg(sourceLabel));
        }
        index++;
    }

    std::vector<size_t> mapping(destinationIds.size());
    size_t destinationIndex = 0;
    for(const IdentifierIntType id : destinationIds) {
        auto iter = sourceMap.find(id);
        if(iter == sourceMap.end()) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "The %1 ID %2 exists in the %3 configuration but not in the %4 configuration. "
                "Autocorrelation analysis currently requires a stable set of element IDs.")
                .arg(elementName)
                .arg(id)
                .arg(destinationLabel)
                .arg(sourceLabel));
        }
        mapping[destinationIndex++] = iter->second;
    }

    return mapping;
}

std::vector<double> extractPropertyValues(const Property* property, const std::vector<size_t>* mapping = nullptr)
{
    OVITO_ASSERT(property);
    const size_t outputElementCount = mapping ? mapping->size() : property->size();
    std::vector<double> values(outputElementCount * property->componentCount(), 0.0);
    std::vector<FloatType> componentValues(property->size());

    for(size_t c = 0; c < property->componentCount(); ++c) {
        property->copyComponentTo(componentValues.begin(), c);
        if(mapping) {
            for(size_t dst = 0; dst < mapping->size(); ++dst)
                values[dst * property->componentCount() + c] = static_cast<double>(componentValues[(*mapping)[dst]]);
        }
        else {
            for(size_t i = 0; i < property->size(); ++i)
                values[i * property->componentCount() + c] = static_cast<double>(componentValues[i]);
        }
    }

    return values;
}

bool identifierBuffersMatch(const std::vector<IdentifierIntType>& lhs, const BufferReadAccess<IdentifierIntType>& rhs)
{
    if(!rhs)
        return false;
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.cbegin());
}

QStringList propertyComponentNames(const Property* property)
{
    if(!property)
        return {};

    QStringList names = property->componentNames();
    if(names.size() == static_cast<qsizetype>(property->componentCount()))
        return names;

    names.clear();
    if(property->componentCount() == 1) {
        names.push_back(property->name());
        return names;
    }

    for(size_t c = 0; c < property->componentCount(); ++c)
        names.push_back(property->nameWithComponent(static_cast<int>(c)));
    return names;
}

QStringList cellComponentNames()
{
    return {
        QStringLiteral("a1.x"), QStringLiteral("a1.y"), QStringLiteral("a1.z"),
        QStringLiteral("a2.x"), QStringLiteral("a2.y"), QStringLiteral("a2.z"),
        QStringLiteral("a3.x"), QStringLiteral("a3.y"), QStringLiteral("a3.z"),
        QStringLiteral("origin.x"), QStringLiteral("origin.y"), QStringLiteral("origin.z")
    };
}

std::vector<double> extractCellValues(const SimulationCell* cell)
{
    OVITO_ASSERT(cell);
    const AffineTransformation& matrix = cell->cellMatrix();
    std::vector<double> values;
    values.reserve(12);
    for(size_t col = 0; col < 4; ++col) {
        values.push_back(matrix(0, col));
        values.push_back(matrix(1, col));
        values.push_back(matrix(2, col));
    }
    return values;
}

DataTable* createLineTable(DataCollection* collection,
                           const QStringView identifier,
                           const QString& title,
                           const std::vector<double>& xValues,
                           const std::vector<std::vector<double>>& columns,
                           QStringList componentNames,
                           const QString& axisLabelX,
                           const QString& axisLabelY,
                           const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(columns.empty() || xValues.empty())
        return nullptr;

    const size_t rowCount = xValues.size();
    const int componentCount = static_cast<int>(columns.size());
    OVITO_ASSERT(std::ranges::all_of(columns, [rowCount](const std::vector<double>& c) { return c.size() == rowCount; }));
    if(componentNames.size() != componentCount)
        componentNames.clear();

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            componentCount,
                                                            axisLabelY,
                                                            0,
                                                            std::move(componentNames));
    BufferWriteAccess<FloatType*, access_mode::discard_write> yAcc(y);
    for(size_t i = 0; i < rowCount; ++i) {
        for(int c = 0; c < componentCount; ++c)
            yAcc.set(i, c, static_cast<FloatType>(columns[c][i]));
    }

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Lag"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t i = 0; i < rowCount; ++i)
        xAcc[i] = static_cast<FloatType>(xValues[i]);

    DataTable* table = collection->createObject<DataTable>(identifier.toString(),
                                                           createdByNode,
                                                           DataTable::Line,
                                                           title,
                                                           std::move(y),
                                                           std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

QString overallCurveLabel(const SignalSamples& signal)
{
    return signal.componentCount <= 1 ? signal.targetLabel : AutocorrelationFunctionModifier::tr("Overall");
}

CorrelationCurves computeCorrelationCurves(const SignalSamples& signal,
                                           const std::vector<int>& sampledFrameNumbers,
                                           bool subtractMean,
                                           bool normalizeByZeroLag,
                                           int maxLag,
                                           bool& hadZeroLagNormalizationIssue)
{
    OVITO_ASSERT(signal.frames.size() == sampledFrameNumbers.size());
    CorrelationCurves curves;

    const size_t frameCount = signal.frames.size();
    if(frameCount < 2)
        throw Exception(AutocorrelationFunctionModifier::tr("Autocorrelation analysis requires at least two sampled trajectory frames."));

    const size_t componentCount = static_cast<size_t>(signal.componentCount);
    const size_t maxLagEffective = std::min<size_t>((maxLag > 0 ? static_cast<size_t>(maxLag) : frameCount - 1), frameCount - 1);

    curves.lagFrames.assign(maxLagEffective + 1, 0.0);
    curves.overall.assign(maxLagEffective + 1, 0.0);
    curves.perComponent.assign(componentCount, std::vector<double>(maxLagEffective + 1, 0.0));

    std::vector<double> means(componentCount, 0.0);
    if(subtractMean) {
        for(const std::vector<double>& frameValues : signal.frames) {
            this_task::throwIfCanceled();
            for(size_t item = 0; item < signal.itemCount; ++item) {
                for(size_t c = 0; c < componentCount; ++c)
                    means[c] += frameValues[item * componentCount + c];
            }
        }
        const double normalization = static_cast<double>(frameCount * signal.itemCount);
        for(double& value : means)
            value /= normalization;
    }

    parallelForChunks(maxLagEffective + 1, 8, [&](size_t, size_t fromLag, size_t toLag) {
        for(size_t lag = fromLag; lag < toLag; ++lag) {
            this_task::throwIfCanceled();
            const size_t originCount = frameCount - lag;
            double overallAccumulator = 0.0;
            double lagFrameAccumulator = 0.0;

            for(size_t c = 0; c < componentCount; ++c) {
                double componentAccumulator = 0.0;
                for(size_t origin = 0; origin < originCount; ++origin) {
                    const std::vector<double>& frame0 = signal.frames[origin];
                    const std::vector<double>& frame1 = signal.frames[origin + lag];
                    lagFrameAccumulator += (c == 0) ? static_cast<double>(sampledFrameNumbers[origin + lag] - sampledFrameNumbers[origin]) : 0.0;

                    double componentSum = 0.0;
                    for(size_t item = 0; item < signal.itemCount; ++item) {
                        double a = frame0[item * componentCount + c];
                        double b = frame1[item * componentCount + c];
                        if(subtractMean) {
                            a -= means[c];
                            b -= means[c];
                        }
                        componentSum += a * b;
                    }
                    componentAccumulator += componentSum / static_cast<double>(signal.itemCount);
                }

                curves.perComponent[c][lag] = componentAccumulator / static_cast<double>(originCount);
                overallAccumulator += curves.perComponent[c][lag];
            }

            curves.overall[lag] = overallAccumulator / static_cast<double>(componentCount);
            curves.lagFrames[lag] = lagFrameAccumulator / static_cast<double>(originCount);
        }
    });

    hadZeroLagNormalizationIssue = false;
    if(normalizeByZeroLag) {
        auto normalizeCurve = [&hadZeroLagNormalizationIssue](std::vector<double>& curve) {
            if(curve.empty())
                return;
            const double zeroLag = curve.front();
            if(!std::isfinite(zeroLag) || std::abs(zeroLag) <= std::numeric_limits<double>::epsilon()) {
                std::ranges::fill(curve, std::numeric_limits<double>::quiet_NaN());
                hadZeroLagNormalizationIssue = true;
                return;
            }
            for(double& value : curve)
                value /= zeroLag;
        };

        normalizeCurve(curves.overall);
        for(std::vector<double>& curve : curves.perComponent)
            normalizeCurve(curve);
    }

    return curves;
}

void appendAttributeSample(const AutocorrelationFunctionModifier* modifier,
                           SignalAccumulator& accumulator,
                           const PipelineFlowState& sampleState)
{
    if(modifier->attributeName().isEmpty())
        throw Exception(AutocorrelationFunctionModifier::tr("No input attribute selected for autocorrelation analysis."));

    if(accumulator.signal.frames.empty()) {
        accumulator.signal.targetLabel = modifier->attributeName();
        accumulator.signal.componentNames = {modifier->attributeName()};
        accumulator.signal.itemCount = 1;
        accumulator.signal.componentCount = 1;
    }

    const QVariant value = sampleState.getAttributeValue(modifier->attributeName());
    if(!value.isValid()) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Global attribute '%1' is not available in one of the sampled trajectory frames.")
            .arg(modifier->attributeName()));
    }

    accumulator.signal.frames.push_back({attributeValueToDouble(value, modifier->attributeName())});
}

void appendTableSample(const AutocorrelationFunctionModifier* modifier,
                       SignalAccumulator& accumulator,
                       const PipelineFlowState& sampleState)
{
    if(!modifier->table())
        throw Exception(AutocorrelationFunctionModifier::tr("No input data table selected for autocorrelation analysis."));

    const DataTable* sampleTable = dynamic_object_cast<DataTable>(sampleState.getLeafObject(modifier->table()));
    if(!sampleTable) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "The selected data table '%1' is not available in one of the sampled trajectory frames.")
            .arg(modifier->table().dataTitleOrPath()));
    }

    const Property* sampleY = sampleTable->y();
    if(!sampleY) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Data table '%1' does not contain any Y-values that could be used for autocorrelation analysis.")
            .arg(modifier->table().dataTitleOrPath()));
    }
    if(!isNumericDataType(sampleY->dataType())) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "The Y-values of data table '%1' are not numeric and cannot be used for autocorrelation analysis.")
            .arg(modifier->table().dataTitleOrPath()));
    }

    if(accumulator.signal.frames.empty()) {
        accumulator.signal.targetLabel = modifier->table().dataTitleOrPath();
        accumulator.signal.componentNames = propertyComponentNames(sampleY);
        accumulator.signal.itemCount = sampleY->size();
        accumulator.signal.componentCount = static_cast<int>(sampleY->componentCount());
        if(const Property* sampleX = sampleTable->x()) {
            accumulator.referenceTableHasX = true;
            accumulator.referenceTableXValues = extractPropertyValues(sampleX);
        }
        else {
            accumulator.referenceTableElementCount = sampleTable->elementCount();
            accumulator.referenceTableIntervalStart = sampleTable->intervalStart();
            accumulator.referenceTableIntervalEnd = sampleTable->intervalEnd();
        }
    }
    else {
        if(sampleY->size() != accumulator.signal.itemCount || sampleY->componentCount() != static_cast<size_t>(accumulator.signal.componentCount)) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Data table '%1' changes its size or number of data columns over time and cannot be used for autocorrelation analysis.")
                .arg(modifier->table().dataTitleOrPath()));
        }

        if(accumulator.referenceTableHasX) {
            const Property* sampleX = sampleTable->x();
            if(!sampleX || extractPropertyValues(sampleX) != accumulator.referenceTableXValues) {
                throw Exception(AutocorrelationFunctionModifier::tr(
                    "Data table '%1' changes its x-coordinates over time and cannot be used for autocorrelation analysis.")
                    .arg(modifier->table().dataTitleOrPath()));
            }
        }
        else if(sampleTable->x()
                || sampleTable->elementCount() != accumulator.referenceTableElementCount
                || sampleTable->intervalStart() != accumulator.referenceTableIntervalStart
                || sampleTable->intervalEnd() != accumulator.referenceTableIntervalEnd) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Data table '%1' changes its x-interval over time and cannot be used for autocorrelation analysis.")
                .arg(modifier->table().dataTitleOrPath()));
        }
    }

    accumulator.signal.frames.push_back(extractPropertyValues(sampleY));
}

void appendPropertySample(const AutocorrelationFunctionModifier* modifier,
                          SignalAccumulator& accumulator,
                          const PipelineFlowState& sampleState)
{
    if(!modifier->propertyContainer())
        throw Exception(AutocorrelationFunctionModifier::tr("No input property container selected for autocorrelation analysis."));
    if(!modifier->property())
        throw Exception(AutocorrelationFunctionModifier::tr("No input property selected for autocorrelation analysis."));
    if(!modifier->property().componentName().isEmpty()) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Autocorrelation analysis currently expects a full property, not an individual vector component."));
    }

    const PropertyContainer* sampleContainer = sampleState.getLeafObject(modifier->propertyContainer());
    if(!sampleContainer) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "The selected property container '%1' is not available in one of the sampled trajectory frames.")
            .arg(modifier->propertyContainer().dataTitleOrPath()));
    }
    sampleContainer->verifyIntegrity();

    const Property* sampleProperty = modifier->property().findInContainer(sampleContainer);
    if(!sampleProperty) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Property '%1' is not available in one of the sampled trajectory frames.")
            .arg(modifier->property().nameWithComponent()));
    }
    if(!isNumericDataType(sampleProperty->dataType())) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Property '%1' is not numeric and cannot be used for autocorrelation analysis.")
            .arg(sampleProperty->name()));
    }
    if(sampleProperty->isTypedProperty() || sampleProperty->typeId() == Property::GenericIdentifierProperty) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "Property '%1' is not supported by the current autocorrelation implementation.")
            .arg(sampleProperty->name()));
    }

    if(accumulator.signal.frames.empty()) {
        accumulator.signal.targetLabel = modifier->property().nameWithComponent();
        accumulator.signal.componentNames = propertyComponentNames(sampleProperty);
        accumulator.signal.itemCount = sampleProperty->size();
        accumulator.signal.componentCount = static_cast<int>(sampleProperty->componentCount());
        accumulator.elementDescriptionName = sampleContainer->getOOMetaClass().elementDescriptionName();
        if(BufferReadAccess<IdentifierIntType> referenceIds(identifierProperty(sampleContainer)); referenceIds) {
            accumulator.referenceIds.assign(referenceIds.cbegin(), referenceIds.cend());
        }
    }
    else {
        if(sampleProperty->componentCount() != static_cast<size_t>(accumulator.signal.componentCount)) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Property '%1' changes its number of vector components over time and cannot be used for autocorrelation analysis.")
                .arg(sampleProperty->name()));
        }
        if(!isNumericDataType(sampleProperty->dataType())) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Property '%1' is not numeric in one of the sampled trajectory frames.")
                .arg(sampleProperty->name()));
        }
    }

    BufferReadAccess<IdentifierIntType> sampleIds(identifierProperty(sampleContainer));
    if(!accumulator.referenceIds.empty()) {
        if(!sampleIds) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "The identifier property of %1 is missing in one of the sampled trajectory frames. "
                "The current autocorrelation implementation requires stable element IDs when the element order changes.")
                .arg(accumulator.elementDescriptionName));
        }

        if(identifierBuffersMatch(accumulator.referenceIds, sampleIds)) {
            accumulator.signal.frames.push_back(extractPropertyValues(sampleProperty));
        }
        else {
            const std::vector<size_t> mapping = buildIndexMapping(
                accumulator.referenceIds,
                sampleIds,
                accumulator.elementDescriptionName,
                AutocorrelationFunctionModifier::tr("reference"),
                AutocorrelationFunctionModifier::tr("sampled"));
            accumulator.signal.frames.push_back(extractPropertyValues(sampleProperty, &mapping));
        }
    }
    else {
        if(sampleProperty->size() != accumulator.signal.itemCount) {
            throw Exception(AutocorrelationFunctionModifier::tr(
                "Property '%1' changes the number of %2 over time. "
                "The current autocorrelation implementation requires a stable element count or a stable identifier property.")
                .arg(sampleProperty->name())
                .arg(sampleContainer->getOOMetaClass().elementDescriptionName()));
        }
        accumulator.signal.frames.push_back(extractPropertyValues(sampleProperty));
    }
}

void appendCellSample(SignalAccumulator& accumulator, const PipelineFlowState& sampleState)
{
    const SimulationCell* sampleCell = sampleState.getObject<SimulationCell>();
    if(!sampleCell)
        throw Exception(AutocorrelationFunctionModifier::tr("The input trajectory does not provide a simulation cell that could be used for autocorrelation analysis."));

    if(accumulator.signal.frames.empty()) {
        accumulator.signal.targetLabel = AutocorrelationFunctionModifier::tr("Simulation cell");
        accumulator.signal.componentNames = cellComponentNames();
        accumulator.signal.itemCount = 1;
        accumulator.signal.componentCount = static_cast<int>(accumulator.signal.componentNames.size());
        accumulator.referenceCellPbcFlags = sampleCell->pbcFlags();
        accumulator.referenceCellIs2D = sampleCell->is2D();
    }
    else if(sampleCell->pbcFlags() != accumulator.referenceCellPbcFlags || sampleCell->is2D() != accumulator.referenceCellIs2D) {
        throw Exception(AutocorrelationFunctionModifier::tr(
            "The periodic boundary settings of the simulation cell change over time and cannot be used for autocorrelation analysis."));
    }

    accumulator.signal.frames.push_back(extractCellValues(sampleCell));
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(AutocorrelationFunctionModifier);
OVITO_CLASSINFO(AutocorrelationFunctionModifier, "DisplayName", "Autocorrelation function");
OVITO_CLASSINFO(AutocorrelationFunctionModifier, "Description", "Compute a time autocorrelation function for a selected trajectory quantity.");
OVITO_CLASSINFO(AutocorrelationFunctionModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, targetType);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, attributeName);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, table);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, propertyContainer);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, property);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, subtractMean);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, normalizeByZeroLag);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, intervalStart);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, maxLag);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, targetType, "Target type");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, attributeName, "Attribute");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, table, "Data table");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, propertyContainer, "Property container");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, property, "Property");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, subtractMean, "Subtract mean value");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, normalizeByZeroLag, "Normalize by zero-lag value");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_LABEL(AutocorrelationFunctionModifier, maxLag, "Maximum lag (sampled-frame steps)");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AutocorrelationFunctionModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AutocorrelationFunctionModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AutocorrelationFunctionModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AutocorrelationFunctionModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(AutocorrelationFunctionModificationNode);
DEFINE_REFERENCE_FIELD(AutocorrelationFunctionModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(AutocorrelationFunctionModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(AutocorrelationFunctionModifier, AutocorrelationFunctionModificationNode);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool AutocorrelationFunctionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObjectRecursive(PropertyContainer::OOClass())
        || input.containsObject<SimulationCell>()
        || input.containsObject<AttributeDataObject>();
}

/******************************************************************************
* Constructor.
******************************************************************************/
void AutocorrelationFunctionModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

/******************************************************************************
* This method is called by the system when the modifier is inserted into a pipeline.
******************************************************************************/
void AutocorrelationFunctionModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(!this_task::isInteractive())
        return;

    const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();

    if((targetType() == AutocorrelationFunctionModifier::Attribute && !attributeName().isEmpty())
        || (targetType() == AutocorrelationFunctionModifier::Table && table())
        || (targetType() == AutocorrelationFunctionModifier::Property && propertyContainer() && property())
        || targetType() == AutocorrelationFunctionModifier::Cell) {
        return;
    }

    for(const ConstDataObjectPath& path : input.getObjectsRecursive(PropertyContainer::OOClass())) {
        if(const PropertyContainer* container = path.lastAs<PropertyContainer>()) {
            if(DataTable::OOClass().isMember(container))
                continue;

            for(const auto& candidateRef : container->properties()) {
                const Ovito::Property* candidate = candidateRef.get();
                if(!isNumericDataType(candidate->dataType()))
                    continue;
                if(candidate->isTypedProperty() || candidate->typeId() == Ovito::Property::GenericIdentifierProperty)
                    continue;

                setTargetType(AutocorrelationFunctionModifier::Property);
                setPropertyContainer(path);
                setProperty(candidate);
                return;
            }
        }
    }

    if(const QVariantMap attributes = input.buildAttributesMap(); !attributes.isEmpty()) {
        setTargetType(AutocorrelationFunctionModifier::Attribute);
        setAttributeName(attributes.firstKey());
        return;
    }

    const std::vector<ConstDataObjectPath> tables = input.getObjectsRecursive(DataTable::OOClass());
    if(!tables.empty()) {
        setTargetType(AutocorrelationFunctionModifier::Table);
        setTable(tables.front());
        return;
    }

    if(input.containsObject<SimulationCell>())
        setTargetType(AutocorrelationFunctionModifier::Cell);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void AutocorrelationFunctionModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(!shouldIgnoreChanges()) {
        if(field == PROPERTY_FIELD(AutocorrelationFunctionModifier::targetType)
            || field == PROPERTY_FIELD(AutocorrelationFunctionModifier::attributeName)
            || field == PROPERTY_FIELD(AutocorrelationFunctionModifier::table)
            || field == PROPERTY_FIELD(AutocorrelationFunctionModifier::propertyContainer)
            || field == PROPERTY_FIELD(AutocorrelationFunctionModifier::property)) {
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
        }
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
* Returns a short piece of information to be displayed next to the modifier title.
******************************************************************************/
QVariant AutocorrelationFunctionModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    switch(targetType()) {
    case Attribute:
        return attributeName();
    case Table:
        return table().dataTitleOrPath();
    case Property:
        return property().nameWithComponent();
    case Cell:
        return tr("Simulation cell");
    }
    return {};
}

/******************************************************************************
* Determines the list of sampled source frames.
******************************************************************************/
std::vector<int> AutocorrelationFunctionModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for autocorrelation analysis."));

    const int stride = std::max(1, samplingFrequency());

    int firstFrame = 0;
    int lastFrame = numFrames - 1;
    if(useCustomFrameInterval()) {
        firstFrame = std::clamp(intervalStart(), 0, numFrames - 1);
        lastFrame = std::clamp(intervalEnd(), 0, numFrames - 1);
        if(firstFrame > lastFrame)
            std::swap(firstFrame, lastFrame);
    }

    std::vector<int> result;
    result.reserve(((lastFrame - firstFrame) / stride) + 1);
    for(int frame = firstFrame; frame <= lastFrame; frame += stride)
        result.push_back(frame);

    if(result.size() < 2)
        throw Exception(tr("Autocorrelation analysis requires at least two sampled trajectory frames."));

    return result;
}

/******************************************************************************
* Asks the modifier for the set of animation time intervals that should be cached.
******************************************************************************/
void AutocorrelationFunctionModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    if(request.modificationNode()->numberOfSourceFrames() > 0) {
        const std::vector<int> frames = sampledFrames(request.modificationNode());
        if(!frames.empty()) {
            request.mutableCachingIntervals().add(TimeInterval(
                request.modificationNode()->sourceFrameToAnimationTime(frames.front()),
                request.modificationNode()->sourceFrameToAnimationTime(frames.back())));
        }
    }

    Modifier::inputCachingHints(request);
}

/******************************************************************************
* Is called by the pipeline system before a new modifier evaluation begins.
******************************************************************************/
void AutocorrelationFunctionModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                         PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                         TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Is called by the ModificationNode to let the modifier adjust the validity interval.
******************************************************************************/
void AutocorrelationFunctionModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> AutocorrelationFunctionModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                           PipelineFlowState&& state)
{
    if(AutocorrelationFunctionModificationNode* modNode = dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Autocorrelation analysis is idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Autocorrelation analysis is queued. Click 'Run autocorrelation analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeCorrelationData(request, std::move(state));
}

/******************************************************************************
* Computes the cached autocorrelation table by traversing the sampled trajectory.
******************************************************************************/
Future<PipelineFlowState> AutocorrelationFunctionModifier::computeCorrelationData(const ModifierEvaluationRequest& request,
                                                                                  PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode())
        ? dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    SignalAccumulator accumulator;
    accumulator.signal.frames.reserve(frames.size());

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this), // Require deferred execution to avoid deep inline continuation chains for long trajectories.
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, SignalAccumulator&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [this](const std::vector<int>&, std::vector<SharedFuture<PipelineFlowState>> batchFutures, SignalAccumulator& accumulator) {
                for(SharedFuture<PipelineFlowState>& future : batchFutures) {
                    this_task::throwIfCanceled();
                    const PipelineFlowState& sampleState = future.result();
                    switch(targetType()) {
                    case Attribute:
                        appendAttributeSample(this, accumulator, sampleState);
                        break;
                    case Table:
                        appendTableSample(this, accumulator, sampleState);
                        break;
                    case Property:
                        appendPropertySample(this, accumulator, sampleState);
                        break;
                    case Cell:
                        appendCellSample(accumulator, sampleState);
                        break;
                    }
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this), [this, request, state = std::move(state), frames, cacheGenerationId](SignalAccumulator accumulator) mutable -> Future<PipelineFlowState> {
            OORef<AutocorrelationFunctionModifier> self(this);
            const int completedRunRequestId = runRequestId();

            return asyncLaunch([self = std::move(self),
                                request = ModifierEvaluationRequest(request),
                                state = std::move(state),
                                frames,
                                accumulator = std::move(accumulator),
                                completedRunRequestId,
                                cacheGenerationId]() mutable {
                CorrelationComputationResult computationResult{std::move(state)};

                if(!dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode()))
                    return computationResult;

                this_task::throwIfCanceled();
                bool hadZeroLagNormalizationIssue = false;
                const CorrelationCurves curves = computeCorrelationCurves(
                    accumulator.signal,
                    frames,
                    self->subtractMean(),
                    self->normalizeByZeroLag(),
                    self->maxLag(),
                    hadZeroLagNormalizationIssue);

                computationResult.results = DataOORef<DataCollection>::create();
                const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();

                QStringList columnNames;
                std::vector<std::vector<double>> columns;
                columnNames.push_back(overallCurveLabel(accumulator.signal));
                columns.push_back(curves.overall);
                if(accumulator.signal.componentCount > 1) {
                    for(int c = 0; c < accumulator.signal.componentCount; ++c) {
                        this_task::throwIfCanceled();
                        columnNames.push_back(accumulator.signal.componentNames.value(c, AutocorrelationFunctionModifier::tr("Component %1").arg(c + 1)));
                        columns.push_back(curves.perComponent[c]);
                    }
                }

                createLineTable(computationResult.results,
                                AutocorrelationFunctionModifier::correlationTableId(),
                                AutocorrelationFunctionModifier::tr("Autocorrelation function"),
                                curves.lagFrames,
                                columns,
                                columnNames,
                                AutocorrelationFunctionModifier::tr("Lag (source frames)"),
                                self->normalizeByZeroLag()
                                    ? AutocorrelationFunctionModifier::tr("Normalized autocorrelation")
                                    : (self->subtractMean() ? AutocorrelationFunctionModifier::tr("Autocovariance")
                                                            : AutocorrelationFunctionModifier::tr("Autocorrelation")),
                                createdByNode);

                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.target"), accumulator.signal.targetLabel, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.sampled_item_count"), static_cast<double>(accumulator.signal.itemCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.component_count"), static_cast<double>(accumulator.signal.componentCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.maximum_lag"), static_cast<double>(curves.lagFrames.empty() ? 0.0 : curves.lagFrames.back()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.subtract_mean"), self->subtractMean() ? 1.0 : 0.0, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("Autocorrelation.normalized"), self->normalizeByZeroLag() ? 1.0 : 0.0, createdByNode);
                if(!curves.overall.empty()) {
                    computationResult.results->setAttribute(QStringLiteral("Autocorrelation.zero_lag"), curves.overall.front(), createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("Autocorrelation.final_value"), curves.overall.back(), createdByNode);
                }

                if(hadZeroLagNormalizationIssue) {
                    computationResult.warningText = AutocorrelationFunctionModifier::tr(
                        "At least one autocorrelation curve could not be normalized because its zero-lag value is zero or undefined.");
                }

                computationResult.completedRunRequestId = completedRunRequestId;
                computationResult.cacheGenerationId = cacheGenerationId;
                return computationResult;
            }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](CorrelationComputationResult computationResult) mutable {
                AutocorrelationFunctionModificationNode* modNode =
                    dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode());
                if(!modNode || !computationResult.results)
                    return std::move(computationResult.state);

                if(modNode->cacheGenerationId() != computationResult.cacheGenerationId || runRequestId() != computationResult.completedRunRequestId)
                    return std::move(computationResult.state);

                modNode->setCachedResults(computationResult.results);
                modNode->setCachedWarningText(computationResult.warningText);
                modNode->setCompletedRunRequestId(computationResult.completedRunRequestId);
                return applyCachedResults(request, std::move(computationResult.state));
            });
        });
}

/******************************************************************************
* Applies the cached autocorrelation data to the current pipeline state.
******************************************************************************/
PipelineFlowState AutocorrelationFunctionModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                     PipelineFlowState state) const
{
    AutocorrelationFunctionModificationNode* modNode =
        dynamic_object_cast<AutocorrelationFunctionModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

/******************************************************************************
* Clears all cached autocorrelation data.
******************************************************************************/
void AutocorrelationFunctionModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

/******************************************************************************
* Is called when a referenced target generated an event.
******************************************************************************/
bool AutocorrelationFunctionModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}   // End of namespace
