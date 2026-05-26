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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "TimeSeriesModifier.h"

namespace Ovito {

namespace {

struct TimeSeriesData {
    std::vector<double> xValues;
    std::vector<std::vector<double>> columns;
    QStringList componentNames;
    QString title;
    QString axisLabelX;
    QString axisLabelY;
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

double numericAttributeValue(const QVariant& value, const QString& attributeName)
{
    bool ok = false;
    const double numericValue = value.toDouble(&ok);
    if(!ok)
        throw Exception(TimeSeriesModifier::tr("Global attribute '%1' is not numeric and cannot be plotted as a time series.").arg(attributeName));
    return numericValue;
}

QString reductionName(TimeSeriesModifier::ReductionMode reductionMode)
{
    switch(reductionMode) {
    case TimeSeriesModifier::Mean: return TimeSeriesModifier::tr("Mean");
    case TimeSeriesModifier::Sum: return TimeSeriesModifier::tr("Sum");
    case TimeSeriesModifier::Minimum: return TimeSeriesModifier::tr("Minimum");
    case TimeSeriesModifier::Maximum: return TimeSeriesModifier::tr("Maximum");
    }
    return {};
}

QString xAxisLabel(TimeSeriesModifier::XAxisMode mode)
{
    switch(mode) {
    case TimeSeriesModifier::Frame: return TimeSeriesModifier::tr("Frame");
    case TimeSeriesModifier::AnimationTime: return TimeSeriesModifier::tr("Time");
    }
    return {};
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

std::vector<int> selectedComponentIndices(const Property* property, int selectedComponentIndex)
{
    OVITO_ASSERT(property);

    if(selectedComponentIndex >= 0) {
        if(selectedComponentIndex >= static_cast<int>(property->componentCount()))
            throw Exception(TimeSeriesModifier::tr("The selected component index %1 exceeds the number of components of property '%2'.").arg(selectedComponentIndex).arg(property->name()));
        return {selectedComponentIndex};
    }

    std::vector<int> indices(property->componentCount());
    std::iota(indices.begin(), indices.end(), 0);
    return indices;
}

QStringList selectedComponentNames(const Property* property, const std::vector<int>& componentIndices)
{
    OVITO_ASSERT(property);

    QStringList names;
    names.reserve(static_cast<qsizetype>(componentIndices.size()));
    for(int componentIndex : componentIndices)
        names.push_back(property->nameWithComponent(componentIndex));
    return names;
}

double reduceValues(const std::vector<FloatType>& values, TimeSeriesModifier::ReductionMode reductionMode)
{
    if(values.empty())
        throw Exception(TimeSeriesModifier::tr("The selected quantity does not contain any values in one of the sampled frames."));

    switch(reductionMode) {
    case TimeSeriesModifier::Mean:
        return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    case TimeSeriesModifier::Sum:
        return std::accumulate(values.begin(), values.end(), 0.0);
    case TimeSeriesModifier::Minimum:
        return *std::min_element(values.begin(), values.end());
    case TimeSeriesModifier::Maximum:
        return *std::max_element(values.begin(), values.end());
    }

    return {};
}

std::vector<double> reducePropertyComponents(const Property* property,
                                             const std::vector<int>& componentIndices,
                                             TimeSeriesModifier::ReductionMode reductionMode)
{
    OVITO_ASSERT(property);

    if(property->size() == 0)
        throw Exception(TimeSeriesModifier::tr("The selected property '%1' does not contain any values in one of the sampled frames.").arg(property->name()));

    std::vector<double> reducedValues;
    reducedValues.reserve(componentIndices.size());

    std::vector<FloatType> componentValues(property->size());
    for(int componentIndex : componentIndices) {
        property->copyComponentTo(componentValues.begin(), componentIndex);
        reducedValues.push_back(reduceValues(componentValues, reductionMode));
    }

    return reducedValues;
}

DataTable* createLineTable(DataCollection* collection,
                           const QString& identifier,
                           const QString& title,
                           const std::vector<double>& xValues,
                           const std::vector<std::vector<double>>& columns,
                           QStringList componentNames,
                           const QString& axisLabelX,
                           const QString& axisLabelY,
                           const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(columns.empty() || xValues.empty())
        throw Exception(TimeSeriesModifier::tr("Cannot create a time-series table without sampled values."));
    OVITO_ASSERT(collection);

    const size_t rowCount = xValues.size();
    const int componentCount = static_cast<int>(columns.size());
    OVITO_ASSERT(std::ranges::all_of(columns, [rowCount](const std::vector<double>& c) { return c.size() == rowCount; }));
    if(componentNames.size() != componentCount)
        componentNames.clear();

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            componentCount,
                                                            axisLabelY.isEmpty() ? QStringLiteral("Y") : axisLabelY,
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
                                                            axisLabelX.isEmpty() ? QStringLiteral("Frame") : axisLabelX);
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t i = 0; i < rowCount; ++i)
        xAcc[i] = static_cast<FloatType>(xValues[i]);

    DataTable* table = collection->createObject<DataTable>(identifier,
                                                           createdByNode,
                                                           DataTable::Line,
                                                           title,
                                                           std::move(y),
                                                           std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

double sampleXValue(TimeSeriesModifier::XAxisMode mode, const ModificationNode* modNode, int frame)
{
    switch(mode) {
    case TimeSeriesModifier::Frame:
        return static_cast<double>(frame);
    case TimeSeriesModifier::AnimationTime:
        return static_cast<double>(modNode->sourceFrameToAnimationTime(frame).ticks());
    }
    return static_cast<double>(frame);
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeSeriesModifier);
OVITO_CLASSINFO(TimeSeriesModifier, "DisplayName", "Time series");
OVITO_CLASSINFO(TimeSeriesModifier, "Description", "Sample a trajectory quantity over multiple frames and plot it as a time series.");
OVITO_CLASSINFO(TimeSeriesModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, targetType);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, attributeName);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, table);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, propertyContainer);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, property);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, reductionMode);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, xAxisMode);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, intervalStart);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(TimeSeriesModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, targetType, "Target type");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, attributeName, "Attribute");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, table, "Data table");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, propertyContainer, "Property container");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, property, "Property");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, reductionMode, "Reduction");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, xAxisMode, "Horizontal axis");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, useCustomFrameInterval, "Restrict sampled interval");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(TimeSeriesModifier, samplingFrequency, "Sampling frequency");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeSeriesModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeSeriesModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeSeriesModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeSeriesModificationNode);
DEFINE_REFERENCE_FIELD(TimeSeriesModificationNode, seriesTable);
DEFINE_PROPERTY_FIELD(TimeSeriesModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(TimeSeriesModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(TimeSeriesModifier, TimeSeriesModificationNode);

bool TimeSeriesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObjectRecursive(PropertyContainer::OOClass())
        || input.containsObject<SimulationCell>()
        || input.containsObject<AttributeDataObject>();
}

void TimeSeriesModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    setEnabled(true);
}

void TimeSeriesModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(!this_task::isInteractive())
        return;

    const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();

    if((targetType() == TimeSeriesModifier::Attribute && !attributeName().isEmpty())
        || (targetType() == TimeSeriesModifier::Table && table())
        || (targetType() == TimeSeriesModifier::Property && propertyContainer() && property())
        || targetType() == TimeSeriesModifier::Cell) {
        return;
    }

    for(const ConstDataObjectPath& path : input.getObjectsRecursive(PropertyContainer::OOClass())) {
        if(const PropertyContainer* container = path.lastAs<PropertyContainer>()) {
            if(DataTable::OOClass().isMember(container))
                continue;

            for(const DataOORef<const Ovito::Property>& candidateRef : container->properties()) {
                const Ovito::Property* candidate = candidateRef;
                if(!isNumericDataType(candidate->dataType()))
                    continue;
                if(candidate->typeId() == Ovito::Property::GenericIdentifierProperty)
                    continue;

                setTargetType(TimeSeriesModifier::Property);
                setPropertyContainer(path);
                setProperty(candidate);
                return;
            }
        }
    }

    const QVariantMap attributes = input.buildAttributesMap();
    for(auto iter = attributes.constBegin(); iter != attributes.constEnd(); ++iter) {
        bool ok = false;
        iter.value().toDouble(&ok);
        if(ok) {
            setTargetType(TimeSeriesModifier::Attribute);
            setAttributeName(iter.key());
            return;
        }
    }

    const std::vector<ConstDataObjectPath> tables = input.getObjectsRecursive(DataTable::OOClass());
    if(!tables.empty()) {
        setTargetType(TimeSeriesModifier::Table);
        setTable(tables.front());
        return;
    }

    if(input.containsObject<SimulationCell>())
        setTargetType(TimeSeriesModifier::Cell);
}

void TimeSeriesModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(!shouldIgnoreChanges()) {
        if(field == PROPERTY_FIELD(TimeSeriesModifier::targetType)
            || field == PROPERTY_FIELD(TimeSeriesModifier::attributeName)
            || field == PROPERTY_FIELD(TimeSeriesModifier::table)
            || field == PROPERTY_FIELD(TimeSeriesModifier::propertyContainer)
            || field == PROPERTY_FIELD(TimeSeriesModifier::property)
            || field == PROPERTY_FIELD(TimeSeriesModifier::reductionMode)
            || field == PROPERTY_FIELD(TimeSeriesModifier::xAxisMode)) {
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
        }
    }

    Modifier::propertyChanged(field);
}

QVariant TimeSeriesModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    switch(targetType()) {
    case TimeSeriesModifier::Attribute:
        return attributeName();
    case TimeSeriesModifier::Table:
        return table().dataTitleOrPath();
    case TimeSeriesModifier::Property:
        return property().nameWithComponent();
    case TimeSeriesModifier::Cell:
        return tr("Simulation cell");
    }
    return {};
}

std::vector<int> TimeSeriesModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for time-series sampling."));

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

    if(result.empty())
        throw Exception(tr("The selected sampling interval does not contain any sampled frames."));

    return result;
}

void TimeSeriesModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    Modifier::inputCachingHints(request);
}

void TimeSeriesModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

void TimeSeriesModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

Future<PipelineFlowState> TimeSeriesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(TimeSeriesModificationNode* modNode = dynamic_object_cast<TimeSeriesModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedSeries() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedSeries(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Time series is idle. Open the Start section and click 'Start series' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Time series is queued. Click 'Start series' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeSeriesData(request, std::move(state));
}

Future<PipelineFlowState> TimeSeriesModifier::computeSeriesData(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    // Traverse sampled frames one at a time to avoid reentrant upstream modifier evaluation.
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 1);
    const int cacheGenerationId = dynamic_object_cast<TimeSeriesModificationNode>(request.modificationNode())
        ? dynamic_object_cast<TimeSeriesModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;
    // Long-running trajectory traversal modifiers should register an outer task-progress object
    // so the status bar reports whole-trajectory progress instead of only nested upstream work.
    auto progress = std::make_shared<TaskProgress>(this_task::ui());
    progress->setText(tr("Computing time series"));
    progress->setMaximum(static_cast<qlonglong>(frames.size()));

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, TimeSeriesData&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [this, request = ModifierEvaluationRequest(request), progress, totalFrameCount = frames.size()]
            (const std::vector<int>& frameBatch, std::vector<SharedFuture<PipelineFlowState>> futures, TimeSeriesData& accumulator) mutable {
                OVITO_ASSERT(frameBatch.size() == futures.size());

                for(size_t batchIndex = 0; batchIndex < futures.size(); ++batchIndex) {
                    this_task::throwIfCanceled();
                    const PipelineFlowState& sampleState = futures[batchIndex].result();
                    const int frame = frameBatch[batchIndex];

                    accumulator.xValues.push_back(sampleXValue(xAxisMode(), request.modificationNode(), frame));

                    std::vector<double> sampledValues;
                    QStringList componentNames;
                    QString title;
                    QString axisLabelY;

                    switch(targetType()) {
                    case TimeSeriesModifier::Attribute: {
                        if(attributeName().isEmpty())
                            throw Exception(tr("No input attribute selected for the time-series analysis."));

                        const QVariant value = sampleState.getAttributeValue(attributeName());
                        if(!value.isValid())
                            throw Exception(tr("Global attribute '%1' is not available in one of the sampled trajectory frames.").arg(attributeName()));

                        sampledValues.push_back(numericAttributeValue(value, attributeName()));
                        componentNames = {attributeName()};
                        title = tr("Time series of %1").arg(attributeName());
                        axisLabelY = attributeName();
                        break;
                    }

                    case TimeSeriesModifier::Table: {
                        if(!table())
                            throw Exception(tr("No input data table selected for the time-series analysis."));

                        const DataTable* sampleTable = dynamic_object_cast<DataTable>(sampleState.getLeafObject(table()));
                        if(!sampleTable)
                            throw Exception(tr("The selected data table '%1' is not available in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));

                        const Ovito::Property* sampleY = sampleTable->y();
                        if(!sampleY)
                            throw Exception(tr("Data table '%1' has no Y-values in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));
                        if(!isNumericDataType(sampleY->dataType()))
                            throw Exception(tr("The Y-values of data table '%1' are not numeric and cannot be reduced into a time series.").arg(table().dataTitleOrPath()));

                        const std::vector<int> componentIndices = selectedComponentIndices(sampleY, -1);
                        sampledValues = reducePropertyComponents(sampleY, componentIndices, reductionMode());
                        componentNames = selectedComponentNames(sampleY, componentIndices);
                        title = tr("Time series of %1").arg(table().dataTitleOrPath());
                        axisLabelY = tr("%1 of %2").arg(reductionName(reductionMode()), sampleTable->title().isEmpty() ? table().dataTitleOrPath() : sampleTable->title());
                        break;
                    }

                    case TimeSeriesModifier::Property: {
                        if(!propertyContainer())
                            throw Exception(tr("No input property container selected for the time-series analysis."));
                        if(!property())
                            throw Exception(tr("No input property selected for the time-series analysis."));

                        const PropertyContainer* sampleContainer = sampleState.getLeafObject(propertyContainer());
                        if(!sampleContainer)
                            throw Exception(tr("The selected property container '%1' is not available in one of the sampled trajectory frames.").arg(propertyContainer().dataTitleOrPath()));

                        QString errorDescription;
                        auto [sampleProperty, componentIndex] = property().findInContainerWithComponent(sampleContainer, errorDescription, false);
                        if(!sampleProperty)
                            throw Exception(errorDescription.isEmpty()
                                ? tr("Property '%1' is not available in one of the sampled trajectory frames.").arg(property().nameWithComponent())
                                : errorDescription);
                        if(!isNumericDataType(sampleProperty->dataType()))
                            throw Exception(tr("Property '%1' is not numeric in one of the sampled trajectory frames.").arg(sampleProperty->name()));

                        const std::vector<int> componentIndices = selectedComponentIndices(sampleProperty, componentIndex);
                        sampledValues = reducePropertyComponents(sampleProperty, componentIndices, reductionMode());
                        componentNames = selectedComponentNames(sampleProperty, componentIndices);
                        title = tr("Time series of %1").arg(property().nameWithComponent());
                        axisLabelY = tr("%1 of %2").arg(reductionName(reductionMode()), property().nameWithComponent());
                        break;
                    }

                    case TimeSeriesModifier::Cell: {
                        const SimulationCell* sampleCell = sampleState.getObject<SimulationCell>();
                        if(!sampleCell)
                            throw Exception(tr("The input trajectory does not provide a simulation cell that could be sampled."));

                        sampledValues = extractCellValues(sampleCell);
                        componentNames = cellComponentNames();
                        title = tr("Time series of simulation cell");
                        axisLabelY = tr("Simulation cell");
                        break;
                    }
                    }

                    if(accumulator.columns.empty()) {
                        accumulator.columns.resize(sampledValues.size());
                        accumulator.componentNames = componentNames;
                        accumulator.title = title;
                        accumulator.axisLabelX = xAxisLabel(xAxisMode());
                        accumulator.axisLabelY = axisLabelY;
                    }
                    else if(accumulator.columns.size() != sampledValues.size()) {
                        throw Exception(tr("The sampled quantity changes its number of components over time and cannot be plotted as one consistent time series."));
                    }

                    for(size_t c = 0; c < sampledValues.size(); ++c)
                        accumulator.columns[c].push_back(sampledValues[c]);

                    progress->setText(tr("Computing time series (%1/%2 frames)")
                                          .arg(accumulator.xValues.size())
                                          .arg(totalFrameCount));
                    progress->setValue(static_cast<qlonglong>(accumulator.xValues.size()));
                }
            },
            TimeSeriesData{})
        .then(ObjectExecutor(this), [this, request, state = std::move(state), progress = std::move(progress), cacheGenerationId](TimeSeriesData accumulator) mutable {
            TimeSeriesModificationNode* modNode = dynamic_object_cast<TimeSeriesModificationNode>(request.modificationNode());
            if(!modNode)
                return std::move(state);

            const int completedRunRequestId = runRequestId();

            if(accumulator.xValues.empty() || accumulator.columns.empty())
                throw Exception(tr("The selected sampling interval does not contain any sampled values."));

            DataTable* outputTable = createLineTable(state.mutableData(),
                                                     QStringLiteral("time-series"),
                                                     accumulator.title,
                                                     accumulator.xValues,
                                                     accumulator.columns,
                                                     std::move(accumulator.componentNames),
                                                     accumulator.axisLabelX,
                                                     accumulator.axisLabelY,
                                                     request.modificationNodeWeak());

            if(modNode->cacheGenerationId() != cacheGenerationId || runRequestId() != completedRunRequestId)
                return std::move(state);

            modNode->setSeriesTable(outputTable);
            modNode->setCompletedRunRequestId(completedRunRequestId);
            progress->setText(tr("Computed time series from %1 sampled frame(s).").arg(accumulator.xValues.size()));
            progress->setValue(static_cast<qlonglong>(accumulator.xValues.size()));
            return applyCachedSeries(request, std::move(state));
        });
}

PipelineFlowState TimeSeriesModifier::applyCachedSeries(const ModifierEvaluationRequest& request, PipelineFlowState state) const
{
    TimeSeriesModificationNode* modNode = dynamic_object_cast<TimeSeriesModificationNode>(request.modificationNode());
    if(!modNode)
        return state;

    if(!modNode->seriesTable())
        throw Exception(tr("No cached time-series table is available."));

    state.addObjectWithUniqueId(modNode->seriesTable());
    state.setStatus(PipelineStatus(tr("Computed time series from %1 sampled frame(s).").arg(modNode->seriesTable()->elementCount())));
    return state;
}

void TimeSeriesModificationNode::invalidateCachedSeries()
{
    setSeriesTable(nullptr);
    setCacheGenerationId(cacheGenerationId() + 1);
}

bool TimeSeriesModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedSeries();
    }
    return ModificationNode::referenceEvent(source, event);
}

}   // End of namespace
