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
DEFINE_REFERENCE_FIELD(DataSet, renderSettings);
SET_PROPERTY_FIELD_LABEL(DataSet, viewportConfig, "Viewport Configuration");
SET_PROPERTY_FIELD_LABEL(DataSet, renderSettings, "Render Settings");

/******************************************************************************
* Constructor.
******************************************************************************/
DataSet::DataSet(ObjectCreationParams params) : RefTarget(params)
{
	if(params.createSubObjects()) {
		setViewportConfig(createDefaultViewportConfiguration(params));
		setRenderSettings(OORef<RenderSettings>::create(params));
	}
}

/******************************************************************************
* Destructor.
******************************************************************************/
DataSet::~DataSet()
{
}

/******************************************************************************
* Returns a viewport configuration that is used as template for new scenes.
******************************************************************************/
OORef<ViewportConfiguration> DataSet::createDefaultViewportConfiguration(ObjectCreationParams params)
{
	OORef<ViewportConfiguration> viewConfig = OORef<ViewportConfiguration>::create(params);

	if(!StandaloneApplication::instance() || !StandaloneApplication::instance()->cmdLineParser().isSet("noviewports")) {

		// Create a scene with animation settings.
		OORef<Scene> scene = OORef<Scene>::create(params);
		OVITO_ASSERT(scene->animationSettings());

		// Create the 4 standard viewports.
		OORef<Viewport> topView = OORef<Viewport>::create(params);
		topView->setScene(scene);
		topView->setViewType(Viewport::VIEW_TOP);

		OORef<Viewport> frontView = OORef<Viewport>::create(params);
		frontView->setScene(scene);
		frontView->setViewType(Viewport::VIEW_FRONT);

		OORef<Viewport> leftView = OORef<Viewport>::create(params);
		leftView->setScene(scene);
		leftView->setViewType(Viewport::VIEW_LEFT);

		OORef<Viewport> perspectiveView = OORef<Viewport>::create(params);
		perspectiveView->setScene(scene);
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
		// Propagate change events only from certain sources to the DataSetContainer.
		return (source == renderSettings());
	}
	return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DataSet::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(viewportConfig)) {
		disconnect(_activeViewportChangedConnection);
		if(viewportConfig())
			_activeViewportChangedConnection = connect(viewportConfig(), &ViewportConfiguration::activeViewportChanged, this, &DataSet::onActiveViewportChanged);
		Q_EMIT viewportConfigReplaced(viewportConfig());
	}
	else if(field == PROPERTY_FIELD(animationSettings)) {
		// Stop animation playback when animation settings are being replaced.
		if(AnimationSettings* oldAnimSettings = static_object_cast<AnimationSettings>(oldTarget))
			oldAnimSettings->stopAnimationPlayback();

		Q_EMIT animationSettingsReplaced(animationSettings());
	}
	else if(field == PROPERTY_FIELD(sceneRoot)) {
		setAnimationSettings(scene() ? scene()->animationSettings() : nullptr);
		Q_EMIT sceneReplaced(scene());
	}
	else if(field == PROPERTY_FIELD(renderSettings)) {
		Q_EMIT renderSettingsReplaced(renderSettings());
	}

#if 0
	// Install a signal/slot connection that updates the viewports every time the animation time has changed.
	if(field == PROPERTY_FIELD(viewportConfig) || field == PROPERTY_FIELD(animationSettings)) {
		disconnect(_updateViewportOnTimeChangeConnection);
		if(animationSettings() && viewportConfig()) {
			_updateViewportOnTimeChangeConnection = connect(animationSettings(), &AnimationSettings::timeChangeComplete, viewportConfig(), &ViewportConfiguration::updateViewports);
			viewportConfig()->updateViewports();
		}
	}
#endif

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
* This is the high-level rendering function, which invokes the renderer to 
* generate one or more output images of the scene. 
******************************************************************************/
bool DataSet::renderScene(const RenderSettings& renderSettings, const ViewportConfiguration& viewportConfiguration, FrameBuffer& frameBuffer, MainThreadOperation& operation)
{
	std::vector<std::pair<Viewport*, QRectF>> viewportLayout;
	if(renderSettings.renderAllViewports()) {
		// When rendering an entire viewport layout, determine the each viewport's destination rectangle within the output frame buffer.
		QSizeF borderSize(0,0);
		if(renderSettings.layoutSeperatorsEnabled()) {
			// Convert separator width from pixels to reduced units, which are relative to the framebuffer width/height.
			borderSize.setWidth( 1.0 / renderSettings.outputImageWidth()  * renderSettings.layoutSeperatorWidth()); 
			borderSize.setHeight(1.0 / renderSettings.outputImageHeight() * renderSettings.layoutSeperatorWidth()); 
		}
		viewportLayout = viewportConfiguration.getViewportRectangles(QRectF(0,0,1,1), borderSize);
	}
	else if(viewportConfiguration.activeViewport()) {
		// When rendering just the active viewport, create an ad-hoc layout for the single viewport.
		viewportLayout.push_back({ viewportConfiguration.activeViewport(), QRectF(0,0,1,1) });
	}

	return renderScene(renderSettings, viewportLayout, frameBuffer, operation);
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to 
* generate one or more output images of the scene. 
******************************************************************************/
bool DataSet::renderScene(const RenderSettings& renderSettings, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, FrameBuffer& frameBuffer, MainThreadOperation& operation)
{
	// Get the selected scene renderer.
	// Note: Using ref-counted pointer here, because the renderer may potentially be deleted before the current function returns. 
	OORef<SceneRenderer> renderer = renderSettings.renderer();
	if(!renderer) throw Exception(tr("No rendering engine has been selected."));

	// Create a ref-counted pointer to ourself to keep the DataSet alive even if the application 
	// is shutting down while we are still in this function.
	OORef<DataSet> self(this);

	bool notCanceled = true;
	try {

		// Resize output frame buffer.
		if(frameBuffer.size() != QSize(renderSettings.outputImageWidth(), renderSettings.outputImageHeight())) {
			frameBuffer.setSize(QSize(renderSettings.outputImageWidth(), renderSettings.outputImageHeight()));
			frameBuffer.clear();
		}

		// Don't update viewports while rendering.
		ViewportSuspender noVPUpdates(operation.userInterface());

		// Determine the size of the rendering frame buffer. It must fit the largest viewport rectangle.
		QSize largestViewportRectSize(0,0);
		for(const std::pair<Viewport*, QRectF>& rect : viewportLayout) {
			// Convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
			QRectF pixelRect(rect.second.x() * frameBuffer.width(), rect.second.y() * frameBuffer.height(), rect.second.width() * frameBuffer.width(), rect.second.height() * frameBuffer.height());
			largestViewportRectSize = largestViewportRectSize.expandedTo(pixelRect.toRect().size());
		}
		if(largestViewportRectSize.isEmpty())
			throw Exception(tr("There is no valid viewport to be rendered."));

		// Initialize the renderer.
		operation.setProgressText(tr("Initializing renderer"));
		if(renderer->startRender(renderSettings, largestViewportRectSize, visCache())) {

			VideoEncoder* videoEncoder = nullptr;
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
			std::unique_ptr<VideoEncoder> videoEncoderPtr;
			// Initialize video encoder.
			if(renderSettings.saveToFile() && renderSettings.imageInfo().isMovie()) {

				if(renderSettings.imageFilename().isEmpty())
					throw Exception(tr("Cannot save rendered images to movie file. Output filename has not been specified."));

				videoEncoderPtr = std::make_unique<VideoEncoder>();
				videoEncoder = videoEncoderPtr.get();
				videoEncoder->openFile(renderSettings.imageFilename(), renderSettings.outputImageWidth(), renderSettings.outputImageHeight(), (renderSettings.framesPerSecond() > 0) ? renderSettings.framesPerSecond() : animationSettings()->framesPerSecond());
			}
#endif

			if(renderSettings.renderingRangeType() == RenderSettings::CURRENT_FRAME) {
				// Render a single frame.
				int frameNumber = animationSettings()->currentFrame();
				operation.setProgressText(tr("Rendering frame %1").arg(frameNumber));
				notCanceled = renderFrame(frameNumber, renderSettings, *renderer, frameBuffer, viewportLayout, videoEncoder, operation);
			}
			else if(renderSettings.renderingRangeType() == RenderSettings::CUSTOM_FRAME) {
				// Render a specific frame.
				int frameNumber = renderSettings.customFrame();
				operation.setProgressText(tr("Rendering frame %1").arg(frameNumber));
				notCanceled = renderFrame(frameNumber, renderSettings, *renderer, frameBuffer, viewportLayout, videoEncoder, operation);
			}
			else if(renderSettings.renderingRangeType() == RenderSettings::ANIMATION_INTERVAL || renderSettings.renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) {
				// Render an animation interval.
				int firstFrameNumber, numberOfFrames;
				if(renderSettings.renderingRangeType() == RenderSettings::ANIMATION_INTERVAL) {
					firstFrameNumber = animationSettings()->firstFrame();
					numberOfFrames = (animationSettings()->lastFrame() - firstFrameNumber + 1);
				}
				else {
					firstFrameNumber = renderSettings.customRangeStart();
					numberOfFrames = (renderSettings.customRangeEnd() - firstFrameNumber + 1);
				}
				numberOfFrames = (numberOfFrames + renderSettings.everyNthFrame() - 1) / renderSettings.everyNthFrame();
				if(numberOfFrames < 1)
					throw Exception(tr("Invalid rendering range: Frame %1 to %2").arg(renderSettings.customRangeStart()).arg(renderSettings.customRangeEnd()));
				operation.setProgressMaximum(numberOfFrames);

				// Render frames, one by one.
				for(int frameIndex = 0; frameIndex < numberOfFrames && notCanceled && !operation.isCanceled(); frameIndex++) {
					int frameNumber = firstFrameNumber + frameIndex * renderSettings.everyNthFrame() + renderSettings.fileNumberBase();

					operation.setProgressValue(frameIndex);
					operation.setProgressText(tr("Rendering animation frame %1 of %2").arg(frameIndex+1).arg(numberOfFrames));

					MainThreadOperation frameOperation = operation.createSubTask(true);
					notCanceled = renderFrame(frameNumber, renderSettings, *renderer, frameBuffer, viewportLayout, videoEncoder, frameOperation);
					if(!notCanceled) break;

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
		if(!ExecutionContext::isInteractive())
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
bool DataSet::renderFrame(int frameNumber, const RenderSettings& settings, SceneRenderer& renderer, 
		FrameBuffer& frameBuffer, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, VideoEncoder* videoEncoder, MainThreadOperation& operation)
{
	// Determine output filename for this frame.
	QString imageFilename;
	if(settings.saveToFile() && !videoEncoder) {
		imageFilename = settings.imageFilename();
		if(imageFilename.isEmpty())
			throw Exception(tr("Cannot save rendered image to file, because no output filename has been specified."));

		// Append frame number to filename when rendering an animation.
		if(settings.renderingRangeType() != RenderSettings::CURRENT_FRAME && settings.renderingRangeType() != RenderSettings::CUSTOM_FRAME) {
			QFileInfo fileInfo(imageFilename);
			imageFilename = fileInfo.path() + QChar('/') + fileInfo.baseName() + QString("%1.").arg(frameNumber, 4, 10, QChar('0')) + fileInfo.completeSuffix();

			// Check for existing image file and skip.
			if(settings.skipExistingImages() && QFileInfo(imageFilename).isFile())
				return true;
		}
	}

	// Compute relative weights of the viewport rectangles for the progress display. 
	std::vector<int> progressWeights(viewportLayout.size());
	std::transform(viewportLayout.cbegin(), viewportLayout.cend(), progressWeights.begin(), [&](const auto& r) {
		return r.second.width() * r.second.height() * frameBuffer.width() * frameBuffer.height();
	});
	operation.beginProgressSubStepsWithWeights(std::move(progressWeights));

	AnimationTime renderTime = AnimationTime::fromFrame(frameNumber);

	// Render each viewport of the layout one after the other.
	for(const std::pair<Viewport*, QRectF>& viewportRect : viewportLayout) {
		Viewport* viewport = viewportRect.first;

		// Convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
		QRectF pixelRect(viewportRect.second.x() * frameBuffer.width(), viewportRect.second.y() * frameBuffer.height(), viewportRect.second.width() * frameBuffer.width(), viewportRect.second.height() * frameBuffer.height());
		QRect destinationRect = pixelRect.toRect();

		if(!destinationRect.isEmpty()) {

			// Set up preliminary projection.
			FloatType viewportAspectRatio = (FloatType)destinationRect.height() / (FloatType)destinationRect.width();
			ViewProjectionParameters projParams = viewport->computeProjectionParameters(renderTime, viewportAspectRatio, renderer.waitForLongOperationsEnabled());

			// Request scene bounding box.
			Box3 boundingBox = renderer.computeSceneBoundingBox(renderTime, viewport->scene(), projParams, nullptr, operation);
			if(operation.isCanceled())
				return false;

			// Determine final view projection.
			projParams = viewport->computeProjectionParameters(renderTime, viewportAspectRatio, renderer.waitForLongOperationsEnabled(), boundingBox);

			// Render one frame.
			try {
				renderer.beginFrame(renderTime, viewport->scene(), projParams, viewport, destinationRect, &frameBuffer);

				// Clear frame buffer with background color.
				ColorA clearColor = settings.generateAlphaChannel() ? ColorA(0,0,0,0) : ColorA(settings.backgroundColor());
				frameBuffer.clear(clearColor, destinationRect);

				// Render viewport "underlays".
				if(!renderer.renderOverlays(true, destinationRect, destinationRect, operation)) {
					renderer.endFrame(false, destinationRect);
					return false;
				}

				// Let the scene renderer do its work.
				if(!renderer.renderFrame(destinationRect, operation)) {
					renderer.endFrame(false, destinationRect);
					return false;
				}

				// Render viewport "overlays" on top.
				if(!renderer.renderOverlays(false, destinationRect, destinationRect, operation)) {
					renderer.endFrame(false, destinationRect);
					return false;
				}

				renderer.endFrame(true, destinationRect);
			}
			catch(...) {
				renderer.endFrame(false, destinationRect);
				throw;
			}
		}

		operation.nextProgressSubStep();
	}
	operation.endProgressSubSteps();

	// Save rendered image to disk.
	if(settings.saveToFile()) {
		if(!videoEncoder) {
			OVITO_ASSERT(!imageFilename.isEmpty());
			if(!frameBuffer.image().save(imageFilename, settings.imageInfo().format()))
				throw Exception(tr("Failed to save rendered image to output file '%1'.").arg(imageFilename));
		}
		else {
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
			videoEncoder->writeFrame(frameBuffer.image());
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
    	throw Exception(tr("Failed to open output file '%1' for writing: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectSaveStream stream(dataStream, operation);
	stream.saveObject(this);
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throw Exception(tr("Failed to write session state file '%1': %2").arg(absolutePath).arg(fileStream.errorString()));
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
    	throw Exception(tr("Failed to open file '%1' for reading: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectLoadStream stream(dataStream, operation);
	stream.setDataset(this);
	OORef<DataSet> dataSet = stream.loadObject<DataSet>();
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throw Exception(tr("Failed to load state file '%1'.").arg(absolutePath));		
	fileStream.close();
}

/******************************************************************************
* Is called whenever a different viewport becomes the currently active one.
******************************************************************************/
void DataSet::onActiveViewportChanged(Viewport* activeViewport)
{
}

}	// End of namespace
