////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/undo/RefTargetOperations.h>
#include <ovito/core/oo/CloneHelper.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Pipeline);
OVITO_CLASSINFO(Pipeline, "ClassNameAlias", "PipelineSceneNode");  // For backward compatibility with OVITO 3.9.2
DEFINE_REFERENCE_FIELD(Pipeline, head);
DEFINE_VECTOR_REFERENCE_FIELD(Pipeline, visElements);
DEFINE_RUNTIME_PROPERTY_FIELD(Pipeline, replacedVisElements);
DEFINE_VECTOR_REFERENCE_FIELD(Pipeline, replacementVisElements);
DEFINE_REFERENCE_FIELD(Pipeline, source);
DEFINE_PROPERTY_FIELD(Pipeline, pipelineTrajectoryCachingEnabled);
DEFINE_PROPERTY_FIELD(Pipeline, preliminaryUpdatesEnabled);
SET_PROPERTY_FIELD_LABEL(Pipeline, head, "Pipeline object");
SET_PROPERTY_FIELD_LABEL(Pipeline, pipelineTrajectoryCachingEnabled, "Precompute all trajectory frames");
SET_PROPERTY_FIELD_LABEL(Pipeline, source, "Pipeline source");
SET_PROPERTY_FIELD_CHANGE_EVENT(Pipeline, head, ReferenceEvent::PipelineChanged);
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(Pipeline, head, "dataProvider"); // For backward compatibility with OVITO 3.9.2
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(Pipeline, source, "pipelineSource"); // For backward compatibility with OVITO 3.9.2

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
Pipeline::~Pipeline() // NOLINT
{
}
#endif

/******************************************************************************
* Throws an exception if the pipeline stage cannot be evaluated at this time.
* This is called by the system to catch user mistakes that would lead to infinite recursion.
******************************************************************************/
void Pipeline::preEvaluationCheck(const PipelineEvaluationRequest& request) const
{
    if(head())
        head()->preEvaluationCheck(request);
}

/******************************************************************************
* Invalidates the data pipeline cache of the object node.
******************************************************************************/
void Pipeline::invalidatePipelineCache(TimeInterval keepInterval)
{
    // Invalidate data cache.
    _pipelineCache.invalidate(keepInterval);

    // Also mark the cached bounding box of the scene node as invalid.
    notifyDependents(Pipeline::BoundingBoxChanged);
}

/******************************************************************************
* Helper function that recursively collects all visual elements attached to a
* data object and its children and stores them in an output vector.
******************************************************************************/
void Pipeline::collectVisElements(const DataObject* dataObj, std::vector<std::pair<DataVis*, QStringList>>& visElements)
{
    for(DataVis* vis : dataObj->visElements()) {
        auto iter = std::ranges::find_if(visElements, [vis](const auto& item) {
            return item.first == vis;
        });
        if(iter == visElements.end())
            visElements.emplace_back(vis, QStringList() << dataObj->objectTitle());
        else
            iter->second << dataObj->objectTitle();
    }

    dataObj->visitSubObjects([&visElements](const DataObject* subObject) {
        collectVisElements(subObject, visElements);
    });
}

/******************************************************************************
* Rebuilds the list of visual elements maintained by the scene node.
******************************************************************************/
void Pipeline::updateVisElementList(const PipelineFlowState& state)
{
    OVITO_ASSERT(_visElementDataObjectTitles.size() == visElements().size() || _visElementDataObjectTitles.empty());

    // Collect all visual elements from the current pipeline state.
    std::vector<std::pair<DataVis*, QStringList>> newVisElements;
    if(state.data())
        collectVisElements(state.data(), newVisElements);

    // Perform the replacement of vis elements according to the replacement map.
    if(!replacedVisElements().empty()) {
        for(auto& [vis, _] : newVisElements) {
            DataVis* oldVis = vis;
            vis = getReplacementVisElement(vis);
            if(vis != oldVis) {
                // Make the same replacement in the output list to maintain the original ordering.
                if(auto index = _visElements.indexOf(oldVis); index >= 0)
                    _visElements.set(this, PROPERTY_FIELD(visElements), index, vis);
            }
        }

        // Remove duplicates of a vis elements from the list, which may occur due to replacements with the same element.
        for(int i = 0; i < newVisElements.size(); i++) {
            auto& [vis, titles] = newVisElements[i];
            auto iter = std::ranges::find_if(newVisElements, [vis](const auto& item) { return item.first == vis; });
            if(iter != newVisElements.begin() + i) {
                OVITO_ASSERT(iter != newVisElements.end());
                // Duplicate found, remove it. Also join titles lists.
                iter->second << titles;
                newVisElements.erase(newVisElements.begin() + i);
                i--;
            }
        }
    }

    // To maintain a stable ordering, first discard those elements from the old list which are not in the new list.
    for(auto i = visElements().size(); i--; ) {
        DataVis* vis = visElements()[i];
        OVITO_ASSERT(visElements().count(vis) == 1);
        if(std::ranges::find_if(newVisElements, [vis](const auto& item) { return item.first == vis; }) == newVisElements.end()) {
            if(_visElementDataObjectTitles.size() == visElements().size())
                _visElementDataObjectTitles.erase(_visElementDataObjectTitles.begin() + i);
            _visElements.remove(this, PROPERTY_FIELD(visElements), i);
        }
    }
    _visElementDataObjectTitles.resize(_visElements.size());

    // Now add any new visual elements to the end of the list.
    for(auto& [vis, titles] : newVisElements) {
        OVITO_CHECK_OBJECT_POINTER(vis);
        OVITO_ASSERT(std::ranges::count_if(newVisElements, [vis](const auto& item) { return item.first == vis; }) == 1);
        auto index = visElements().indexOf(vis);
        if(index == -1) {
            _visElements.push_back(this, PROPERTY_FIELD(visElements), vis);
            _visElementDataObjectTitles.push_back(std::move(titles));
        }
        else {
            _visElementDataObjectTitles[index] = std::move(titles);
        }
    }

    // Consistency checks.
    OVITO_ASSERT(visElements().size() == newVisElements.size());
    OVITO_ASSERT(visElements().size() == _visElementDataObjectTitles.size());

    // Since this method was invoked after a completed pipeline evaluation, inform all vis elements that their input state has changed.
    for(DataVis* vis : visElements())
        vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
}

/******************************************************************************
* Returns the titles of all data objects associated with a visual element produced by this pipeline.
******************************************************************************/
const QStringList* Pipeline::getDataObjectTitlesForVisElement(DataVis* vis) const
{
    OVITO_ASSERT(visElements().size() == _visElementDataObjectTitles.size() || _visElementDataObjectTitles.empty());
    auto index = visElements().indexOf(vis);
    return (index != -1 && index < _visElementDataObjectTitles.size()) ? &_visElementDataObjectTitles[index] : nullptr;
}

/******************************************************************************
* This method is called when a referenced object has changed.
******************************************************************************/
bool Pipeline::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == head()) {
        if(event.type() == ReferenceEvent::TargetChanged) {
            invalidatePipelineCache(static_cast<const TargetChangedEvent&>(event).unchangedInterval());
        }
        else if(event.type() == ReferenceEvent::TargetDeleted) {
            // Reduce memory footprint when the pipeline's data provider gets deleted.
            _pipelineCache.reset();

            // Data provider has been deleted -> delete pipeline as well.
            if(!isUndoingOrRedoing())
                requestObjectDeletion();
        }
        else if(event.type() == ReferenceEvent::PipelineChanged) {
            // Determine the new source node of the pipeline.
            updatePipelineSourceReference();
            // Forward pipeline changed events from the pipeline.
            return true;
        }
        else if(event.type() == ReferenceEvent::AnimationFramesChanged) {
            // Forward animation interval events from the pipeline.
            return true;
        }
        else if(event.type() == ReferenceEvent::InteractiveStateAvailable) {
            if(preliminaryUpdatesEnabled()) {
                // Invalidate the cache whenever the last pipeline stage can provide a new interactive state.
                _pipelineCache.invalidateInteractiveState();
                // Recompute the cached bounding box of the scene node.
                notifyDependents(Pipeline::BoundingBoxChanged);
                // Inform all vis elements that their input state has changed when the pipeline reports that a new preliminary output state is available.
                for(DataVis* vis : visElements())
                    vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
                // Do not forward this signal to ScenePreparation objects if it comes from the final pipeline stage.
                // That's to avoid refreshing the viewports twice - once for the preliminary update and once for the final update
                // when the pipeline evaluation is completed.
                if(event.sender() == head() && head()->shouldRefreshViewportsAfterEvaluation())
                    return false;
            }
            else {
                // Do not forward signal to scene in order to suppress interactive viewport updates.
                return false;
            }
        }
        else if(event.type() == ReferenceEvent::TargetEnabledOrDisabled) {
            // Inform vis elements that their input state has changed if the last pipeline stage was disabled.
            // This is necessary, because we don't receive a InteractiveStateAvailable signal from the pipeline stage in this case.
            for(DataVis* vis : visElements())
                vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
        }
    }
    else if(_visElements.contains(source)) {
        if(event.type() == ReferenceEvent::TargetChanged) {
            // Recompute bounding box of scene node when a visual element changes.
            notifyDependents(Pipeline::BoundingBoxChanged);

            // Trigger an interactive viewport repaint without pipeline re-evaluation.
            notifyDependents(ReferenceEvent::InteractiveStateAvailable);
        }
    }
    if(source == this->source() && event.type() == ReferenceEvent::TitleChanged) {
        // Forward this event to scene nodes referencing this the pipeline.
        return true;
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data provider of the pipeline has been replaced.
******************************************************************************/
void Pipeline::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(head)) {
        // Invalidate caches when the pipeline's data provider is replaced.
        invalidatePipelineCache();

        // The animation length and the title of the pipeline might have changed.
        if(!isBeingLoaded() && !isBeingDeleted())
            notifyDependents(ReferenceEvent::AnimationFramesChanged);

        // Determine the new source node of the pipeline.
        updatePipelineSourceReference();
    }
    else if(field == PROPERTY_FIELD(replacedVisElements)) {
        OVITO_ASSERT(false);
    }
    else if(field == PROPERTY_FIELD(replacementVisElements)) {
        // Reset pipeline cache if a new replacement for a visual element is assigned.
        invalidatePipelineCache();
    }
    else if(field == PROPERTY_FIELD(source)) {
        // When the source node of the pipeline is being replaced, the pipeline's title changes.
        notifyDependents(ReferenceEvent::TitleChanged);
    }
    RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Returns the list of scene nodes that share this pipeline.
******************************************************************************/
QVector<SceneNode*> Pipeline::sceneNodes() const
{
    QVector<SceneNode*> list;
    visitDependents([&](RefMaker* dependent) {
        SceneNode* node = dynamic_object_cast<SceneNode>(dependent);
        if(node != nullptr && node->pipeline() == this)
            list.push_back(node);
    });
    return list;
}

/******************************************************************************
* Returns one of the scene nodes referencing this pipeline (if any).
******************************************************************************/
SceneNode* Pipeline::someSceneNode() const
{
    SceneNode* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        SceneNode* node = dynamic_object_cast<SceneNode>(dependent);
        if(node != nullptr && node->pipeline() == this)
            result = node;
    });
    return result;
}

/******************************************************************************
* Determines  whether this pipeline is currently part of a visualization scene.
******************************************************************************/
bool Pipeline::isInScene() const
{
    bool result = false;
    visitDependents([&](RefMaker* dependent) {
        SceneNode* node = dynamic_object_cast<SceneNode>(dependent);
        if(node != nullptr && node->pipeline() == this && node->isInScene())
            result = true;
    });
    return result;
}

/******************************************************************************
* Rescales the times of all animation keys from the old animation interval to the new interval.
******************************************************************************/
void Pipeline::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
    RefTarget::rescaleTime(oldAnimationInterval, newAnimationInterval);
    _pipelineCache.invalidate();
}

/******************************************************************************
* Returns the title of this object.
******************************************************************************/
QString Pipeline::objectTitle() const
{
    // Use the title of the pipeline's data source by default.
    if(source())
        return source()->objectTitle();

    // Fall back to default behavior.
    return RefTarget::objectTitle();
}

/******************************************************************************
* Applies a modifier by appending a node for it to the pipeline.
******************************************************************************/
ModificationNode* Pipeline::applyModifier(AnimationTime time, bool interactiveMode, Modifier* modifier)
{
    OVITO_ASSERT(modifier);
    OVITO_ASSERT(this_task::get());

    OORef<ModificationNode> node = modifier->createModificationNode();
    node->setModifier(modifier);
    node->setInput(head());
    modifier->initializeModifier(ModifierInitializationRequest(time, false, interactiveMode, node));
    setHead(node);
    return node;
}

/******************************************************************************
* Determines the current source of the data pipeline and updates the internal weak reference field.
******************************************************************************/
void Pipeline::updatePipelineSourceReference()
{
    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(head()))
        _source.set(this, PROPERTY_FIELD(source), modNode->pipelineSource());
    else
        _source.set(this, PROPERTY_FIELD(source), head());
}

/******************************************************************************
* Sets the data source of this pipeline, i.e., the node that generates the
* input data entering the pipeline.
******************************************************************************/
void Pipeline::setSource(PipelineNode* sourceObject)
{
    ModificationNode* modNode = dynamic_object_cast<ModificationNode>(head());
    if(!modNode) {
        setHead(sourceObject);
    }
    else {
        for(;;) {
            if(ModificationNode* modNodePredecessor = dynamic_object_cast<ModificationNode>(modNode->input()))
                modNode = modNodePredecessor;
            else
                break;
        }
        modNode->setInput(sourceObject);
    }
    OVITO_ASSERT(ModificationNode::OOClass().isMember(sourceObject) || this->source() == sourceObject);
}

/******************************************************************************
* Computes the axis-aligned bounding box of the pipeline's visual output in local coordinates.
******************************************************************************/
Box3 Pipeline::localBoundingBox(AnimationTime time, TimeInterval& validity) const
{
    const PipelineFlowState& state = getCachedPipelineOutput(time);

    // Let visual elements compute the bounding boxes of the data objects.
    Box3 bb;
    ConstDataObjectPath dataObjectPath;
    if(state.data())
        getDataObjectBoundingBox(time, state.data(), state, validity, bb, dataObjectPath);
    OVITO_ASSERT(dataObjectPath.empty());
    validity.intersect(state.stateValidity());
    return bb;
}

/******************************************************************************
* Computes the bounding box of a data object and all its sub-objects.
******************************************************************************/
void Pipeline::getDataObjectBoundingBox(AnimationTime time, const DataObject* dataObj, const PipelineFlowState& state, TimeInterval& validity, Box3& bb, ConstDataObjectPath& dataObjectPath) const
{
    bool isOnStack = false;

    // Call all vis elements of the data object.
    for(DataVis* vis : dataObj->visElements()) {
        // Let the pipeline substitute the vis element with another one.
        vis = getReplacementVisElement(vis);
        if(vis->isEnabled()) {
            // Push the data object onto the stack.
            if(!isOnStack) {
                dataObjectPath.push_back(dataObj);
                isOnStack = true;
            }
            try {
                // Let the vis element compute the bounding box in local coordinates.
                bb.addBox(vis->boundingBoxImmediate(time, dataObjectPath, this, state, validity));
            }
            catch(const Exception& ex) {
                // Swallow any errors that might occur during the computation of the bounding box after printing them to the console.
                ex.logError();
            }
        }
    }

    // Recursively visit all sub-objects of this data object and render them as well.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        // Push the data object onto the stack.
        if(!isOnStack) {
            dataObjectPath.push_back(dataObj);
            isOnStack = true;
        }
        getDataObjectBoundingBox(time, subObject, state, validity, bb, dataObjectPath);
    });

    // Pop the data object from the stack.
    if(isOnStack) {
        dataObjectPath.pop_back();
    }
}

/******************************************************************************
* Asks this object to delete itself.
******************************************************************************/
void Pipeline::requestObjectDeletion()
{
    OVITO_ASSERT(this_task::get());

    // Temporary reference to the pipeline's stages.
    OORef<PipelineNode> oldHead = head();

    // Throw away data source.
    // This will also clear the caches of the pipeline.
    setHead(nullptr);

    // Walk along the pipeline and delete the individual modifiers/source objects (unless they are shared with another pipeline).
    // This is necessary to update any other references the scene may have to the pipeline's modifiers,
    // e.g. the ColorLegendOverlay.
    while(oldHead) {
        OORef<PipelineNode> next;
        if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(oldHead.get()))
            next = modNode->input();
        // Delete the pipeline stage if it is not part of any other pipeline in the scene.
        if(oldHead->pipelines(false).isEmpty())
            oldHead->requestObjectDeletion();
        oldHead = std::move(next);
    }

    // Discard transient references to visual elements.
    _visElements.clear(this, PROPERTY_FIELD(visElements));

    RefTarget::requestObjectDeletion();
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
******************************************************************************/
void Pipeline::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(replacementVisElements) && !isBeingLoaded()) {
        // Reset pipeline cache if a new replacement for a visual element is assigned.
        invalidatePipelineCache();
    }
    RefTarget::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been removed from a VectorReferenceField.
******************************************************************************/
void Pipeline::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(replacementVisElements) && !isBeingDeleted()) {
        // Reset pipeline cache if a replacement for a visual element is removed.
        invalidatePipelineCache();
    }
    RefTarget::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void Pipeline::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(pipelineTrajectoryCachingEnabled)) {
        _pipelineCache.setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());

        // Send target changed event to trigger a new pipeline evaluation, which is
        // needed to start the precomputation process.
        if(pipelineTrajectoryCachingEnabled())
            notifyTargetChanged(PROPERTY_FIELD(pipelineTrajectoryCachingEnabled));
    }

    RefTarget::propertyChanged(field);
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void Pipeline::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    RefTarget::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x02);
    // Save list of weak references to vis elements that have been replaced with local copies.
    stream.writeSizeT(replacedVisElements().size());
    for(const auto& weakRef : replacedVisElements()) {
        stream.saveObject(weakRef.lock().get(), excludeRecomputableData);
    }
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void Pipeline::loadFromStream(ObjectLoadStream& stream)
{
    RefTarget::loadFromStream(stream);

    // For backward compatibility with OVITO 3.11:
    // The Pipeline class used to be derived from the SceneNode class.
    // Parse the placeholder chunk which used to be written by the SceneNode::saveToStream() method in older OVITO versions.
    if(stream.formatVersion() < 30013) {
        stream.expectChunk(0x02);
        stream.closeChunk();
    }

    int version = stream.expectChunkRange(0x01, 1);
    if(version >= 1) {
        // Load list of weak references to replaced vis elements.
        std::vector<OOWeakRef<DataVis>> visElements(stream.readSizeT());
        for(auto& weakRef : visElements) {
            weakRef = stream.loadObject<DataVis>();
        }
        setReplacedVisElements(std::move(visElements));
    }
    stream.closeChunk();

    // Transfer the caching flag loaded from the state file to the internal cache instance.
    _pipelineCache.setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void Pipeline::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);

    OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());
    OVITO_ASSERT(!isUndoRecording());

    // Remove null entries from the replacedVisElements and replacementVisElements lists due to expired weak references.
    if(std::ranges::any_of(replacedVisElements(), std::mem_fn(&OOWeakRef<DataVis>::expired))) {
        auto newReplacedVisElements = replacedVisElements();
        for(int i = (int)newReplacedVisElements.size() - 1; i >= 0; i--) {
            if(newReplacedVisElements[i].expired()) {
                newReplacedVisElements.erase(newReplacedVisElements.begin() + i);
                _replacementVisElements.remove(this, PROPERTY_FIELD(replacementVisElements), i);
            }
        }
        setReplacedVisElements(std::move(newReplacedVisElements));
        OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());
    }

    // Clear the ad-hoc reference to the SceneNode to avoid the circular reference.
    _deserializationSceneNode.reset();
}

/******************************************************************************
* For backward compatibility with OVITO 3.11:
* The Pipeline class has been separated from the SceneNode base class in OVITO 3.12.
* A separate SceneNode must now be created when loading an old Pipeline object from a state file.
* This function creates and returns the SceneNode for this pipeline object during deserialization of legacy state files.
******************************************************************************/
OORef<SceneNode>& Pipeline::deserializationSceneNode()
{
    if(!_deserializationSceneNode) {
        _deserializationSceneNode = OORef<SceneNode>::create();
        _deserializationSceneNode->setPipeline(this);
    }
    return _deserializationSceneNode;
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed or changed in a newer version of OVITO.
* This is needed for file backward compatibility with OVITO 3.11.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr Pipeline::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // For backward compatibility with OVITO 3.11:
    // The 'replacedVisElements' list used to be a vector reference field in previous program versions.
    // Now it is a simple property field holding a vector of weak references to vis elements.
    if(field.definingClass == &Pipeline::OOClass() && stream.formatVersion() < 30013) {
        if(field.identifier == "replacedVisElements") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numElements;
                stream >> numElements;
                std::vector<OOWeakRef<DataVis>> elements;
                for(qint32 i = 0; i < numElements; i++) {
                    elements.push_back(stream.loadObject<DataVis>());
                }
                static_object_cast<Pipeline>(&owner)->setReplacedVisElements(std::move(elements));
                stream.closeChunk();
            };
        }
    }

    // For backward compatibility with OVITO 3.11:
    // The Pipeline class has been split from the SceneNode base class. This means we have to handle
    // the deserialization of the SceneNode fields here and then copy them over to the separate SceneNode instance.
    if(field.definingClass == &SceneNode::OOClass() && stream.formatVersion() < 30013) {
        if(field.identifier == "displayColor") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x04);
                static_object_cast<Pipeline>(&owner)->deserializationSceneNode()->_displayColor.loadFromStream(stream);
                stream.closeChunk();
            };
        }
        else if(field.identifier == "sceneNodeName" || field.identifier == "nodeName") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x04);
                static_object_cast<Pipeline>(&owner)->deserializationSceneNode()->_sceneNodeName.loadFromStream(stream);
                stream.closeChunk();
            };
        }
        else if(field.identifier == "transformationController") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                static_object_cast<Pipeline>(&owner)->deserializationSceneNode()->setTransformationController(stream.loadObject<Controller>());
                stream.closeChunk();
            };
        }
        else if(field.identifier == "lookatTargetNode") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                SceneNode* node = static_object_cast<Pipeline>(&owner)->deserializationSceneNode();
                if(OORef<Pipeline> targetPipeline = stream.loadObject<Pipeline>())
                    node->_lookatTargetNode.set(node, PROPERTY_FIELD(SceneNode::lookatTargetNode), targetPipeline->deserializationSceneNode());
                stream.closeChunk();
            };
        }
        else if(field.identifier == "hiddenInViewports") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numHiddenInViewports;
                stream >> numHiddenInViewports;
                std::vector<OOWeakRef<Viewport>> viewports;
                for(qint32 i = 0; i < numHiddenInViewports; i++) {
                    viewports.push_back(stream.loadObject<Viewport>());
                }
                static_object_cast<Pipeline>(&owner)->deserializationSceneNode()->setHiddenInViewports(std::move(viewports));
                stream.closeChunk();
            };
        }
        else if(field.identifier == "children") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numChildren;
                stream >> numChildren;
                OVITO_ASSERT(numChildren == 0);
                stream.closeChunk();
            };
        }
    }

    return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);
}

/******************************************************************************
* Returns the internal replacement for the given vis element.
* If there is no replacement, the original vis element is returned.
******************************************************************************/
DataVis* Pipeline::getReplacementVisElement(DataVis* vis) const
{
    OVITO_ASSERT(vis);
    OVITO_ASSERT(replacementVisElements().size() == replacedVisElements().size());
    auto it = std::ranges::find(replacedVisElements(), vis);
    if(it != replacedVisElements().end())
        return replacementVisElements()[std::distance(replacedVisElements().begin(), it)];
    else
        return vis;
}

/******************************************************************************
* Replaces the given visual element in this pipeline's output with an
* independent copy.
******************************************************************************/
DataVis* Pipeline::makeVisElementIndependent(DataVis* visElement)
{
    OVITO_ASSERT(visElement != nullptr);
    OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());

    // Check if the visual element is already replaced.
    if(DataVis* replacement = getReplacementVisElement(visElement); replacement != visElement)
        return replacement;

    // Clone the visual element.
    OORef<DataVis> clonedVisElement = CloneHelper::cloneSingleObject(visElement, true);
    DataVis* newVis = clonedVisElement.get();

    // Make sure the scene gets notified that the pipeline is changing if the operation is being undone.
    pushIfUndoRecording<TargetChangedUndoOperation>(this);

    // Put the copy into our mapping table, which will subsequently be applied
    // after every pipeline evaluation to replace the upstream visual element
    // with our private copy.
    auto index = replacementVisElements().indexOf(visElement);
    if(index == -1) {
        auto newList = replacedVisElements();
        newList.push_back(visElement);
        setReplacedVisElements(std::move(newList));
        _replacementVisElements.push_back(this, PROPERTY_FIELD(replacementVisElements), std::move(clonedVisElement));
    }
    else {
        _replacementVisElements.set(this, PROPERTY_FIELD(replacementVisElements), index, std::move(clonedVisElement));
    }
    OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());

    // Make sure the scene gets notified that the pipeline is changing if the operation is being redone.
    pushIfUndoRecording<TargetChangedRedoOperation>(this);

    notifyTargetChanged();

    return newVis;
}

/******************************************************************************
* Replaces all references to the given visual element in the pipeline with new compatible objects.
******************************************************************************/
void Pipeline::replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement)
{
    OVITO_ASSERT(visElement != nullptr);

    for(int index = 0; index < replacementVisElements().size(); index++) {
        if(replacementVisElements()[index] == visElement) {
            // Make sure the scene gets notified that the pipeline is changing if the operation is being undone.
            pushIfUndoRecording<TargetChangedUndoOperation>(this);

            // Adopt title of replaced visual element.
            OORef<DataVis> replacedVisElement = replacedVisElements()[index].lock();
            QString title = replacedVisElement ? replacedVisElement->objectTitle() : QString();

            _replacementVisElements.set(this, PROPERTY_FIELD(replacementVisElements), index, getReplacement(title));

            // Make sure the scene gets notified that the pipeline is changing if the operation is being redone.
            pushIfUndoRecording<TargetChangedRedoOperation>(this);

            notifyTargetChanged();
        }
    }

    // Perform the replacement operation recursively in all nodes of the pipeline.
    if(head())
        head()->replaceVisualElement(visElement, getReplacement);
}

/******************************************************************************
* Helper function that recursively finds all data objects which the given
* vis element is associated with.
******************************************************************************/
void Pipeline::collectDataObjectsForVisElement(ConstDataObjectPath& path, DataVis* vis, std::vector<ConstDataObjectPath>& dataObjectPaths) const
{
    // Check if this vis element we are looking for is among the vis elements attached to the current data object.
    for(DataVis* otherVis : path.back()->visElements()) {
        if(getReplacementVisElement(otherVis) == vis) {
            dataObjectPaths.push_back(path);
            break;
        }
    }

    // Recursively visit the sub-objects of the object.
    path.back()->visitSubObjects([&](const DataObject* subObject) {
        path.push_back(subObject);
        collectDataObjectsForVisElement(path, vis, dataObjectPaths);
        path.pop_back();
    });
}

/******************************************************************************
* Gathers a list of data objects from the given pipeline flow state (which
* should have been produced by this pipeline) that are associated with the
* given vis element. This method takes into account replacement vis elements.
******************************************************************************/
std::vector<ConstDataObjectPath> Pipeline::getDataObjectsForVisElement(const PipelineFlowState& state, DataVis* vis) const
{
    std::vector<ConstDataObjectPath> results;
    if(state) {
        ConstDataObjectPath path(1);
        for(const DataObject* obj : state.data()->objects()) {
            OVITO_ASSERT(path.size() == 1);
            path[0] = obj;
            collectDataObjectsForVisElement(path, vis, results);
        }
    }
    return results;
}

}   // End of namespace
