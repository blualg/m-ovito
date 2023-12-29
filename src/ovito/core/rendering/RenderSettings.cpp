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
#include <ovito/core/rendering/StandardSceneRenderer.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/viewport/ViewportSuspender.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
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
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeperatorsEnabled);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeperatorWidth);
DEFINE_PROPERTY_FIELD(RenderSettings, layoutSeperatorColor);
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
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeperatorsEnabled, "Layout separators");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeperatorWidth, "Separator width");
SET_PROPERTY_FIELD_LABEL(RenderSettings, layoutSeperatorColor, "Separator color");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageWidth, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, outputImageHeight, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, everyNthFrame, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, framesPerSecond, IntegerParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RenderSettings, layoutSeperatorWidth, IntegerParameterUnit, 1);

/******************************************************************************
* Constructor.
******************************************************************************/
RenderSettings::RenderSettings(ObjectInitializationFlags flags) : RefTarget(flags),
    _outputImageWidth(640),
    _outputImageHeight(480),
    _generateAlphaChannel(false),
    _saveToFile(false),
    _skipExistingImages(false),
    _renderingRangeType(CURRENT_FRAME),
    _customRangeStart(0),
    _customRangeEnd(100),
    _customFrame(0),
    _everyNthFrame(1),
    _fileNumberBase(0),
    _framesPerSecond(0),
    _renderAllViewports(false),
    _layoutSeperatorsEnabled(false),
    _layoutSeperatorWidth(2),
    _layoutSeperatorColor(0.5, 0.5, 0.5)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Setup default background color.
        setBackgroundColorController(ControllerManager::createColorController());
        setBackgroundColor(Color(1,1,1));

        // Create an instance of the default renderer class.
        setRenderer(OORef<StandardSceneRenderer>::create(flags));
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
void RenderSettings::render(const ViewportConfiguration& viewportConfiguration, FrameBuffer& frameBuffer)
{
    std::vector<std::pair<Viewport*, QRectF>> viewportLayout;
    if(renderAllViewports()) {
        // When rendering an entire viewport layout, determine the each viewport's destination rectangle within the output frame buffer.
        QSizeF borderSize(0,0);
        if(layoutSeperatorsEnabled()) {
            // Convert separator width from pixels to reduced units, which are relative to the framebuffer width/height.
            borderSize.setWidth( 1.0 / outputImageWidth()  * layoutSeperatorWidth());
            borderSize.setHeight(1.0 / outputImageHeight() * layoutSeperatorWidth());
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

    render(viewportLayout, animationSettings, frameBuffer);
}

/******************************************************************************
* This is the high-level rendering function, which invokes the renderer to
* generate one or more output images of the scene.
******************************************************************************/
void RenderSettings::render(const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, AnimationSettings* animationSettings, FrameBuffer& frameBuffer)
{
    // Get the selected scene renderer.
    // Note: Using ref-counted pointer here, because the renderer may potentially be deleted before the current function returns.
    OORef<SceneRenderer> renderer = this->renderer();
    if(!renderer)
        throw Exception(tr("No rendering engine has been selected."));

    // Create a ref-counted pointer to ourself to keep the RenderSettings alive even if the application
    // is shutting down while we are still in this function.
    OORef<RenderSettings> self(this);

    try {
        // Resize output frame buffer.
        if(frameBuffer.size() != QSize(outputImageWidth(), outputImageHeight())) {
            frameBuffer.setSize(QSize(outputImageWidth(), outputImageHeight()));
            frameBuffer.clear();
        }

        // Don't update viewports while rendering.
        ViewportSuspender noVPUpdates;

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
        this_task::setProgressText(tr("Initializing renderer"));
        if(renderer->startRender(this, largestViewportRectSize, Application::instance()->visCache())) {

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

            if(renderingRangeType() == RenderSettings::CURRENT_FRAME) {
                // Render a single frame.
                int frameNumber = animationSettings ? animationSettings->currentFrame() : 0;
                this_task::setProgressText(tr("Rendering frame %1").arg(frameNumber));
                renderFrame(frameNumber, *renderer, frameBuffer, viewportLayout, videoEncoder);
            }
            else if(renderingRangeType() == RenderSettings::CUSTOM_FRAME) {
                // Render a specific frame.
                int frameNumber = customFrame();
                this_task::setProgressText(tr("Rendering frame %1").arg(frameNumber));
                renderFrame(frameNumber, *renderer, frameBuffer, viewportLayout, videoEncoder);
            }
            else if(renderingRangeType() == RenderSettings::ANIMATION_INTERVAL || renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) {
                // Render an animation interval.
                int firstFrameNumber, numberOfFrames;
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
                this_task::setProgressMaximum(numberOfFrames);

                // Render frames one by one.
                for(int frameIndex = 0; frameIndex < numberOfFrames && !this_task::isCanceled(); frameIndex++) {
                    int frameNumber = firstFrameNumber + frameIndex * everyNthFrame() + fileNumberBase();

                    this_task::setProgressValue(frameIndex);
                    this_task::setProgressText(tr("Rendering animation (frame %1 of %2)").arg(frameIndex+1).arg(numberOfFrames));

                    // Create a sub-task to display a second progress bar.
                    MainThreadOperation frameTask;

                    // Render the animation frame.
                    renderFrame(frameNumber, *renderer, frameBuffer, viewportLayout, videoEncoder);

                    // Periodically free visual element resources during animation rendering to avoid clogging the memory.
                    Application::instance()->visCache().discardUnusedObjects();
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
            Application::instance()->visCache().discardUnusedObjects();
    }
    catch(...) {
        // Shutdown renderer.
        renderer->endRender();
        throw;
    }
}

/******************************************************************************
* Renders a single frame and saves the output file.
******************************************************************************/
void RenderSettings::renderFrame(int frameNumber, SceneRenderer& renderer,
        FrameBuffer& frameBuffer, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, VideoEncoder* videoEncoder)
{
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

            // Check for existing image file and skip.
            if(skipExistingImages() && QFileInfo(outputFilename).isFile())
                return;
        }
    }

    // Compute relative weights of the viewport rectangles for the progress display.
    std::vector<int> progressWeights(viewportLayout.size());
    std::transform(viewportLayout.cbegin(), viewportLayout.cend(), progressWeights.begin(), [&](const auto& r) {
        return r.second.width() * r.second.height() * frameBuffer.width() * frameBuffer.height();
    });
    this_task::beginProgressSubStepsWithWeights(std::move(progressWeights));

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
            Box3 boundingBox = renderer.computeSceneBoundingBox(renderTime, viewport->scene(), projParams, nullptr);
            if(this_task::isCanceled())
                return;

            // Determine final view projection.
            projParams = viewport->computeProjectionParameters(renderTime, viewportAspectRatio, renderer.waitForLongOperationsEnabled(), boundingBox);

            // Render one frame.
            try {
                renderer.beginFrame(renderTime, viewport->scene(), projParams, viewport, destinationRect, &frameBuffer);

                // Clear frame buffer with background color.
                ColorA clearColor = generateAlphaChannel() ? ColorA(0,0,0,0) : ColorA(backgroundColorAt(renderTime));
                frameBuffer.clear(clearColor, destinationRect, true);

                // Render viewport "underlays".
                if(!renderer.renderOverlays(true, destinationRect, destinationRect)) {
                    renderer.endFrame(false, destinationRect);
                    return;
                }

                // Let the scene renderer do its work.
                if(!renderer.renderFrame(destinationRect)) {
                    renderer.endFrame(false, destinationRect);
                    return;
                }

                // Render viewport "overlays" on top.
                if(!renderer.renderOverlays(false, destinationRect, destinationRect)) {
                    renderer.endFrame(false, destinationRect);
                    return;
                }

                renderer.endFrame(true, destinationRect);
            }
            catch(...) {
                renderer.endFrame(false, destinationRect);
                throw;
            }
        }

        this_task::nextProgressSubStep();
    }
    this_task::endProgressSubSteps();

    // Write rendered image or video frame to disk.
    if(saveToFile()) {
        if(!videoEncoder) {
            OVITO_ASSERT(!outputFilename.isEmpty());

            // The QImage.save() function requires a Qt application object (to load file format plugins).
            Application::instance()->createQtApplication(false);

            // Use the QImage.save() function to save the rendered image to disk.
            if(!frameBuffer.image().save(outputFilename, imageInfo().format()))
                throw Exception(tr("Failed to save rendered image to output file '%1'.").arg(outputFilename));
        }
        else {
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
            videoEncoder->writeFrame(frameBuffer.image());
#endif
        }
    }
}

}   // End of namespace
