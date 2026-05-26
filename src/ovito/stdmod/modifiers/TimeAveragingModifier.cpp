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
#include "TimeAveragingModifier.h"

#include <algorithm>
#include <unordered_map>

namespace Ovito {

namespace {

struct RunningAverageData {
    size_t sampleCount = 0;
    FloatType attributeSum = 0;
    DataOORef<DataTable> referenceTable;
    DataOORef<Property> referenceProperty;
    DataOORef<Property> referenceIdentifiers;
    DataOORef<SimulationCell> referenceCell;
    QString elementDescriptionName;
    std::vector<FloatType> sums;
    AffineTransformation cellSum;
    bool hasCellSum = false;
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

QString averagedAttributeName(const QString& name, bool overwrite)
{
    return overwrite ? name : QStringLiteral("%1 (Average)").arg(name);
}

QString averagedPropertyName(const QStringView name, bool overwrite)
{
    return overwrite ? name.toString() : QStringLiteral("%1 Average").arg(name);
}

QString averagedTableId(const QString& id, bool overwrite)
{
    const QString baseId = id.isEmpty() ? QStringLiteral("table") : id;
    return overwrite ? baseId : QStringLiteral("%1[average]").arg(baseId);
}

FloatType attributeValueToFloat(const QVariant& value, const QString& attributeName)
{
    bool ok = false;
    const double numericValue = value.toDouble(&ok);
    if(!ok)
        throw Exception(TimeAveragingModifier::tr("Global attribute '%1' is not numeric and cannot be time-averaged.").arg(attributeName));
    return static_cast<FloatType>(numericValue);
}

int averagedPropertyDataType(const Property* property, bool overwrite)
{
    OVITO_ASSERT(property);

    if(!overwrite)
        return DataBuffer::FloatDefault;

    if(property->isStandardProperty()) {
        if(property->dataType() != DataBuffer::Float32 && property->dataType() != DataBuffer::Float64) {
            throw Exception(TimeAveragingModifier::tr(
                "The standard property '%1' cannot be overwritten with time-averaged values because it does not use a floating-point data type.")
                .arg(property->name()));
        }
        return property->dataType();
    }

    if(property->dataType() == DataBuffer::Float32 || property->dataType() == DataBuffer::Float64)
        return property->dataType();

    return DataBuffer::FloatDefault;
}

int averagedPropertyTypeId(const Property* property, bool overwrite)
{
    return (overwrite && property && property->isStandardProperty()) ? property->typeId() : 0;
}

void writeFloatingPointProperty(Property* property, const std::vector<FloatType>& values)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(values.size() == property->size() * property->componentCount());

    if(property->dataType() == DataBuffer::Float32) {
        BufferWriteAccess<float*, access_mode::discard_write> output(property);
        for(size_t i = 0; i < property->size(); i++) {
            for(size_t c = 0; c < property->componentCount(); c++) {
                output.set(i, c, static_cast<float>(values[i * property->componentCount() + c]));
            }
        }
    }
    else if(property->dataType() == DataBuffer::Float64) {
        BufferWriteAccess<double*, access_mode::discard_write> output(property);
        for(size_t i = 0; i < property->size(); i++) {
            for(size_t c = 0; c < property->componentCount(); c++) {
                output.set(i, c, static_cast<double>(values[i * property->componentCount() + c]));
            }
        }
    }
    else {
        throw Exception(TimeAveragingModifier::tr("Internal error: time-averaged output property '%1' has a non-floating-point data type.").arg(property->name()));
    }
}

std::vector<size_t> buildIndexMapping(
    const BufferReadAccess<IdentifierIntType>& destinationIds,
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
            throw Exception(TimeAveragingModifier::tr(
                "Detected duplicate %1 ID %2 in the %3 configuration. Time averaging cannot match elements in this case.")
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
            throw Exception(TimeAveragingModifier::tr(
                "The %1 ID %2 exists in the %3 configuration but not in the %4 configuration. Time averaging currently requires a stable set of element IDs.")
                .arg(elementName)
                .arg(id)
                .arg(destinationLabel)
                .arg(sourceLabel));
        }
        mapping[destinationIndex++] = iter->second;
    }

    return mapping;
}

void accumulatePropertyValues(const Property* property, std::vector<FloatType>& sums, const std::vector<size_t>* mapping = nullptr)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(isNumericDataType(property->dataType()));
    OVITO_ASSERT(sums.size() == property->size() * property->componentCount());

    std::vector<FloatType> componentValues(property->size());
    for(size_t c = 0; c < property->componentCount(); c++) {
        property->copyComponentTo(componentValues.begin(), c);
        if(mapping) {
            OVITO_ASSERT(mapping->size() * property->componentCount() == sums.size());
            for(size_t destinationIndex = 0; destinationIndex < mapping->size(); destinationIndex++) {
                sums[destinationIndex * property->componentCount() + c] += componentValues[(*mapping)[destinationIndex]];
            }
        }
        else {
            for(size_t i = 0; i < property->size(); i++) {
                sums[i * property->componentCount() + c] += componentValues[i];
            }
        }
    }
}

bool identifierBuffersMatch(const BufferReadAccess<IdentifierIntType>& lhs, const BufferReadAccess<IdentifierIntType>& rhs)
{
    if(!lhs || !rhs)
        return false;
    return lhs.size() == rhs.size() && std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin());
}

const Property* identifierProperty(const PropertyContainer* container)
{
    if(!container)
        return nullptr;
    if(!container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        return nullptr;
    return container->getProperty(Property::GenericIdentifierProperty);
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeAveragingModifier);
OVITO_CLASSINFO(TimeAveragingModifier, "DisplayName", "Time averaging");
OVITO_CLASSINFO(TimeAveragingModifier, "Description", "Sample a trajectory quantity over multiple frames and compute its mean value.");
OVITO_CLASSINFO(TimeAveragingModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, targetType);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, attributeName);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, table);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, propertyContainer);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, property);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, intervalStart);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, overwrite);
DEFINE_PROPERTY_FIELD(TimeAveragingModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, targetType, "Target type");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, attributeName, "Attribute");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, table, "Data table");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, propertyContainer, "Property container");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, property, "Property");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, useCustomFrameInterval, "Restrict averaging interval");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, samplingFrequency, "Sampling frequency");
SET_PROPERTY_FIELD_LABEL(TimeAveragingModifier, overwrite, "Overwrite original quantity");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeAveragingModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeAveragingModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TimeAveragingModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeAveragingModificationNode);
DEFINE_REFERENCE_FIELD(TimeAveragingModificationNode, averagedProperty);
DEFINE_REFERENCE_FIELD(TimeAveragingModificationNode, referenceIdentifiers);
DEFINE_REFERENCE_FIELD(TimeAveragingModificationNode, averagedTable);
DEFINE_REFERENCE_FIELD(TimeAveragingModificationNode, averagedCell);
DEFINE_PROPERTY_FIELD(TimeAveragingModificationNode, averagedAttributeValue);
DEFINE_PROPERTY_FIELD(TimeAveragingModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(TimeAveragingModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(TimeAveragingModifier, TimeAveragingModificationNode);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool TimeAveragingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObjectRecursive(PropertyContainer::OOClass())
        || input.containsObject<SimulationCell>()
        || input.containsObject<AttributeDataObject>();
}

/******************************************************************************
* Constructor.
******************************************************************************/
void TimeAveragingModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    setEnabled(false);
}

/******************************************************************************
* This method is called by the system when the modifier is inserted into a pipeline.
******************************************************************************/
void TimeAveragingModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(!this_task::isInteractive())
        return;

    const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();

    if((targetType() == TimeAveragingModifier::Attribute && !attributeName().isEmpty())
        || (targetType() == TimeAveragingModifier::Table && table())
        || (targetType() == TimeAveragingModifier::Property && propertyContainer() && property())
        || targetType() == TimeAveragingModifier::Cell) {
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
                if(candidate->isTypedProperty() || candidate->typeId() == Ovito::Property::GenericIdentifierProperty)
                    continue;

                setTargetType(TimeAveragingModifier::Property);
                setPropertyContainer(path);
                setProperty(candidate);
                return;
            }
        }
    }

    if(const QVariantMap attributes = input.buildAttributesMap(); !attributes.isEmpty()) {
        setTargetType(TimeAveragingModifier::Attribute);
        setAttributeName(attributes.firstKey());
        return;
    }

    const std::vector<ConstDataObjectPath> tables = input.getObjectsRecursive(DataTable::OOClass());
    if(!tables.empty()) {
        setTargetType(TimeAveragingModifier::Table);
        setTable(tables.front());
        return;
    }

    if(input.containsObject<SimulationCell>())
        setTargetType(TimeAveragingModifier::Cell);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void TimeAveragingModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(!shouldIgnoreChanges()) {
        if(field == PROPERTY_FIELD(TimeAveragingModifier::targetType)
            || field == PROPERTY_FIELD(TimeAveragingModifier::attributeName)
            || field == PROPERTY_FIELD(TimeAveragingModifier::table)
            || field == PROPERTY_FIELD(TimeAveragingModifier::propertyContainer)
            || field == PROPERTY_FIELD(TimeAveragingModifier::property)) {
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
        }
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
* Returns a short piece of information to be displayed next to the modifier title.
******************************************************************************/
QVariant TimeAveragingModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
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
std::vector<int> TimeAveragingModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for time averaging."));

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
        throw Exception(tr("The selected averaging interval does not contain any sampled frames."));

    return result;
}

/******************************************************************************
* Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
******************************************************************************/
void TimeAveragingModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    Modifier::inputCachingHints(request);
}

/******************************************************************************
* Is called by the pipeline system before a new modifier evaluation begins.
******************************************************************************/
void TimeAveragingModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Is called by the ModificationNode to let the modifier adjust the validity interval.
******************************************************************************/
void TimeAveragingModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> TimeAveragingModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(TimeAveragingModificationNode* modNode = dynamic_object_cast<TimeAveragingModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedAverage() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedAverage(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Time averaging is idle. Open the Start section and click 'Start averaging' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Time averaging is queued. Click 'Start averaging' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeAverageData(request, std::move(state));
}

/******************************************************************************
* Computes the cached average data by traversing the selected frame interval.
******************************************************************************/
Future<PipelineFlowState> TimeAveragingModifier::computeAverageData(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    // Traverse sampled frames one at a time to avoid reentrant upstream modifier evaluation.
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 1);
    const int cacheGenerationId = dynamic_object_cast<TimeAveragingModificationNode>(request.modificationNode())
        ? dynamic_object_cast<TimeAveragingModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;
    // Long-running trajectory traversal modifiers should register an outer task-progress object
    // so the status bar reports whole-trajectory progress instead of only nested upstream work.
    auto progress = std::make_shared<TaskProgress>(this_task::ui());
    progress->setText(tr("Computing time average"));
    progress->setMaximum(static_cast<qlonglong>(frames.size()));

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, RunningAverageData&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [this, progress, totalFrameCount = frames.size()](const std::vector<int>&, std::vector<SharedFuture<PipelineFlowState>> futures, RunningAverageData& accumulator) {
                for(SharedFuture<PipelineFlowState>& future : futures) {
                    this_task::throwIfCanceled();
                    const PipelineFlowState& sampleState = future.result();

                    switch(targetType()) {
                    case Attribute: {
                        if(attributeName().isEmpty())
                            throw Exception(tr("No input attribute selected for time averaging."));

                        const QVariant value = sampleState.getAttributeValue(attributeName());
                        if(!value.isValid()) {
                            throw Exception(tr("Global attribute '%1' is not available in one of the sampled trajectory frames.").arg(attributeName()));
                        }
                        accumulator.attributeSum += attributeValueToFloat(value, attributeName());
                        break;
                    }

                    case Table: {
                        if(!table())
                            throw Exception(tr("No input data table selected for time averaging."));

                        const DataTable* sampleTable = dynamic_object_cast<DataTable>(sampleState.getLeafObject(table()));
                        if(!sampleTable) {
                            throw Exception(tr("The selected data table '%1' is not available in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));
                        }
                        const Ovito::Property* sampleY = sampleTable->y();
                        if(!sampleY) {
                            throw Exception(tr("Data table '%1' has no Y-values in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));
                        }
                        if(!isNumericDataType(sampleY->dataType())) {
                            throw Exception(tr("The Y-values of data table '%1' are not numeric and cannot be time-averaged.").arg(table().dataTitleOrPath()));
                        }

                        if(!accumulator.referenceTable) {
                            accumulator.referenceTable = CloneHelper::cloneSingleObject(sampleTable, false);
                            accumulator.sums.assign(sampleY->size() * sampleY->componentCount(), FloatType(0));
                        }

                        const Ovito::Property* referenceY = accumulator.referenceTable->y();
                        if(sampleY->size() != referenceY->size() || sampleY->componentCount() != referenceY->componentCount()) {
                            throw Exception(tr("Data table '%1' changes its size or number of data columns over time and cannot be averaged.").arg(table().dataTitleOrPath()));
                        }
                        if(sampleTable->x() || accumulator.referenceTable->x()) {
                            if(!sampleTable->x() || !accumulator.referenceTable->x() || !sampleTable->x()->equals(*accumulator.referenceTable->x())) {
                                throw Exception(tr("Data table '%1' changes its x-coordinates over time and cannot be averaged.").arg(table().dataTitleOrPath()));
                            }
                        }
                        else {
                            if(sampleTable->elementCount() != accumulator.referenceTable->elementCount()
                                    || sampleTable->intervalStart() != accumulator.referenceTable->intervalStart()
                                    || sampleTable->intervalEnd() != accumulator.referenceTable->intervalEnd()) {
                                throw Exception(tr("Data table '%1' changes its x-interval over time and cannot be averaged.").arg(table().dataTitleOrPath()));
                            }
                        }

                        accumulatePropertyValues(sampleY, accumulator.sums);
                        break;
                    }

                    case Property: {
                        if(!propertyContainer())
                            throw Exception(tr("No input property container selected for time averaging."));
                        if(!property())
                            throw Exception(tr("No input property selected for time averaging."));
                        if(!property().componentName().isEmpty()) {
                            throw Exception(tr("Time averaging currently expects a full property, not an individual vector component."));
                        }

                        const PropertyContainer* sampleContainer = sampleState.getLeafObject(propertyContainer());
                        if(!sampleContainer) {
                            throw Exception(tr("The selected property container '%1' is not available in one of the sampled trajectory frames.").arg(propertyContainer().dataTitleOrPath()));
                        }
                        sampleContainer->verifyIntegrity();

                        const Ovito::Property* sampleProperty = property().findInContainer(sampleContainer);
                        if(!sampleProperty) {
                            throw Exception(tr("Property '%1' is not available in one of the sampled trajectory frames.").arg(property().nameWithComponent()));
                        }
                        if(!isNumericDataType(sampleProperty->dataType())) {
                            throw Exception(tr("Property '%1' is not numeric in one of the sampled trajectory frames.").arg(sampleProperty->name()));
                        }
                        if(sampleProperty->isTypedProperty() || sampleProperty->typeId() == Ovito::Property::GenericIdentifierProperty) {
                            throw Exception(tr("Property '%1' is not supported by the current open-source time averaging implementation.").arg(sampleProperty->name()));
                        }

                        if(!accumulator.referenceProperty) {
                            accumulator.referenceProperty = CloneHelper::cloneSingleObject(sampleProperty, false);
                            accumulator.elementDescriptionName = sampleContainer->getOOMetaClass().elementDescriptionName();
                            accumulator.sums.assign(sampleProperty->size() * sampleProperty->componentCount(), FloatType(0));
                            const Ovito::Property* ids = identifierProperty(sampleContainer);
                            if(ids)
                                accumulator.referenceIdentifiers = CloneHelper::cloneSingleObject(ids, false);
                        }

                        const Ovito::Property* referenceProperty = accumulator.referenceProperty;
                        if(sampleProperty->componentCount() != referenceProperty->componentCount()) {
                            throw Exception(tr("Property '%1' changes its number of vector components over time and cannot be averaged.").arg(referenceProperty->name()));
                        }

                        BufferReadAccess<IdentifierIntType> referenceIds(accumulator.referenceIdentifiers);
                        BufferReadAccess<IdentifierIntType> sampleIds(identifierProperty(sampleContainer));
                        if(referenceIds) {
                            if(!sampleIds) {
                                throw Exception(tr("The identifier property of %1 is missing in one of the sampled trajectory frames. The current time averaging implementation requires stable element IDs when the element order changes.")
                                    .arg(accumulator.elementDescriptionName));
                            }

                            if(identifierBuffersMatch(referenceIds, sampleIds)) {
                                accumulatePropertyValues(sampleProperty, accumulator.sums);
                            }
                            else {
                                const std::vector<size_t> mapping = buildIndexMapping(
                                    referenceIds,
                                    sampleIds,
                                    accumulator.elementDescriptionName,
                                    tr("reference"),
                                    tr("sampled"));
                                accumulatePropertyValues(sampleProperty, accumulator.sums, &mapping);
                            }
                        }
                        else {
                            if(sampleProperty->size() != referenceProperty->size()) {
                                throw Exception(tr("Property '%1' changes the number of %2 over time. The current time averaging implementation requires a stable element count or a stable identifier property.")
                                    .arg(referenceProperty->name())
                                    .arg(accumulator.elementDescriptionName));
                            }
                            accumulatePropertyValues(sampleProperty, accumulator.sums);
                        }
                        break;
                    }

                    case Cell: {
                        const SimulationCell* sampleCell = sampleState.getObject<SimulationCell>();
                        if(!sampleCell)
                            throw Exception(tr("The input trajectory does not provide a simulation cell that could be averaged."));

                        if(!accumulator.referenceCell) {
                            accumulator.referenceCell = CloneHelper::cloneSingleObject(sampleCell, false);
                            accumulator.cellSum = sampleCell->cellMatrix();
                            accumulator.hasCellSum = true;
                        }
                        else {
                            if(sampleCell->pbcFlags() != accumulator.referenceCell->pbcFlags() || sampleCell->is2D() != accumulator.referenceCell->is2D()) {
                                throw Exception(tr("The periodic boundary settings of the simulation cell change over time and cannot be averaged."));
                            }
                            accumulator.cellSum += sampleCell->cellMatrix();
                        }
                        break;
                    }
                    }

                    accumulator.sampleCount++;
                    progress->setText(tr("Computing time average (%1/%2 frames)")
                                          .arg(accumulator.sampleCount)
                                          .arg(totalFrameCount));
                    progress->setValue(static_cast<qlonglong>(accumulator.sampleCount));
                }
            },
            RunningAverageData{})
        .then(ObjectExecutor(this), [this, request, state = std::move(state), progress = std::move(progress), cacheGenerationId](RunningAverageData accumulator) mutable {
            TimeAveragingModificationNode* modNode = dynamic_object_cast<TimeAveragingModificationNode>(request.modificationNode());
            if(!modNode)
                return std::move(state);
            const int completedRunRequestId = runRequestId();

            if(accumulator.sampleCount == 0)
                throw Exception(tr("The selected averaging interval does not contain any sampled frames."));

            switch(targetType()) {
            case Attribute: {
                modNode->setAveragedAttributeValue(static_cast<double>(accumulator.attributeSum / accumulator.sampleCount));
                break;
            }
            case Table: {
                if(!accumulator.referenceTable)
                    throw Exception(tr("No cached time-averaged data table is available."));

                for(FloatType& value : accumulator.sums)
                    value /= accumulator.sampleCount;

                const Ovito::Property* referenceY = accumulator.referenceTable->y();
                DataOORef<DataTable> averagedTable = std::move(accumulator.referenceTable);
                averagedTable->setIdentifier(averagedTableId(averagedTable->identifier(), overwrite()));
                averagedTable->setCreatedByNode(request.modificationNodeWeak());

                PropertyPtr averagedY = referenceY->cloneWithoutData(referenceY->size(), DataBuffer::FloatDefault);
                averagedY->setCreatedByNode(request.modificationNodeWeak());
                writeFloatingPointProperty(averagedY, accumulator.sums);
                averagedTable->setY(std::move(averagedY));

                modNode->setAveragedTable(std::move(averagedTable));
                break;
            }
            case Property: {
                if(!accumulator.referenceProperty)
                    throw Exception(tr("No cached time-averaged property data is available."));

                for(FloatType& value : accumulator.sums)
                    value /= accumulator.sampleCount;

                const int outputDataType = averagedPropertyDataType(accumulator.referenceProperty, overwrite());
                const int outputTypeId = averagedPropertyTypeId(accumulator.referenceProperty, overwrite());
                PropertyPtr averagedProperty = accumulator.referenceProperty->cloneWithoutData(accumulator.referenceProperty->size(), outputDataType);
                averagedProperty->setTypeId(outputTypeId);
                averagedProperty->setName(averagedPropertyName(accumulator.referenceProperty->name(), overwrite()));
                averagedProperty->setCreatedByNode(request.modificationNodeWeak());
                writeFloatingPointProperty(averagedProperty, accumulator.sums);

                modNode->setAveragedProperty(std::move(averagedProperty));
                if(accumulator.referenceIdentifiers)
                    modNode->setReferenceIdentifiers(std::move(accumulator.referenceIdentifiers));
                break;
            }
            case Cell: {
                if(!accumulator.referenceCell || !accumulator.hasCellSum)
                    throw Exception(tr("No cached time-averaged simulation cell is available."));

                DataOORef<SimulationCell> averagedCell = std::move(accumulator.referenceCell);
                averagedCell->setCreatedByNode(request.modificationNodeWeak());
                if(!overwrite())
                    averagedCell->setIdentifier(QStringLiteral("simulation-cell[average]"));
                averagedCell->setCellMatrix(accumulator.cellSum * (FloatType(1) / accumulator.sampleCount));

                modNode->setAveragedCell(std::move(averagedCell));
                break;
            }
            }

            if(modNode->cacheGenerationId() != cacheGenerationId || runRequestId() != completedRunRequestId)
                return std::move(state);

            modNode->setCompletedRunRequestId(completedRunRequestId);
            progress->setText(tr("Computed time average from %1 sampled frame(s).").arg(accumulator.sampleCount));
            progress->setValue(static_cast<qlonglong>(accumulator.sampleCount));
            return applyCachedAverage(request, std::move(state));
        });
}

/******************************************************************************
* Creates the cached average of a scalar attribute.
******************************************************************************/
void TimeAveragingModifier::computeAttributeAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const
{
    OVITO_ASSERT(modNode);

    if(attributeName().isEmpty())
        throw Exception(tr("No input attribute selected for time averaging."));

    FloatType sum = 0;
    for(const PipelineFlowState& sampleState : sampleStates) {
        const QVariant value = sampleState.getAttributeValue(attributeName());
        if(!value.isValid()) {
            throw Exception(tr("Global attribute '%1' is not available in one of the sampled trajectory frames.").arg(attributeName()));
        }
        sum += attributeValueToFloat(value, attributeName());
    }

    modNode->setAveragedAttributeValue(static_cast<double>(sum / sampleStates.size()));
}

/******************************************************************************
* Creates the cached average of a data table.
******************************************************************************/
void TimeAveragingModifier::computeTableAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const
{
    OVITO_ASSERT(modNode);

    if(!table())
        throw Exception(tr("No input data table selected for time averaging."));

    const DataTable* referenceTable = dynamic_object_cast<DataTable>(sampleStates.front().getLeafObject(table()));
    if(!referenceTable) {
        throw Exception(tr("The selected data table '%1' is not available in the first sampled trajectory frame.").arg(table().dataTitleOrPath()));
    }
    const Ovito::Property* referenceY = referenceTable->y();
    if(!referenceY) {
        throw Exception(tr("Data table '%1' does not contain any Y-values that could be averaged.").arg(table().dataTitleOrPath()));
    }
    if(!isNumericDataType(referenceY->dataType())) {
        throw Exception(tr("The Y-values of data table '%1' are not numeric and cannot be time-averaged.").arg(table().dataTitleOrPath()));
    }

    std::vector<FloatType> sums(referenceY->size() * referenceY->componentCount(), FloatType(0));

    for(const PipelineFlowState& sampleState : sampleStates) {
        const DataTable* sampleTable = dynamic_object_cast<DataTable>(sampleState.getLeafObject(table()));
        if(!sampleTable) {
            throw Exception(tr("The selected data table '%1' is not available in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));
        }
        const Ovito::Property* sampleY = sampleTable->y();
        if(!sampleY) {
            throw Exception(tr("Data table '%1' has no Y-values in one of the sampled trajectory frames.").arg(table().dataTitleOrPath()));
        }
        if(sampleY->size() != referenceY->size() || sampleY->componentCount() != referenceY->componentCount()) {
            throw Exception(tr("Data table '%1' changes its size or number of data columns over time and cannot be averaged.").arg(table().dataTitleOrPath()));
        }
        if(sampleTable->x() || referenceTable->x()) {
            if(!sampleTable->x() || !referenceTable->x() || !sampleTable->x()->equals(*referenceTable->x())) {
                throw Exception(tr("Data table '%1' changes its x-coordinates over time and cannot be averaged.").arg(table().dataTitleOrPath()));
            }
        }
        else {
            if(sampleTable->elementCount() != referenceTable->elementCount()
                    || sampleTable->intervalStart() != referenceTable->intervalStart()
                    || sampleTable->intervalEnd() != referenceTable->intervalEnd()) {
                throw Exception(tr("Data table '%1' changes its x-interval over time and cannot be averaged.").arg(table().dataTitleOrPath()));
            }
        }

        accumulatePropertyValues(sampleY, sums);
    }

    for(FloatType& value : sums)
        value /= sampleStates.size();

    DataOORef<DataTable> averagedTable = CloneHelper::cloneSingleObject(referenceTable, false);
    averagedTable->setIdentifier(averagedTableId(referenceTable->identifier(), overwrite()));
    averagedTable->setCreatedByNode(request.modificationNodeWeak());

    PropertyPtr averagedY = referenceY->cloneWithoutData(referenceY->size(), DataBuffer::FloatDefault);
    averagedY->setCreatedByNode(request.modificationNodeWeak());
    writeFloatingPointProperty(averagedY, sums);
    averagedTable->setY(std::move(averagedY));

    modNode->setAveragedTable(std::move(averagedTable));
}

/******************************************************************************
* Creates the cached average of an element-wise property.
******************************************************************************/
void TimeAveragingModifier::computePropertyAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const
{
    OVITO_ASSERT(modNode);

    if(!propertyContainer())
        throw Exception(tr("No input property container selected for time averaging."));
    if(!property())
        throw Exception(tr("No input property selected for time averaging."));
    if(!property().componentName().isEmpty()) {
        throw Exception(tr("Time averaging currently expects a full property, not an individual vector component."));
    }

    const PropertyContainer* referenceContainer = sampleStates.front().getLeafObject(propertyContainer());
    if(!referenceContainer) {
        throw Exception(tr("The selected property container '%1' is not available in the first sampled trajectory frame.").arg(propertyContainer().dataTitleOrPath()));
    }
    referenceContainer->verifyIntegrity();

    const Ovito::Property* referenceProperty = property().findInContainer(referenceContainer);
    if(!referenceProperty) {
        throw Exception(tr("The selected property '%1' is not available in the first sampled trajectory frame.").arg(property().nameWithComponent()));
    }
    if(!isNumericDataType(referenceProperty->dataType())) {
        throw Exception(tr("Property '%1' is not numeric and cannot be time-averaged.").arg(referenceProperty->name()));
    }
    if(referenceProperty->isTypedProperty() || referenceProperty->typeId() == Ovito::Property::GenericIdentifierProperty) {
        throw Exception(tr("Property '%1' is not supported by the current open-source time averaging implementation.").arg(referenceProperty->name()));
    }

    BufferReadAccess<IdentifierIntType> referenceIds(identifierProperty(referenceContainer));
    std::vector<FloatType> sums(referenceProperty->size() * referenceProperty->componentCount(), FloatType(0));

    for(const PipelineFlowState& sampleState : sampleStates) {
        const PropertyContainer* sampleContainer = sampleState.getLeafObject(propertyContainer());
        if(!sampleContainer) {
            throw Exception(tr("The selected property container '%1' is not available in one of the sampled trajectory frames.").arg(propertyContainer().dataTitleOrPath()));
        }
        sampleContainer->verifyIntegrity();

        const Ovito::Property* sampleProperty = property().findInContainer(sampleContainer);
        if(!sampleProperty) {
            throw Exception(tr("Property '%1' is not available in one of the sampled trajectory frames.").arg(property().nameWithComponent()));
        }
        if(sampleProperty->componentCount() != referenceProperty->componentCount()) {
            throw Exception(tr("Property '%1' changes its number of vector components over time and cannot be averaged.").arg(referenceProperty->name()));
        }
        if(!isNumericDataType(sampleProperty->dataType())) {
            throw Exception(tr("Property '%1' is not numeric in one of the sampled trajectory frames.").arg(referenceProperty->name()));
        }

        BufferReadAccess<IdentifierIntType> sampleIds(identifierProperty(sampleContainer));
        if(referenceIds) {
            if(!sampleIds) {
                throw Exception(tr("The identifier property of %1 is missing in one of the sampled trajectory frames. The current time averaging implementation requires stable element IDs when the element order changes.")
                    .arg(referenceContainer->getOOMetaClass().elementDescriptionName()));
            }

            if(identifierBuffersMatch(referenceIds, sampleIds)) {
                accumulatePropertyValues(sampleProperty, sums);
            }
            else {
                const std::vector<size_t> mapping = buildIndexMapping(
                    referenceIds,
                    sampleIds,
                    referenceContainer->getOOMetaClass().elementDescriptionName(),
                    tr("reference"),
                    tr("sampled"));
                accumulatePropertyValues(sampleProperty, sums, &mapping);
            }
        }
        else {
            if(sampleProperty->size() != referenceProperty->size()) {
                throw Exception(tr("Property '%1' changes the number of %2 over time. The current time averaging implementation requires a stable element count or a stable identifier property.")
                    .arg(referenceProperty->name())
                    .arg(referenceContainer->getOOMetaClass().elementDescriptionName()));
            }
            accumulatePropertyValues(sampleProperty, sums);
        }
    }

    for(FloatType& value : sums)
        value /= sampleStates.size();

    const int outputDataType = averagedPropertyDataType(referenceProperty, overwrite());
    const int outputTypeId = averagedPropertyTypeId(referenceProperty, overwrite());
    PropertyPtr averagedProperty = referenceProperty->cloneWithoutData(referenceProperty->size(), outputDataType);
    averagedProperty->setTypeId(outputTypeId);
    averagedProperty->setName(averagedPropertyName(referenceProperty->name(), overwrite()));
    averagedProperty->setCreatedByNode(request.modificationNodeWeak());
    writeFloatingPointProperty(averagedProperty, sums);

    modNode->setAveragedProperty(std::move(averagedProperty));
    if(referenceIds) {
        modNode->setReferenceIdentifiers(CloneHelper::cloneSingleObject(identifierProperty(referenceContainer), false));
    }
}

/******************************************************************************
* Creates the cached average of the simulation cell.
******************************************************************************/
void TimeAveragingModifier::computeCellAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const
{
    OVITO_ASSERT(modNode);

    const SimulationCell* referenceCell = sampleStates.front().getObject<SimulationCell>();
    if(!referenceCell)
        throw Exception(tr("The input trajectory does not provide a simulation cell that could be averaged."));

    AffineTransformation averageCell = referenceCell->cellMatrix();
    for(auto iter = std::next(sampleStates.begin()); iter != sampleStates.end(); ++iter) {
        const SimulationCell* sampleCell = iter->getObject<SimulationCell>();
        if(!sampleCell)
            throw Exception(tr("The simulation cell is missing in one of the sampled trajectory frames."));
        if(sampleCell->pbcFlags() != referenceCell->pbcFlags() || sampleCell->is2D() != referenceCell->is2D()) {
            throw Exception(tr("The periodic boundary settings of the simulation cell change over time and cannot be averaged."));
        }
        averageCell += sampleCell->cellMatrix();
    }
    averageCell = averageCell * (FloatType(1) / sampleStates.size());

    DataOORef<SimulationCell> averagedCell = CloneHelper::cloneSingleObject(referenceCell, false);
    averagedCell->setCreatedByNode(request.modificationNodeWeak());
    if(!overwrite())
        averagedCell->setIdentifier(QStringLiteral("simulation-cell[average]"));
    averagedCell->setCellMatrix(averageCell);

    modNode->setAveragedCell(std::move(averagedCell));
}

/******************************************************************************
* Applies the cached average data to the current pipeline state.
******************************************************************************/
PipelineFlowState TimeAveragingModifier::applyCachedAverage(const ModifierEvaluationRequest& request, PipelineFlowState state) const
{
    TimeAveragingModificationNode* modNode = dynamic_object_cast<TimeAveragingModificationNode>(request.modificationNode());
    if(!modNode)
        return state;

    switch(targetType()) {
    case Attribute: {
        if(attributeName().isEmpty())
            throw Exception(tr("No input attribute selected for time averaging."));
        if(!modNode->averagedAttributeValue().isValid())
            throw Exception(tr("No cached time-averaged attribute value is available."));
        state.setAttribute(averagedAttributeName(attributeName(), overwrite()), modNode->averagedAttributeValue(), request.modificationNodeWeak());
        break;
    }

    case Table: {
        if(!modNode->averagedTable())
            throw Exception(tr("No cached time-averaged data table is available."));

        if(overwrite()) {
            if(const DataObject* tableObject = state.getLeafObject(table()))
                state.replaceObject(tableObject, modNode->averagedTable());
            else
                state.addObject(modNode->averagedTable());
        }
        else {
            state.addObjectWithUniqueId(modNode->averagedTable());
        }
        break;
    }

    case Property: {
        if(!propertyContainer())
            throw Exception(tr("No input property container selected for time averaging."));
        if(!modNode->averagedProperty())
            throw Exception(tr("No cached time-averaged property data is available."));

        PropertyContainer* container = state.expectMutableLeafObject(propertyContainer());
        container->verifyIntegrity();

        ConstPropertyPtr outputProperty = modNode->averagedProperty();
        BufferReadAccess<IdentifierIntType> referenceIds(modNode->referenceIdentifiers());
        if(referenceIds) {
            BufferReadAccess<IdentifierIntType> currentIds(identifierProperty(container));
            if(!currentIds) {
                throw Exception(tr("The current frame does not provide the identifier property required to map the time-averaged values to the current set of %1.")
                    .arg(container->getOOMetaClass().elementDescriptionName()));
            }

            if(!identifierBuffersMatch(currentIds, referenceIds)) {
                const std::vector<size_t> mapping = buildIndexMapping(
                    currentIds,
                    referenceIds,
                    container->getOOMetaClass().elementDescriptionName(),
                    tr("current"),
                    tr("averaged reference"));

                PropertyPtr reorderedProperty = modNode->averagedProperty()->cloneWithoutData(container->elementCount(), modNode->averagedProperty()->dataType());
                reorderedProperty->setCreatedByNode(request.modificationNodeWeak());
                modNode->averagedProperty()->mappedCopyTo(*reorderedProperty, mapping);
                outputProperty = std::move(reorderedProperty);
            }
            else if(currentIds.size() != modNode->averagedProperty()->size()) {
                throw Exception(tr("The number of %1 in the current frame does not match the cached time-averaged property data.").arg(container->getOOMetaClass().elementDescriptionName()));
            }
        }
        else if(container->elementCount() != modNode->averagedProperty()->size()) {
            throw Exception(tr("The number of %1 in the current frame does not match the cached time-averaged property data. This open-source implementation currently requires a stable element count.")
                .arg(container->getOOMetaClass().elementDescriptionName()));
        }

        container->createProperty(outputProperty);
        break;
    }

    case Cell: {
        if(!modNode->averagedCell())
            throw Exception(tr("No cached time-averaged simulation cell is available."));

        if(overwrite()) {
            SimulationCell* cell = state.expectMutableObject<SimulationCell>();
            cell->setCellMatrix(modNode->averagedCell()->cellMatrix());
            cell->setPbcFlags(modNode->averagedCell()->pbcFlags());
            cell->setIs2D(modNode->averagedCell()->is2D());
        }
        else {
            state.addObjectWithUniqueId(modNode->averagedCell());
        }
        break;
    }
    }

    return state;
}

/******************************************************************************
* Clears all cached average data.
******************************************************************************/
void TimeAveragingModificationNode::invalidateCachedAverage()
{
    setAveragedProperty(nullptr);
    setReferenceIdentifiers(nullptr);
    setAveragedTable(nullptr);
    setAveragedCell(nullptr);
    setAveragedAttributeValue(QVariant{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool TimeAveragingModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedAverage();
    }
    return ModificationNode::referenceEvent(source, event);
}

}   // End of namespace
