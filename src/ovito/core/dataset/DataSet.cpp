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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/StandaloneApplication.h>
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
	#include <ovito/core/utilities/io/video/VideoEncoder.h>
#endif

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataSet);
DEFINE_REFERENCE_FIELD(DataSet, viewportConfig);
DEFINE_REFERENCE_FIELD(DataSet, animationSettings);
DEFINE_REFERENCE_FIELD(DataSet, sceneRoot);
DEFINE_REFERENCE_FIELD(DataSet, selection);
DEFINE_REFERENCE_FIELD(DataSet, renderSettings);
SET_PROPERTY_FIELD_LABEL(DataSet, viewportConfig, "Viewport Configuration");
SET_PROPERTY_FIELD_LABEL(DataSet, animationSettings, "Animation Settings");
SET_PROPERTY_FIELD_LABEL(DataSet, sceneRoot, "Scene");
SET_PROPERTY_FIELD_LABEL(DataSet, selection, "Selection");
SET_PROPERTY_FIELD_LABEL(DataSet, renderSettings, "Render Settings");

/******************************************************************************
* Constructor.
******************************************************************************/
DataSet::DataSet(ObjectCreationParams params) : RefTarget(ObjectCreationParams(this, params.flags())), _unitsManager(this)
{
	connect(&_pipelineEvaluationWatcher, &TaskWatcher::finished, this, &DataSet::pipelineEvaluationFinished);

	if(params.createSubObjects()) {
		setViewportConfig(createDefaultViewportConfiguration(ObjectCreationParams(this, params.flags())));
		setAnimationSettings(OORef<AnimationSettings>::create(ObjectCreationParams(this, params.flags())));
		setSceneRoot(OORef<Scene>::create(ObjectCreationParams(this, params.flags())));
		setSelection(OORef<SelectionSet>::create(ObjectCreationParams(this, params.flags())));
		setRenderSettings(OORef<RenderSettings>::create(ObjectCreationParams(this, params.flags())));
	}
}

/******************************************************************************
* Destructor.
******************************************************************************/
DataSet::~DataSet()
{
	// Stop pipeline evaluation, which might still be in progress.
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset();
}

/******************************************************************************
* Returns a viewport configuration that is used as template for new scenes.
******************************************************************************/
OORef<ViewportConfiguration> DataSet::createDefaultViewportConfiguration(ObjectCreationParams params)
{
	UndoSuspender noUndo(undoStack());

	OORef<ViewportConfiguration> viewConfig = OORef<ViewportConfiguration>::create(params);

	if(!StandaloneApplication::instance() || !StandaloneApplication::instance()->cmdLineParser().isSet("noviewports")) {

		// Create the 4 standard viewports.
		OORef<Viewport> topView = OORef<Viewport>::create(params);
		topView->setViewType(Viewport::VIEW_TOP);

		OORef<Viewport> frontView = OORef<Viewport>::create(params);
		frontView->setViewType(Viewport::VIEW_FRONT);

		OORef<Viewport> leftView = OORef<Viewport>::create(params);
		leftView->setViewType(Viewport::VIEW_LEFT);

		OORef<Viewport> perspectiveView = OORef<Viewport>::create(params);
		perspectiveView->setViewType(Viewport::VIEW_PERSPECTIVE);
		perspectiveView->setCameraTransformation(ViewportSettings::getSettings().coordinateSystemOrientation() * AffineTransformation::lookAlong({90, -120, 100}, {-90, 120, -100}, {0,0,1}).inverse());

		// Set up the 4-pane layout of the viewports.
		OORef<ViewportLayoutCell> rootLayoutCell = OORef<ViewportLayoutCell>::create(params);
		rootLayoutCell->setSplitDirection(ViewportLayoutCell::Horizontal);
		rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->setSplitDirection(ViewportLayoutCell::Vertical);
		rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->children()[0]->setViewport(topView);
		rootLayoutCell->children()[0]->children()[1]->setViewport(leftView);
		rootLayoutCell->children()[1]->setSplitDirection(ViewportLayoutCell::Vertical);
		rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[1]->children()[0]->setViewport(frontView);
		rootLayoutCell->children()[1]->children()[1]->setViewport(perspectiveView);
		viewConfig->setLayoutRootCell(std::move(rootLayoutCell));

		viewConfig->setActiveViewport(perspectiveView);

#ifndef Q_OS_WASM
		Viewport::ViewType maximizedViewportType = static_cast<Viewport::ViewType>(ViewportSettings::getSettings().defaultMaximizedViewportType());
		if(maximizedViewportType != Viewport::VIEW_NONE) {
			for(Viewport* vp : viewConfig->viewports()) {
				if(vp->viewType() == maximizedViewportType) {
					viewConfig->setActiveViewport(vp);
					viewConfig->setMaximizedViewport(vp);
					break;
				}
			}
			if(!viewConfig->maximizedViewport()) {
				viewConfig->setMaximizedViewport(viewConfig->activeViewport());
				if(maximizedViewportType > Viewport::VIEW_NONE && maximizedViewportType <= Viewport::VIEW_PERSPECTIVE)
					viewConfig->maximizedViewport()->setViewType(maximizedViewportType);
			}
		}
		else viewConfig->setMaximizedViewport(nullptr);
#else
		viewConfig->setMaximizedViewport(viewConfig->activeViewport());
#endif
	}

	return viewConfig;
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool DataSet::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "DataSet::referenceEvent", "Reference events may only be processed in the main thread.");

	if(event.type() == ReferenceEvent::TargetChanged) {
		if(source == scene()) {

			// If any of the scene pipelines change, the scene-ready state needs to be reset (unless it's still unfulfilled).
			if(_sceneReadyPromise.isValid() && _sceneReadyPromise.isFinished()) {
				_sceneReadyPromise.reset();
				OVITO_ASSERT(!_pipelineEvaluation.isValid());
			}

			// If any of the scene pipelines change, we should interrupt the pipeline evaluation that is currently in progress.
			// Ignore messages from visual elements, because they usually don't require a pipeline re-evaluation.
			if(_pipelineEvaluation.isValid() && dynamic_object_cast<DataVis>(event.sender()) == nullptr) {
				// Restart pipeline evaluation:
				makeSceneReadyLater(true);
			}
		}
		else if(source == animationSettings()) {
			// If the animation time changes, we should interrupt any pipeline evaluation that is currently in progress.
			if(_pipelineEvaluation.isValid() && _pipelineEvaluation.time() != animationSettings()->time()) {
				_pipelineEvaluationWatcher.reset();
				_pipelineEvaluation.reset();
				// Restart pipeline evaluation:
				makeSceneReadyLater(false);
			}
		}

		// Propagate event only from certain sources to the DataSetContainer:
		return (source == scene() || source == selection() || source == renderSettings());
	}
	else if(event.type() == ReferenceEvent::AnimationFramesChanged && source == scene() && !isBeingLoaded()) {
		// Automatically adjust scene's animation interval to length of loaded source animations.
		if(animationSettings()->autoAdjustInterval()) {
			UndoSuspender noUndo(this);
			animationSettings()->adjustAnimationInterval();
		}
	}
	return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DataSet::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(viewportConfig)) {
		Q_EMIT viewportConfigReplaced(viewportConfig());

		// Whenever viewport updates are resumed, we also resume evaluation of the scene's data pipelines.
		if(oldTarget) disconnect(static_cast<ViewportConfiguration*>(oldTarget), &ViewportConfiguration::viewportUpdateResumed, this, &DataSet::onViewportUpdatesResumed);
		if(newTarget) connect(static_cast<ViewportConfiguration*>(newTarget), &ViewportConfiguration::viewportUpdateResumed, this, &DataSet::onViewportUpdatesResumed);
	}
	else if(field == PROPERTY_FIELD(animationSettings)) {
		// Stop animation playback when animation settings are being replaced.
		if(AnimationSettings* oldAnimSettings = static_object_cast<AnimationSettings>(oldTarget))
			oldAnimSettings->stopAnimationPlayback();

		Q_EMIT animationSettingsReplaced(animationSettings());
	}
	else if(field == PROPERTY_FIELD(renderSettings)) {
		Q_EMIT renderSettingsReplaced(renderSettings());
	}
	else if(field == PROPERTY_FIELD(selection)) {
		Q_EMIT selectionSetReplaced(selection());
	}

	// Install a signal/slot connection that updates the viewports every time the animation time has changed.
	if(field == PROPERTY_FIELD(viewportConfig) || field == PROPERTY_FIELD(animationSettings)) {
		disconnect(_updateViewportOnTimeChangeConnection);
		if(animationSettings() && viewportConfig()) {
			_updateViewportOnTimeChangeConnection = connect(animationSettings(), &AnimationSettings::timeChangeComplete, viewportConfig(), &ViewportConfiguration::updateViewports);
			viewportConfig()->updateViewports();
		}
	}

	RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Returns the container to which this dataset belongs.
******************************************************************************/
DataSetContainer* DataSet::container() const
{
	OVITO_ASSERT_MSG(!_container.isNull(), "DataSet::container()", "DataSet is not in a DataSetContainer.");
	return _container.data();
}

/******************************************************************************
* Returns the abstract user interface this dataset was opened in.
******************************************************************************/
UserInterface& DataSet::userInterface() const
{
	return container()->userInterface();
}

/******************************************************************************
* Rescales the animation keys of all controllers in the scene.
******************************************************************************/
void DataSet::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
	// Iterate over all objects in the scene.
	for(RefTarget* reftarget : getAllDependencies()) {
		reftarget->rescaleTime(oldAnimationInterval, newAnimationInterval);
	}
}

/******************************************************************************
* Returns a future that is triggered once all data pipelines in the scene
* have been completely evaluated at the current animation time.
******************************************************************************/
SharedFuture<> DataSet::whenSceneReady()
{
	OVITO_CHECK_OBJECT_POINTER(scene());
	OVITO_CHECK_OBJECT_POINTER(animationSettings());
	OVITO_CHECK_OBJECT_POINTER(viewportConfig());
	OVITO_ASSERT(!viewportConfig()->isRendering());

	TimePoint time = animationSettings()->time();
	if(_sceneReadyPromise.isValid()) {
		// The promise should never be in the canceled state, because we automatically reset it when it gets canceled (see below).
		OVITO_ASSERT(!_sceneReadyPromise.isCanceled());

		// Recreate async operation object if the animation time has changed.
		if(_sceneReadyPromise.isFinished() && _sceneReadyTime != time)
			_sceneReadyPromise.reset();
		else
			_sceneReadyTime = time;
	}

	// Create a new promise to represent the process of making the scene ready.
	if(!_sceneReadyPromise.isValid()) {
		_sceneReadyPromise = Promise<>::create<Task>(true);

		/// Reset the promise to the null state as soon as it gets canceled.
		_sceneReadyPromise.finally(executor(), [this](UNUSED_CONTINUATION_FUNC_PARAM) {
			if(_sceneReadyPromise.isCanceled())
				_sceneReadyPromise.reset();
		});

		_sceneReadyTime = time;

		// This will call makeSceneReady() soon in order to evaluate all pipelines in the scene.
		makeSceneReadyLater(false);
	}

	return _sceneReadyPromise.sharedFuture();
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines in the current scene.
******************************************************************************/
void DataSet::makeSceneReady(bool forceReevaluation)
{
	// Make sure whenSceneReady() was called before.
	if(!_sceneReadyPromise.isValid()) {
		return;
	}

	// Stop when application is shutting down.
	if(_container.isNull()) {
		return;
	}	 

	// If scene is already ready, we are done.
	if(_sceneReadyPromise.isFinished() && _sceneReadyTime == animationSettings()->time()) {
		return;
	}

	// Is there already a pipeline evaluation in progress?
	if(_pipelineEvaluation.isValid()) {
		// Keep waiting for the current pipeline evaluation to finish unless we are at the different animation time now.
		// Or unless the pipeline has been deleted from the scene in the meantime.
		if(!forceReevaluation && _pipelineEvaluation.time() == animationSettings()->time() && _pipelineEvaluation.pipeline() && _pipelineEvaluation.pipeline()->isChildOf(scene())) {
			return;
		}
	}

	// If viewport updates are suspended, we simply wait until they get resumed.
	if(viewportConfig()->isSuspended())
		return;

	// Request results from all data pipelines in the scene.
	// If at least one of them is not immediately available, we'll have to
	// wait until its evaulation completes.
	PipelineEvaluationFuture oldEvaluation = std::move(_pipelineEvaluation);
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset(animationSettings()->time());
	_sceneReadyTime = animationSettings()->time();
	PipelineEvaluationRequest request(animationSettings()->time());

	scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
		// Request visual elements too.
		_pipelineEvaluation = pipeline->evaluateRenderingPipeline(request);
		if(!_pipelineEvaluation.isFinished()) {
			// Wait for this state to become available and return a pending future.
			return false;
		}
		else if(!_pipelineEvaluation.isCanceled()) {
			try { _pipelineEvaluation.results(); }
			catch(...) {
				qWarning() << "DataSet::makeSceneReady(): An exception was thrown in a data pipeline. This should never happen.";
				OVITO_ASSERT(false);
			}
		}
		_pipelineEvaluation.reset(animationSettings()->time());
		return true;
	});

	oldEvaluation.reset();

	// If all pipelines are already complete, we are done.
	if(!_pipelineEvaluation.isValid()) {
		// Set the promise to the fulfilled state.
		_sceneReadyPromise.setFinished();
	}
	else {
		_pipelineEvaluationWatcher.watch(_pipelineEvaluation.task());
	}
}

/******************************************************************************
* Is called whenver viewport updates are resumed.
******************************************************************************/
void DataSet::onViewportUpdatesResumed()
{
	makeSceneReadyLater(true);
}

/******************************************************************************
* Is called when the pipeline evaluation of a scene node has finished.
******************************************************************************/
void DataSet::pipelineEvaluationFinished()
{
	OVITO_ASSERT(_pipelineEvaluation.isValid());
	OVITO_ASSERT(_pipelineEvaluation.pipeline());
	OVITO_ASSERT(_pipelineEvaluation.isFinished());

	// Query results of the pipeline evaluation to see if an exception has been thrown.
	if(_sceneReadyPromise.isValid() && !_pipelineEvaluation.isCanceled()) {
		try {
			_pipelineEvaluation.results();
		}
		catch(...) {
			qWarning() << "DataSet::pipelineEvaluationFinished(): An exception was thrown in a data pipeline. This should never happen.";
			OVITO_ASSERT(false);
		}
	}

	_pipelineEvaluation.reset();
	_pipelineEvaluationWatcher.reset();

	// One of the pipelines in the scene became ready.
	// Check if there are more pending pipelines in the scene.
	makeSceneReady(false);
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to 
* generate one or more output images of the scene. 
******************************************************************************/
bool DataSet::renderScene(RenderSettings* renderSettings, ViewportConfiguration* viewportConfiguration, FrameBuffer* frameBuffer, MainThreadOperation& operation)
{
	OVITO_CHECK_OBJECT_POINTER(renderSettings);
	OVITO_CHECK_OBJECT_POINTER(viewportConfiguration);

	std::vector<std::pair<Viewport*, QRectF>> viewportLayout;
	if(renderSettings->renderAllViewports()) {
		// When rendering an entire viewport layout, determine the each viewport's destination rectangle within the output frame buffer.
		QSizeF borderSize(0,0);
		if(renderSettings->layoutSeperatorsEnabled()) {
			// Convert separator width from pixels to reduced units, which are relative to the framebuffer width/height.
			borderSize.setWidth( 1.0 / renderSettings->outputImageWidth()  * renderSettings->layoutSeperatorWidth()); 
			borderSize.setHeight(1.0 / renderSettings->outputImageHeight() * renderSettings->layoutSeperatorWidth()); 
		}
		viewportLayout = viewportConfiguration->getViewportRectangles(QRectF(0,0,1,1), borderSize);
	}
	else if(viewportConfiguration->activeViewport()) {
		// When rendering just the active viewport, create an ad-hoc layout for the single viewport.
		viewportLayout.push_back({ viewportConfiguration->activeViewport(), QRectF(0,0,1,1) });
	}

	return renderScene(renderSettings, viewportLayout, frameBuffer, operation);
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to 
* generate one or more output images of the scene. 
******************************************************************************/
bool DataSet::renderScene(RenderSettings* renderSettings, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, FrameBuffer* frameBuffer, MainThreadOperation& operation)
{
	OVITO_CHECK_OBJECT_POINTER(renderSettings);
	OVITO_ASSERT(frameBuffer);

	// Get the selected scene renderer.
	// Note: Using ref-counted pointer here, because the renderer may potentially be deleted before the current function returns. 
	OORef<SceneRenderer> renderer = renderSettings->renderer();
	if(!renderer) throwException(tr("No rendering engine has been selected."));

	// Create a ref-counted pointer to ourself to keep the DataSet alive even if the application 
	// is shutting down while we are still in this function.
	OORef<DataSet> self(this);

	bool notCanceled = true;
	try {

		// Resize output frame buffer.
		if(frameBuffer->size() != QSize(renderSettings->outputImageWidth(), renderSettings->outputImageHeight())) {
			frameBuffer->setSize(QSize(renderSettings->outputImageWidth(), renderSettings->outputImageHeight()));
			frameBuffer->clear();
		}

		// Don't update viewports while rendering.
		ViewportSuspender noVPUpdates(this);

		// Determine the size of the rendering frame buffer. It must fit the largest viewport rectangle.
		QSize largestViewportRectSize(0,0);
		for(const std::pair<Viewport*, QRectF>& rect : viewportLayout) {
			// Convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
			QRectF pixelRect(rect.second.x() * frameBuffer->width(), rect.second.y() * frameBuffer->height(), rect.second.width() * frameBuffer->width(), rect.second.height() * frameBuffer->height());
			largestViewportRectSize = largestViewportRectSize.expandedTo(pixelRect.toRect().size());
		}
		if(largestViewportRectSize.isEmpty())
			throwException(tr("There is no valid viewport to be rendered."));

		// Initialize the renderer.
		operation.setProgressText(tr("Initializing renderer"));
		if(renderer->startRender(this, renderSettings, largestViewportRectSize)) {

			VideoEncoder* videoEncoder = nullptr;
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
			std::unique_ptr<VideoEncoder> videoEncoderPtr;
			// Initialize video encoder.
			if(renderSettings->saveToFile() && renderSettings->imageInfo().isMovie()) {

				if(renderSettings->imageFilename().isEmpty())
					throwException(tr("Cannot save rendered images to movie file. Output filename has not been specified."));

				videoEncoderPtr = std::make_unique<VideoEncoder>();
				videoEncoder = videoEncoderPtr.get();
				int ticksPerFrame = std::max(1, (renderSettings->framesPerSecond() > 0) ? (TICKS_PER_SECOND / renderSettings->framesPerSecond()) : animationSettings()->ticksPerFrame());
				videoEncoder->openFile(renderSettings->imageFilename(), renderSettings->outputImageWidth(), renderSettings->outputImageHeight(), ticksPerFrame);
			}
#endif

			if(renderSettings->renderingRangeType() == RenderSettings::CURRENT_FRAME) {
				// Render a single frame.
				TimePoint renderTime = animationSettings()->time();
				int frameNumber = animationSettings()->timeToFrame(renderTime);
				operation.setProgressText(tr("Rendering frame %1").arg(frameNumber));
				notCanceled = renderFrame(renderTime, frameNumber, renderSettings, renderer, frameBuffer, viewportLayout, videoEncoder, operation);
			}
			else if(renderSettings->renderingRangeType() == RenderSettings::CUSTOM_FRAME) {
				// Render a specific frame.
				TimePoint renderTime = animationSettings()->frameToTime(renderSettings->customFrame());
				operation.setProgressText(tr("Rendering frame %1").arg(renderSettings->customFrame()));
				notCanceled = renderFrame(renderTime, renderSettings->customFrame(), renderSettings, renderer, frameBuffer, viewportLayout, videoEncoder, operation);
			}
			else if(renderSettings->renderingRangeType() == RenderSettings::ANIMATION_INTERVAL || renderSettings->renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) {
				// Render an animation interval.
				TimePoint renderTime;
				int firstFrameNumber, numberOfFrames;
				if(renderSettings->renderingRangeType() == RenderSettings::ANIMATION_INTERVAL) {
					renderTime = animationSettings()->animationInterval().start();
					firstFrameNumber = animationSettings()->timeToFrame(animationSettings()->animationInterval().start());
					numberOfFrames = (animationSettings()->timeToFrame(animationSettings()->animationInterval().end()) - firstFrameNumber + 1);
				}
				else {
					firstFrameNumber = renderSettings->customRangeStart();
					renderTime = animationSettings()->frameToTime(firstFrameNumber);
					numberOfFrames = (renderSettings->customRangeEnd() - firstFrameNumber + 1);
				}
				numberOfFrames = (numberOfFrames + renderSettings->everyNthFrame() - 1) / renderSettings->everyNthFrame();
				if(numberOfFrames < 1)
					throwException(tr("Invalid rendering range: Frame %1 to %2").arg(renderSettings->customRangeStart()).arg(renderSettings->customRangeEnd()));
				operation.setProgressMaximum(numberOfFrames);

				// Render frames, one by one.
				for(int frameIndex = 0; frameIndex < numberOfFrames && notCanceled && !operation.isCanceled(); frameIndex++) {
					int frameNumber = firstFrameNumber + frameIndex * renderSettings->everyNthFrame() + renderSettings->fileNumberBase();

					operation.setProgressValue(frameIndex);
					operation.setProgressText(tr("Rendering animation frame %1 of %2").arg(frameIndex+1).arg(numberOfFrames));

					MainThreadOperation frameOperation = operation.createSubTask(true);
					notCanceled = renderFrame(renderTime, frameNumber, renderSettings, renderer, frameBuffer, viewportLayout, videoEncoder, frameOperation);
					if(!notCanceled) break;

					// Go to next animation frame.
					renderTime += animationSettings()->ticksPerFrame() * renderSettings->everyNthFrame();

					// Periodically free visual element resources during animation rendering to avoid clogging the memory.
					visCache().discardUnusedObjects();
				}
			}

#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
			// Finalize movie file.
			if(videoEncoder)
				videoEncoder->closeFile();
#endif
		}

		// Shutdown renderer.
		renderer->endRender();

		// Free visual element resources to avoid clogging the memory in cases where render() gets called repeatedly from a script.
		if(ExecutionContext::current() == ExecutionContext::Scripting)
			visCache().discardUnusedObjects();
	}
	catch(Exception& ex) {
		// Shutdown renderer.
		renderer->endRender();
		// Provide a context for this error.
		if(ex.context() == nullptr) ex.setContext(this);
		throw;
	}

	return notCanceled;
}

/******************************************************************************
* Renders a single frame and saves the output file.
******************************************************************************/
bool DataSet::renderFrame(TimePoint renderTime, int frameNumber, RenderSettings* settings, SceneRenderer* renderer, 
		FrameBuffer* frameBuffer, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, VideoEncoder* videoEncoder, MainThreadOperation& operation)
{
	// Determine output filename for this frame.
	QString imageFilename;
	if(settings->saveToFile() && !videoEncoder) {
		imageFilename = settings->imageFilename();
		if(imageFilename.isEmpty())
			throwException(tr("Cannot save rendered image to file, because no output filename has been specified."));

		// Append frame number to filename when rendering an animation.
		if(settings->renderingRangeType() != RenderSettings::CURRENT_FRAME && settings->renderingRangeType() != RenderSettings::CUSTOM_FRAME) {
			QFileInfo fileInfo(imageFilename);
			imageFilename = fileInfo.path() + QChar('/') + fileInfo.baseName() + QString("%1.").arg(frameNumber, 4, 10, QChar('0')) + fileInfo.completeSuffix();

			// Check for existing image file and skip.
			if(settings->skipExistingImages() && QFileInfo(imageFilename).isFile())
				return true;
		}
	}

	// Compute relative weights of the viewport rectangles for the progress display. 
	std::vector<int> progressWeights(viewportLayout.size());
	std::transform(viewportLayout.cbegin(), viewportLayout.cend(), progressWeights.begin(), [frameBuffer](const auto& r) {
		return r.second.width() * r.second.height() * frameBuffer->width() * frameBuffer->height();
	});
	operation.beginProgressSubStepsWithWeights(std::move(progressWeights));

	// Render each viewport of the layout one after the other.
	for(const std::pair<Viewport*, QRectF>& viewportRect : viewportLayout) {
		Viewport* viewport = viewportRect.first;

		// Convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
		QRectF pixelRect(viewportRect.second.x() * frameBuffer->width(), viewportRect.second.y() * frameBuffer->height(), viewportRect.second.width() * frameBuffer->width(), viewportRect.second.height() * frameBuffer->height());
		QRect destinationRect = pixelRect.toRect();

		if(!destinationRect.isEmpty()) {

			// Set up preliminary projection.
			FloatType viewportAspectRatio = (FloatType)destinationRect.height() / (FloatType)destinationRect.width();
			ViewProjectionParameters projParams = viewport->computeProjectionParameters(renderTime, viewportAspectRatio, renderer->waitForLongOperationsEnabled());

			// Request scene bounding box.
			Box3 boundingBox = renderer->computeSceneBoundingBox(renderTime, projParams, nullptr, operation);
			if(operation.isCanceled())
				return false;

			// Determine final view projection.
			projParams = viewport->computeProjectionParameters(renderTime, viewportAspectRatio, renderer->waitForLongOperationsEnabled(), boundingBox);

			// Render one frame.
			try {
				renderer->beginFrame(renderTime, projParams, viewport, destinationRect, frameBuffer);

				// Clear frame buffer with background color.
				ColorA clearColor = settings->generateAlphaChannel() ? ColorA(0,0,0,0) : ColorA(settings->backgroundColor());
				frameBuffer->clear(clearColor, destinationRect);

				// Render viewport "underlays".
				if(!renderer->renderOverlays(true, destinationRect, destinationRect, operation)) {
					renderer->endFrame(false, destinationRect);
					return false;
				}

				// Let the scene renderer do its work.
				if(!renderer->renderFrame(destinationRect, operation)) {
					renderer->endFrame(false, destinationRect);
					return false;
				}

				// Render viewport "overlays" on top.
				if(!renderer->renderOverlays(false, destinationRect, destinationRect, operation)) {
					renderer->endFrame(false, destinationRect);
					return false;
				}

				renderer->endFrame(true, destinationRect);
			}
			catch(...) {
				renderer->endFrame(false, destinationRect);
				throw;
			}
		}

		operation.nextProgressSubStep();
	}
	operation.endProgressSubSteps();

	// Save rendered image to disk.
	if(settings->saveToFile()) {
		if(!videoEncoder) {
			OVITO_ASSERT(!imageFilename.isEmpty());
			if(!frameBuffer->image().save(imageFilename, settings->imageInfo().format()))
				throwException(tr("Failed to save rendered image to output file '%1'.").arg(imageFilename));
		}
		else {
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
			videoEncoder->writeFrame(frameBuffer->image());
#endif
		}
	}

	return !operation.isCanceled();
}

/******************************************************************************
* Saves the dataset to a session state file.
******************************************************************************/
void DataSet::saveToFile(const QString& filePath, MainThreadOperation operation) const
{
	// Make path absolute.
	QString absolutePath = QFileInfo(filePath).absoluteFilePath();

	QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::WriteOnly))
    	throwException(tr("Failed to open output file '%1' for writing: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectSaveStream stream(dataStream, operation);
	stream.saveObject(this);
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throwException(tr("Failed to write session state file '%1': %2").arg(absolutePath).arg(fileStream.errorString()));
	fileStream.close();
}

/******************************************************************************
* Loads the dataset's contents from a session state file.
******************************************************************************/
void DataSet::loadFromFile(const QString& filePath, MainThreadOperation operation)
{
	// Make path absolute.
	QString absolutePath = QFileInfo(filePath).absoluteFilePath();

	QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::ReadOnly))
    	throwException(tr("Failed to open file '%1' for reading: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectLoadStream stream(dataStream, operation);
	stream.setDataset(this);
	OORef<DataSet> dataSet = stream.loadObject<DataSet>();
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throwException(tr("Failed to load state file '%1'.").arg(absolutePath));		
	fileStream.close();
}

}	// End of namespace
