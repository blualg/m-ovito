////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/FrameGraphBuilder.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/viewport/ViewportSuspender.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include "RenderSettings.h"

#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    #include <ovito/core/utilities/io/video/VideoEncoder.h>
#endif

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RenderSettings);
DEFINE_PROPERTY_FIELD(RenderSettings, imageInfo);
DEFINE_VIRTUAL_PROPERTY_FIELD(RenderSettings, imageFilename, setImageFilename);
DEFINE_REFERENCE_FIELD(RenderSettings, renderer);
DEFINE_REFERENCE_FIELD(RenderSettings, backgroundColorController);
DEFINE_PROPERTY_FIELD(RenderSettings, outputImageWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, outputImageHeight);
DEFINE_PROPERTY_FIELD(RenderSettings, generateAlphaChannel);
DEFINE_PROPERTY_FIELD(RenderSettings, saveToFile);
DEFINE_PROPERTY_FIELD(RenderSettings, skipExistingImages);
DEFINE_PROPERTY_FIELD(RenderSettings, renderingRangeType);
DEFINE_PROPERTY_FIELD(RenderSettings, customRangeStart);
DEFINE_PROPERTY_FIELD(RenderSettings, customRangeEnd);
DEFINE_PROPERTY_FIELD(RenderSettings, customFrame);
DEFINE_PROPERTY_FIELD(RenderSettings, everyNthFrame);
DEFINE_PROPERTY_FIELD(RenderSettings, fileNumberBase);
DEFINE_PROPERTY_FIELD(RenderSettings, framesPerSecond);
DEFINE_PROPERTY_FIELD(RenderSettings, renderAllViewports);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorsEnabled);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeparatorColor);
SET_PROPERTY_FIELD_LABEL(RenderSettings, imageInfo, "Image info");
SET_PROPERTY_FIELD_LABEL(RenderSettings, imageFilename, "Image output path");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderer, "Renderer");
SET_PROPERTY_FIELD_LABEL(RenderSettings, backgroundColorController, "Background color");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outputImageWidth, "Width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, outputImageHeight, "Height");
SET_PROPERTY_FIELD_LABEL(RenderSettings, generateAlphaChannel, "Transparent background");
SET_PROPERTY_FIELD_LABEL(RenderSettings, saveToFile, "Save to file");
SET_PROPERTY_FIELD_LABEL(RenderSettings, skipExistingImages, "Skip existing animation images");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderingRangeType, "Rendering range");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customRangeStart, "Range start");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customRangeEnd, "Range end");
SET_PROPERTY_FIELD_LABEL(RenderSettings, customFrame, "Frame");
SET_PROPERTY_FIELD_LABEL(RenderSettings, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(RenderSettings, fileNumberBase, "File number base");
SET_PROPERTY_FIELD_LABEL(RenderSettings, framesPerSecond, "Frames per second");
SET_PROPERTY_FIELD_LABEL(RenderSettings, renderAllViewports, "Render all viewports");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorsEnabled, "Layout separators");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorWidth, "Separator width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeparatorColor, "Separator color");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageWidth, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageHeight, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, everyNthFrame, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, framesPerSecond, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, layoutSeparatorWidth, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorsEnabled, "layoutSeperatorsEnabled"); // For backward compatibility with OVITO 3.10.6
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorWidth, "layoutSeperatorWidth"); // For backward compatibility with OVITO 3.10.6
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(RenderSettings, layoutSeparatorColor, "layoutSeperatorColor"); // For backward compatibility with OVITO 3.10.6

/******************************************************************************
* Constructor.
******************************************************************************/
void RenderSettings::initializeObject(ObjectInitializationFlags flags)
{
    RefTarget::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Set default background color.
        setBackgroundColorController(ControllerManager::createColorController());
        setBackgroundColor(Color(1,1,1));

        // Create an instance of the default renderer class.
        // Use the OpenGL renderer as the default implementation.
        if(OvitoClassPtr rendererClass = PluginManager::instance().findClass("OpenGLRenderer", "OpenGLRenderer")) {
            setRenderer(static_object_cast<SceneRenderer>(rendererClass->createInstance()));
        }
    }
}

/******************************************************************************
* Sets the output filename of the rendered image.
******************************************************************************/
void RenderSettings::setImageFilename(const QString& filename)
{
    if(filename != imageFilename()) {
        ImageInfo newInfo = imageInfo();
        newInfo.setFilename(filename);
        setImageInfo(newInfo);
    }
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to
* generate one or more output images of the scene.
******************************************************************************/
void RenderSettings::render(const ViewportConfiguration& viewportConfiguration, const std::shared_ptr<FrameBuffer>& outputFrameBuffer)
{
    std::vector<std::pair<Viewport*, QRectF>> viewportLayout;
    if(renderAllViewports()) {
        // When rendering an entire viewport layout, determine the each viewport's target rectangle within the output framebuffer.
        QSizeF borderSize(0,0);
        if(layoutSeparatorsEnabled()) {
            // Convert separator width from pixels to reduced units, which are relative to the framebuffer width/height.
            borderSize.setWidth( 1.0 / outputImageWidth()  * layoutSeparatorWidth());
            borderSize.setHeight(1.0 / outputImageHeight() * layoutSeparatorWidth());
        }
        viewportLayout = viewportConfiguration.getViewportRectangles(QRectF(0,0,1,1), borderSize);
    }
    else if(viewportConfiguration.activeViewport()) {
        // When rendering just the active viewport, create an ad-hoc layout for the single viewport.
        viewportLayout.push_back({ viewportConfiguration.activeViewport(), QRectF(0,0,1,1) });
    }

    // Get the active animation settings.
    AnimationSettings* animationSettings = nullptr;
    if(Viewport* vp = viewportConfiguration.activeViewport()) {
        if(vp->scene())
            animationSettings = vp->scene()->animationSettings();
    }

    render(viewportLayout, animationSettings, outputFrameBuffer);
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to
* generate one or more output images of the scene.
******************************************************************************/
void RenderSettings::render(const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, AnimationSettings* animationSettings, const std::shared_ptr<FrameBuffer>& outputFrameBuffer)
{
    // Get the selected scene renderer.
    // Note: Using ref-counted pointer here, because the renderer may potentially be deleted before the current function returns.
    OORef<SceneRenderer> renderer = this->renderer();
    if(!renderer)
        throw Exception(tr("No rendering backend has been selected."));

    // Create a ref-counted pointer to ourself to keep the RenderSettings alive even if the application
    // is shutting down while we are still in this function.
    OORef<RenderSettings> self(this);

    // Resize output frame buffer.
    if(outputFrameBuffer->size() != QSize(outputImageWidth(), outputImageHeight())) {
        outputFrameBuffer->setSize(QSize(outputImageWidth(), outputImageHeight()));
        outputFrameBuffer->clear();
    }

    // Don't render interactive viewports while rendering an offscreen image.
    ViewportSuspender noVPUpdates;

    // Determine the range of frames to be rendered.
    int numberOfFrames = 1;
    int firstFrameNumber = 0;
    if(renderingRangeType() == RenderSettings::CURRENT_FRAME) {
        // Render a single frame.
        firstFrameNumber = animationSettings ? animationSettings->currentFrame() : 0;
    }
    else if(renderingRangeType() == RenderSettings::CUSTOM_FRAME) {
        // Render a specific frame.
        firstFrameNumber = customFrame();
    }
    else if(renderingRangeType() == RenderSettings::ANIMATION_INTERVAL || renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) {
        // Render an animation interval.
        if(renderingRangeType() == RenderSettings::ANIMATION_INTERVAL) {
            firstFrameNumber = animationSettings ? animationSettings->firstFrame() : 0;
            numberOfFrames = animationSettings ? (animationSettings->lastFrame() - firstFrameNumber + 1) : 0;
        }
        else {
            firstFrameNumber = customRangeStart();
            numberOfFrames = (customRangeEnd() - firstFrameNumber + 1);
        }
        numberOfFrames = (numberOfFrames + everyNthFrame() - 1) / everyNthFrame();
        if(numberOfFrames < 1)
            throw Exception(tr("Invalid rendering range: frame %1 to %2").arg(customRangeStart()).arg(customRangeEnd()));
    }
    else {
        throw Exception(tr("Invalid rendering range type: %1").arg(renderingRangeType()));
    }

    // Initialize the rendering job.
    OORef<RenderingJob> renderingJob = renderer->createOffscreenRenderingJob();
    this_task::throwIfCanceled();

    // Per viewport data.
    struct ViewportRenderingData {
        OORef<Viewport> viewport;
        OORef<AbstractRenderingFrameBuffer> renderingFrameBuffer;
        RendererResourceCache::ResourceFrame inactiveCacheFrame;
    };

    // Create the rendering frame buffers, one for each viewport to be rendered.
    std::vector<ViewportRenderingData> viewportRenderingData;
    viewportRenderingData.reserve(viewportLayout.size());
    std::vector<int> viewportProgressWeights;
    viewportProgressWeights.reserve(viewportLayout.size());
    for(const auto& r : viewportLayout) {
        // Compute the rectangular area covered by the viewport in the output frame buffer.
        // For this, convert viewport layout rect from relative coordinates to frame buffer pixel coordinates and round to nearest integers.
        QRectF pixelRect(r.second.x() * outputFrameBuffer->width(), r.second.y() * outputFrameBuffer->height(), r.second.width() * outputFrameBuffer->width(), r.second.height() * outputFrameBuffer->height());
        QRect destinationRect = pixelRect.toRect();
        if(destinationRect.isEmpty())
            continue;
        ViewportRenderingData& vpData = viewportRenderingData.emplace_back();
        // Request a rendering frame buffer of the right size from the rendering job.
        vpData.renderingFrameBuffer = renderingJob->createOffscreenFrameBuffer(destinationRect, outputFrameBuffer);
        vpData.viewport = r.first;
        // When rendering multiple viewports, compute relative weights of the viewport rectangles for the progress display.
        viewportProgressWeights.push_back(r.second.width() * r.second.height() * outputFrameBuffer->width() * outputFrameBuffer->height());
    }

    VideoEncoder* videoEncoder = nullptr;
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    std::unique_ptr<VideoEncoder> videoEncoderPtr;
    // Initialize video encoder.
    if(saveToFile() && imageInfo().isMovie()) {
        if(imageFilename().isEmpty())
            throw Exception(tr("Cannot save rendered images to movie file. Output filename has not been specified."));

        videoEncoderPtr = std::make_unique<VideoEncoder>();
        videoEncoder = videoEncoderPtr.get();
        float fps = framesPerSecond();
        if(fps <= 0)
            fps = animationSettings ? animationSettings->framesPerSecond() : 1.0f;
        videoEncoder->openFile(imageFilename(), outputImageWidth(), outputImageHeight(), fps);
    }
#endif

    // The visualization data cache used for building the frame graph.
    std::shared_ptr<RendererResourceCache> visCache = ExecutionContext::current().ui().datasetContainer().visCache();

    Future<void> renderFuture;
    QString lastOutputFilename;
    auto finishRenderingAndSaveToFile = [&](bool frameCompleted) {
        // Before rendering the next frame, wait for the previous one to complete.
        if(renderFuture)
            renderFuture.waitForFinished();
        renderFuture.reset();

        // Write rendered image or video frame to disk.
        if(frameCompleted && saveToFile()) {
            if(!videoEncoder) {
                OVITO_ASSERT(!lastOutputFilename.isEmpty());

                // The QImage.save() function requires a Qt application object in order to load the Qt file format plugins.
                Application::instance()->createQtApplication(false);

                // Use the QImage.save() function to save the rendered image to disk.
                if(!outputFrameBuffer->image().save(lastOutputFilename, imageInfo().format()))
                    throw Exception(tr("Failed to save rendered image to output file '%1'.").arg(lastOutputFilename));
            }
            else {
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
                videoEncoder->writeFrame(outputFrameBuffer->image());
#endif
            }
        }
    };

    // Render frames one by one.
    for(int frameIndex = 0; frameIndex < numberOfFrames && !this_task::isCanceled(); frameIndex++) {
        int frameNumber = firstFrameNumber + frameIndex * everyNthFrame() + fileNumberBase();
        AnimationTime renderTime = AnimationTime::fromFrame(frameNumber);

        if(numberOfFrames > 1) {
            this_task::setProgressMaximum(numberOfFrames, false);
            this_task::setProgressValue(frameIndex);
            this_task::setProgressText(tr("Rendering animation (frame %1 of %2)").arg(frameIndex+1).arg(numberOfFrames));
        }

        // Create a sub-task to display a second progress bar in the UI.
        MainThreadOperation frameTask;

        if(numberOfFrames == 1)
            this_task::setProgressText(tr("Rendering frame %1").arg(frameNumber));

        // Determine output filename for this frame.
        QString outputFilename;
        if(saveToFile() && !videoEncoder) {
            outputFilename = imageFilename();
            if(outputFilename.isEmpty())
                throw Exception(tr("Cannot save rendered image to file, because no output filename has been specified."));

            // Append frame number to filename when rendering an animation.
            if(renderingRangeType() != RenderSettings::CURRENT_FRAME && renderingRangeType() != RenderSettings::CUSTOM_FRAME) {
                QFileInfo fileInfo(outputFilename);
                outputFilename = fileInfo.path() + QChar('/') + fileInfo.baseName() + QString("%1.").arg(frameNumber, 4, 10, QChar('0')) + fileInfo.completeSuffix();

                // Check for existing image file and skip frame.
                if(skipExistingImages() && QFileInfo(outputFilename).isFile())
                    continue;
            }
        }

        // Subdivide progress range into sub-steps for each viewport when rendering a multi-viewport layout.
        this_task::beginProgressSubStepsWithWeights(viewportProgressWeights);

        // Render each viewport of the layout one after the other.
        for(ViewportRenderingData& vpData : viewportRenderingData) {

            // Set up preliminary projection.
            FloatType viewportAspectRatio = (FloatType)vpData.renderingFrameBuffer->outputViewportRect().height() / vpData.renderingFrameBuffer->outputViewportRect().width();
            ViewProjectionParameters projParams = vpData.viewport->computeProjectionParameters(renderTime, viewportAspectRatio);
            this_task::throwIfCanceled();

            // Create a new frame graph.
            OORef<FrameGraph> frameGraph = OORef<FrameGraph>::create(
                visCache->acquireResourceFrame(),
                renderTime,
                projParams,
                vpData.renderingFrameBuffer->outputViewportRect().size(),
                false,
                false,
                stopOnPipelineError(),
                renderingJob->preferredImageFormat(),
                renderingJob->multisamplingLevel());

            // Set background color.
            frameGraph->setClearColor(generateAlphaChannel() ? ColorA(0,0,0,0) : ColorA(backgroundColorAt(renderTime)));

            // Target rectangles for overlay/underlay rendering.
            const QRect& logicalOverlayRect = vpData.renderingFrameBuffer->outputViewportRect();
            const QRect& physicalOverlayRect = vpData.renderingFrameBuffer->renderingViewportRect();

            // Let the FrameGraphBuilder class do the heavy lifting and generate the frame graph for the current scene.
            frameGraph = FrameGraphBuilder::build(std::move(frameGraph),
                vpData.viewport->scene(), vpData.viewport,
                logicalOverlayRect, physicalOverlayRect, projParams).result();

            // Let the scene renderer implementation post-process the frame graph.
            renderingJob->postprocessFrameGraph(*frameGraph);

            // Compute final projection based on the now known bounding box.
            frameGraph->setProjectionParams(vpData.viewport->computeProjectionParameters(renderTime, viewportAspectRatio, frameGraph->sceneBoundingBox()));
            this_task::throwIfCanceled();

            // Get the cache frame back from the frame graph to keep resources alive until we start the next frame.
            vpData.inactiveCacheFrame = frameGraph->takeCacheFrame();

            // Before rendering the next frame, wait for the previous one to complete.
            finishRenderingAndSaveToFile(&vpData == &viewportRenderingData.front() && frameIndex != 0);

            // Pass the frame graph to the scene renderer to produce the rendering in the framebuffer.
            outputFrameBuffer->discardChanges();
            renderFuture = renderingJob->renderFrame(frameGraph, vpData.renderingFrameBuffer);

            this_task::nextProgressSubStep();
        }
        this_task::endProgressSubSteps();

        lastOutputFilename = outputFilename;
    }

    // Wait for the last frame to complete.
    finishRenderingAndSaveToFile(true);

#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    // Finalize movie file.
    if(videoEncoder)
        videoEncoder->closeFile();
#endif
}

/******************************************************************************
* Computes a viewport's area in the framebuffer to be rendered.
******************************************************************************/
QRect RenderSettings::viewportFramebufferArea(const Viewport* viewport, const ViewportConfiguration* viewportConfig) const
{
    QRect frameBufferRect(0, 0, outputImageWidth(), outputImageHeight());

    // Aspect ratio of the viewport rectangle in the rendered output image.
    if(renderAllViewports() && viewportConfig && viewport) {

        // Compute target rectangles of all viewports of the current layout.
        // TODO: This should be optimized. Computing the full layout every time seems unnecessary.
        std::vector<std::pair<Viewport*, QRectF>> viewportRects = viewportConfig->getViewportRectangles(frameBufferRect);

        // Find the viewport among the list of all viewports to look up its target rectangle in the output image.
        for(const std::pair<Viewport*, QRectF>& rect : viewportRects) {
            if(rect.first == viewport)
                return rect.second.toRect();
        }
    }

    return frameBufferRect;
}

}   // End of namespace
