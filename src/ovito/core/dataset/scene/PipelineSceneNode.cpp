////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/TransformingDataVis.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/UndoStackOperations.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/oo/CloneHelper.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(PipelineSceneNode);
DEFINE_REFERENCE_FIELD(PipelineSceneNode, dataProvider);
DEFINE_VECTOR_REFERENCE_FIELD(PipelineSceneNode, visElements);
DEFINE_VECTOR_REFERENCE_FIELD(PipelineSceneNode, replacedVisElements);
DEFINE_VECTOR_REFERENCE_FIELD(PipelineSceneNode, replacementVisElements);
DEFINE_REFERENCE_FIELD(PipelineSceneNode, pipelineSource);
DEFINE_PROPERTY_FIELD(PipelineSceneNode, pipelineTrajectoryCachingEnabled);
SET_PROPERTY_FIELD_LABEL(PipelineSceneNode, dataProvider, "Pipeline object");
SET_PROPERTY_FIELD_LABEL(PipelineSceneNode, pipelineTrajectoryCachingEnabled, "Precompute all trajectory frames");
SET_PROPERTY_FIELD_LABEL(PipelineSceneNode, pipelineSource, "Pipeline source");
SET_PROPERTY_FIELD_CHANGE_EVENT(PipelineSceneNode, dataProvider, ReferenceEvent::PipelineChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineSceneNode::PipelineSceneNode(ObjectCreationParams params) : SceneNode(params), 
	_pipelineCache(this, false), 
	_pipelineRenderingCache(this, true),
	_pipelineTrajectoryCachingEnabled(false)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
PipelineSceneNode::~PipelineSceneNode() // NOLINT
{
}

/******************************************************************************
* Performs a synchronous evaluation of the pipeline yielding only preliminary results.
******************************************************************************/
const PipelineFlowState& PipelineSceneNode::evaluatePipelineSynchronous(AnimationTime time, bool includeVisElements)
{	
	PipelineEvaluationRequest request(time, time.frame());
	return includeVisElements ? 
		_pipelineRenderingCache.evaluatePipelineSynchronous(request) : 
		_pipelineCache.evaluatePipelineSynchronous(request);
}

/******************************************************************************
* Performs an asynchronous evaluation of the data pipeline.
******************************************************************************/
PipelineEvaluationFuture PipelineSceneNode::evaluatePipeline(const PipelineEvaluationRequest& request)
{
	return PipelineEvaluationFuture(request, _pipelineCache.evaluatePipeline(request), this);
}

/******************************************************************************
* Performs an asynchronous evaluation of the data pipeline.
******************************************************************************/
PipelineEvaluationFuture PipelineSceneNode::evaluateRenderingPipeline(const PipelineEvaluationRequest& request)
{
	return PipelineEvaluationFuture(request, _pipelineRenderingCache.evaluatePipeline(request), this);
}

/******************************************************************************
* Invalidates the data pipeline cache of the object node.
******************************************************************************/
void PipelineSceneNode::invalidatePipelineCache(TimeInterval keepInterval, bool resetSynchronousCache)
{
	// Invalidate data caches.
	_pipelineCache.invalidate(keepInterval, resetSynchronousCache);
	_pipelineRenderingCache.invalidate(keepInterval, resetSynchronousCache);	
	
	// Also mark the cached bounding box of this scene node as invalid.
	invalidateBoundingBox();
}

/******************************************************************************
* Helper function that recursively collects all visual elements attached to a
* data object and its children and stores them in an output vector.
******************************************************************************/
void PipelineSceneNode::collectVisElements(const DataObject* dataObj, std::vector<DataVis*>& visElements)
{
	for(DataVis* vis : dataObj->visElements()) {
		if(boost::find(visElements, vis) == visElements.end())
			visElements.push_back(vis);
	}

	dataObj->visitSubObjects([&visElements](const DataObject* subObject) {
		collectVisElements(subObject, visElements);
		return false;
	});
}

/******************************************************************************
* Rebuilds the list of visual elements maintained by the scene node.
******************************************************************************/
void PipelineSceneNode::updateVisElementList(const PipelineFlowState& state)
{
	// Only gather vis elements that are present in the pipeline at the animation time currently shown in the GUI.
	std::optional<AnimationTime> time = currentAnimationTime();
	if(!time || !state.stateValidity().contains(*time))
		return;

	// Collect all visual elements from the current pipeline state.
	std::vector<DataVis*> newVisElements;
	if(state.data())
		collectVisElements(state.data(), newVisElements);

	// Perform the replacement of vis elements.
	if(!replacedVisElements().empty()) {
		for(DataVis*& vis : newVisElements) {
			DataVis* oldVis = vis;
			vis = getReplacementVisElement(vis);
			if(vis != oldVis) {
				// Make the same replacement in the output list to maintain the original ordering.
				if(auto index = _visElements.indexOf(oldVis); index >= 0)
					_visElements.set(this, PROPERTY_FIELD(visElements), index, vis);
			}
		}
	}

	// To maintain a stable ordering, first discard those elements from the old list which are not in the new list.
	for(int i = visElements().size() - 1; i >= 0; i--) {
		DataVis* vis = visElements()[i];
		if(std::find(newVisElements.begin(), newVisElements.end(), vis) == newVisElements.end()) {
			_visElements.remove(this, PROPERTY_FIELD(visElements), i);
		}
	}

	// Now add any new visual elements to the end of the list.
	for(DataVis* vis : newVisElements) {
		OVITO_CHECK_OBJECT_POINTER(vis);
		if(!visElements().contains(vis))
			_visElements.push_back(this, PROPERTY_FIELD(visElements), vis);
	}

	// Since this method was invoked after a completed pipeline evaluation, inform all vis elements that their input state has changed.
	for(DataVis* vis : visElements())
		vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
}

/******************************************************************************
* This method is called when a referenced object has changed.
******************************************************************************/
bool PipelineSceneNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(source == dataProvider()) {
		if(event.type() == ReferenceEvent::TargetChanged) {
			invalidatePipelineCache(static_cast<const TargetChangedEvent&>(event).unchangedInterval());
		}
		else if(event.type() == ReferenceEvent::TargetDeleted) {
			// Reduce memory footprint when the pipeline's data provider gets deleted.
			invalidatePipelineCache(TimeInterval::empty(), true);

			// Data provider has been deleted -> delete scene node as well.
			if(!isUndoingOrRedoing())
				deleteNode();
		}
		else if(event.type() == ReferenceEvent::PipelineChanged) {
			// Determine the new source object of the pipeline.
			updatePipelineSourceReference();
			// Forward pipeline changed events from the pipeline.
			return true;
		}
		else if(event.type() == ReferenceEvent::AnimationFramesChanged) {
			// Forward animation interval events from the pipeline.
			return true;
		}
		else if(event.type() == ReferenceEvent::PreliminaryStateAvailable) {
			// Invalidate the cache whenever the pipeline can provide a new preliminary state.
			_pipelineCache.invalidateSynchronousState();
			_pipelineRenderingCache.invalidateSynchronousState();
			// Also recompute the cached bounding box of this scene node.
			invalidateBoundingBox();
			// Inform all vis elements that their input state has changed when the pipeline reports that a new preliminary output state is available.
			for(DataVis* vis : visElements())
				vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
		}
		else if(event.type() == ReferenceEvent::TargetEnabledOrDisabled) {
			// Inform vis elements that their input state has changed if the last pipeline stage was disabled.
			// This is necessary, because we don't receive a PreliminaryStateAvailable signal from the pipeline stage in this case.
			for(DataVis* vis : visElements())
				vis->notifyDependents(ReferenceEvent::PipelineInputChanged);
		}
	}
	else if(_visElements.contains(source)) {
		if(event.type() == ReferenceEvent::TargetChanged) {

			// Recompute bounding box when a visual element changes.
			invalidateBoundingBox();

			// Invalidate the rendering pipeline cache whenever an asynchronous visual element changes.
			if(dynamic_object_cast<TransformingDataVis>(source)) {
				// Do not completely discard these cached objects, because we might be able to re-use the transformed data objects.
				_pipelineRenderingCache.invalidate();

				// Trigger a pipeline re-evaluation.
				// Note: We have to call notifyTargetChanged() here, because the visElements field is flagged as PROPERTY_FIELD_NO_CHANGE_MESSAGE.
				notifyTargetChanged(PROPERTY_FIELD(visElements));
			}
			else {
				// Trigger an immediate viewport repaint without pipeline re-evaluation.
				notifyDependents(ReferenceEvent::PreliminaryStateAvailable);
			}
		}
	}
	if(source == pipelineSource()) {
		if(event.type() == ReferenceEvent::TitleChanged && nodeName().isEmpty()) {
			// Forward this event to dependents of the pipeline.
			return true;
		}
	}
	return SceneNode::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data provider of the pipeline has been replaced.
******************************************************************************/
void PipelineSceneNode::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(dataProvider)) {
		// Reset caches when the pipeline data source is replaced.
		invalidatePipelineCache(TimeInterval::empty(), false);

		// The animation length and the title of the pipeline might have changed.
		if(!isBeingLoaded() && !isAboutToBeDeleted())
			notifyDependents(ReferenceEvent::AnimationFramesChanged);

		// Determine the new source object of the pipeline.
		updatePipelineSourceReference();
	}
	else if(field == PROPERTY_FIELD(replacedVisElements)) {
		OVITO_ASSERT(false);
	}
	else if(field == PROPERTY_FIELD(replacementVisElements)) {
		// Reset pipeline cache if a new replacement for a visual element is assigned.
		invalidatePipelineCache();
	}
	else if(field == PROPERTY_FIELD(pipelineSource)) {
		// When the source of the pipeline is being replaced, the pipeline's title changes.
		if(nodeName().isEmpty())
			notifyDependents(ReferenceEvent::TitleChanged);		
	}
	SceneNode::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void PipelineSceneNode::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
	SceneNode::saveToStream(stream, excludeRecomputableData);
	stream.beginChunk(0x01);
	// For future use...
	stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void PipelineSceneNode::loadFromStream(ObjectLoadStream& stream)
{
	SceneNode::loadFromStream(stream);
	stream.expectChunk(0x01);
	// For future use...
	stream.closeChunk();

	// Transfer the caching flag loaded from the state file to the internal cache instance.
	_pipelineRenderingCache.setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());
}

/******************************************************************************
* Rescales the times of all animation keys from the old animation interval to the new interval.
******************************************************************************/
void PipelineSceneNode::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
	SceneNode::rescaleTime(oldAnimationInterval, newAnimationInterval);
	_pipelineCache.invalidate();
	_pipelineRenderingCache.invalidate();
}

/******************************************************************************
* Returns the title of this object.
******************************************************************************/
QString PipelineSceneNode::objectTitle() const
{
	// If a user-defined name has been assigned to this pipeline, return it as the node's display title.
	if(!nodeName().isEmpty())
		return nodeName();

	// Otherwise, use the title of the pipeline's data source.
	if(pipelineSource())
		return pipelineSource()->objectTitle();

	// Fall back to default behavior.
	return SceneNode::objectTitle();
}

/******************************************************************************
* Applies a modifier by appending it to the end of the node's modification
* pipeline.
******************************************************************************/
ModifierApplication* PipelineSceneNode::applyModifier(Modifier* modifier)
{
	OVITO_ASSERT(modifier);	

	OORef<ModifierApplication> modApp = modifier->createModifierApplication();
	modApp->setModifier(modifier);
	modApp->setInput(dataProvider());
	AnimationTime time = currentAnimationTime().value_or(AnimationTime(0));
	modifier->initializeModifier(ModifierInitializationRequest(time, time.frame(), modApp));
	setDataProvider(modApp);
	return modApp;
}

/******************************************************************************
* Determines the current source of the data pipeline and updates the internal weak reference field.
******************************************************************************/
void PipelineSceneNode::updatePipelineSourceReference()
{
	if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dataProvider()))
		_pipelineSource.set(this, PROPERTY_FIELD(pipelineSource), modApp->pipelineSource());
	else
		_pipelineSource.set(this, PROPERTY_FIELD(pipelineSource), dataProvider());
}

/******************************************************************************
* Sets the data source of this node's pipeline, i.e., the object that provides the
* input data entering the pipeline.
******************************************************************************/
void PipelineSceneNode::setPipelineSource(PipelineObject* sourceObject)
{
	ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dataProvider());
	if(!modApp) {
		setDataProvider(sourceObject);
	}
	else {
		for(;;) {
			if(ModifierApplication* modApp2 = dynamic_object_cast<ModifierApplication>(modApp->input()))
				modApp = modApp2;
			else
				break;
		}
		modApp->setInput(sourceObject);
	}
	OVITO_ASSERT(ModifierApplication::OOClass().isMember(sourceObject) || this->pipelineSource() == sourceObject);
}

/******************************************************************************
* Computes the bounding box of the scene node in local coordinates.
******************************************************************************/
Box3 PipelineSceneNode::localBoundingBox(AnimationTime time, TimeInterval& validity) const
{
	const PipelineFlowState& state = const_cast<PipelineSceneNode*>(this)->evaluatePipelineSynchronous(time, true);

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
void PipelineSceneNode::getDataObjectBoundingBox(AnimationTime time, const DataObject* dataObj, const PipelineFlowState& state, TimeInterval& validity, Box3& bb, ConstDataObjectPath& dataObjectPath) const
{
	bool isOnStack = false;

	// Call all vis elements of the data object.
	for(DataVis* vis : dataObj->visElements()) {
		// Let the pipeline substitude the vis element with another one.
		vis = getReplacementVisElement(vis);
		if(vis->isEnabled()) {
			// Push the data object onto the stack.
			if(!isOnStack) {
				dataObjectPath.push_back(dataObj);
				isOnStack = true;
			}
			try {
				// Let the vis element do the rendering.
				bb.addBox(vis->boundingBox(time, dataObjectPath, this, state, validity));
			}
			catch(const Exception& ex) {
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
		return false;
	});

	// Pop the data object from the stack.
	if(isOnStack) {
		dataObjectPath.pop_back();
	}
}

/******************************************************************************
* Deletes this node from the scene.
******************************************************************************/
void PipelineSceneNode::deleteNode()
{
	// Temporary reference to the pipeline's stages.
	OORef<PipelineObject> oldDataProvider = dataProvider();

	// Throw away data source.
	// This will also clear the caches of the pipeline.
	setDataProvider(nullptr);

	// Walk along the pipeline and delete the individual modifiers/source objects (unless they are shared with another pipeline).
	// This is necessary to update any other references the scene may have to the pipeline's modifiers, 
	// e.g. the ColorLegendOverlay.
	while(oldDataProvider) {
		OORef<PipelineObject> next;
		if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(oldDataProvider.get()))
			next = modApp->input();
		// Delete the pipeline stage if it is not part of any other pipeline in the scene.
		if(oldDataProvider->pipelines(false).isEmpty())
			oldDataProvider->deleteReferenceObject();
		oldDataProvider = std::move(next);
	}

	// Discard transient references to visual elements.
	_visElements.clear(this, PROPERTY_FIELD(visElements));

	SceneNode::deleteNode();
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
******************************************************************************/
void PipelineSceneNode::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(replacementVisElements)) {
		// Reset pipeline cache if a new replacement for a visual element is assigned.
		invalidatePipelineCache();
	}
	SceneNode::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
******************************************************************************/
void PipelineSceneNode::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(replacedVisElements) && !isAboutToBeDeleted()) {
		// If an upstream vis element is being removed from the list, because the weakly referenced vis element is being deleted,
		// then also discard our corresponding replacement element managed by the pipeline.
		if(!isUndoingOrRedoing()) {
			OVITO_ASSERT(replacedVisElements().size() + 1 == replacementVisElements().size());
			_replacementVisElements.remove(this, PROPERTY_FIELD(replacementVisElements), listIndex);
		}
		// Reset pipeline cache if a replacement for a visual element is removed.
		invalidatePipelineCache();
	}
	SceneNode::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void PipelineSceneNode::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(pipelineTrajectoryCachingEnabled)) {
		_pipelineRenderingCache.setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());

		// Send target changed event to trigger a new pipeline evaluation, which is 
		// needed to start the precomputation process.
		if(pipelineTrajectoryCachingEnabled())
			notifyTargetChanged(PROPERTY_FIELD(pipelineTrajectoryCachingEnabled));
	}

	SceneNode::propertyChanged(field);
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void PipelineSceneNode::loadFromStreamComplete(ObjectLoadStream& stream)
{
	SceneNode::loadFromStreamComplete(stream);

	// Remove null entries from the replacedVisElements list due to expired weak references.
	for(int i = replacedVisElements().size() - 1; i >= 0; i--) {
		if(replacedVisElements()[i] == nullptr) {
			_replacedVisElements.remove(this, PROPERTY_FIELD(replacedVisElements), i);
		}
	}
	OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());
	OVITO_ASSERT(!isUndoRecording());
}

/******************************************************************************
* Returns the internal replacement for the given data vis element.
* If there is no replacement, the original vis element is returned.
******************************************************************************/
DataVis* PipelineSceneNode::getReplacementVisElement(DataVis* vis) const 
{
	OVITO_ASSERT(replacementVisElements().size() == replacedVisElements().size());
	OVITO_ASSERT(std::find(replacedVisElements().begin(), replacedVisElements().end(), nullptr) == replacedVisElements().end());
	OVITO_ASSERT(vis);
	int index = replacedVisElements().indexOf(vis);
	if(index >= 0)
		return replacementVisElements()[index];
	else
		return vis;
}

/******************************************************************************
* Replaces the given visual element in this pipeline's output with an
* independent copy.
******************************************************************************/
DataVis* PipelineSceneNode::makeVisElementIndependent(DataVis* visElement)
{
	OVITO_ASSERT(visElement != nullptr);
	OVITO_ASSERT(!replacedVisElements().contains(visElement));
	OVITO_ASSERT(replacedVisElements().size() == replacementVisElements().size());

	// Clone the visual element.
	OORef<DataVis> clonedVisElement = CloneHelper().cloneObject(visElement, true);
	DataVis* newVis = clonedVisElement.get();

	// Make sure the scene gets notified that the pipeline is changing if the operation is being undone.
	pushIfUndoRecording<TargetChangedUndoOperation>(this);

	// Put the copy into our mapping table, which will subsequently be applied
	// after every pipeline evaluation to replace the upstream visual element
	// with our local copy.
	int index = replacementVisElements().indexOf(visElement);
	if(index == -1) {
		_replacedVisElements.push_back(this, PROPERTY_FIELD(replacedVisElements), visElement);
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
* Helper function that recursively finds all data objects which the given 
* vis element is associated with.
******************************************************************************/
void PipelineSceneNode::collectDataObjectsForVisElement(ConstDataObjectPath& path, DataVis* vis, std::vector<ConstDataObjectPath>& dataObjectPaths) const
{
	// Check if this vis element we are looking for is among the vis elements attached to the current data object.
	for(DataVis* otherVis : path.back()->visElements()) {
		otherVis = getReplacementVisElement(otherVis);
		if(otherVis == vis) {
			dataObjectPaths.push_back(path);
			break;
		}
	}

	// Recursively visit the sub-objects of the object.
	path.back()->visitSubObjects([&](const DataObject* subObject) {
		path.push_back(subObject);
		collectDataObjectsForVisElement(path, vis, dataObjectPaths);
		path.pop_back();
		return false;
	});
}

/******************************************************************************
* Gathers a list of data objects from the given pipeline flow state (which 
* should have been produced by this pipeline) that are associated with the 
* given vis element. This method takes into account replacement vis elements 
* of this pipeline node.
******************************************************************************/
std::vector<ConstDataObjectPath> PipelineSceneNode::getDataObjectsForVisElement(const PipelineFlowState& state, DataVis* vis) const
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

}	// End of namespace
