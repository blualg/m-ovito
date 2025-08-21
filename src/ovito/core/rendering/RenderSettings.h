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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "FrameBuffer.h"
#include "SceneRenderer.h"

namespace Ovito {

/**
 * Stores general settings for rendering pictures and movies.
 */
class OVITO_CORE_EXPORT RenderSettings : public RefTarget
{
    OVITO_CLASS(RenderSettings)

public:

    /// This enumeration specifies the animation range that should be rendered.
    enum RenderingRangeType {
        CURRENT_FRAME,      ///< Renders the current animation frame.
        ANIMATION_INTERVAL, ///< Renders the complete animation interval.
        CUSTOM_INTERVAL,    ///< Renders a user-defined time interval.
        CUSTOM_FRAME,       ///< Renders a specific animation frame.
    };
    Q_ENUM(RenderingRangeType);

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the aspect ratio (height/width) of the rendered image.
    FloatType outputImageAspectRatio() const { return (FloatType)outputImageHeight() / (FloatType)outputImageWidth(); }

    /// Returns the background color of the rendered image.
    Color backgroundColor() const { return backgroundColorAt(AnimationTime(0)); }
    /// Returns the background color of the rendered image at the given animation time.
    Color backgroundColorAt(AnimationTime time) const { return backgroundColorController() ? backgroundColorController()->getColorValue(time) : Color(0,0,0); }
    /// Sets the background color of the rendered image.
    void setBackgroundColor(const Color& color) { if(backgroundColorController()) backgroundColorController()->setColorValue(AnimationTime(0), color); }

    /// Returns the output filename of the rendered image.
    const QString& imageFilename() const { return imageInfo().filename(); }
    /// Sets the output filename of the rendered image.
    void setImageFilename(const QString& filename);

    /// Formats the image filename and replaces wildcards with the current frame number.
    static QString formatImageFilename(const QString& filename, int frameNumber);

    /// Returns whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    bool stopOnPipelineError() const { return _stopOnPipelineError; }
    /// Sets whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    void setStopOnPipelineError(bool stopOnPipelineError) { _stopOnPipelineError = stopOnPipelineError; }

    /// High-level rendering function that invokes the renderer to generate one or more output images of the scene.
    [[nodiscard]] Future<void> render(const ViewportConfiguration& viewportConfiguration, const std::shared_ptr<FrameBuffer>& outputFrameBuffer);

    /// High-level rendering function that invokes the renderer to generate one or more output images of the scene.
    [[nodiscard]] Future<void> render(const std::vector<std::pair<Viewport*, QRectF>> viewportLayout, OORef<const AnimationSettings> animationSettings, const std::shared_ptr<FrameBuffer> outputFrameBuffer);

    /// Computes a viewport's area in the rendered output image.
    QRect viewportFramebufferArea(const Viewport* viewport, const ViewportConfiguration* viewportConfig) const;

private:

    /// Contains the output filename and format of the image to be rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ImageInfo{}, imageInfo, setImageInfo);

    /// The filename of the output image.
    DECLARE_VIRTUAL_PROPERTY_FIELD(QString, imageFilename);

    /// The instance of the plugin renderer class.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SceneRenderer>, renderer, setRenderer, PROPERTY_FIELD_MEMORIZE);

    /// Controls the background color of the rendered image.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Controller>, backgroundColorController, setBackgroundColorController, PROPERTY_FIELD_MEMORIZE);

    /// The width of the output image in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{640}, outputImageWidth, setOutputImageWidth, PROPERTY_FIELD_MEMORIZE);

    /// The height of the output image in pixels.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{480}, outputImageHeight, setOutputImageHeight, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the alpha channel will be included in the output image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, generateAlphaChannel, setGenerateAlphaChannel, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the rendered image is saved to the output file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, saveToFile, setSaveToFile);

    /// Controls whether already rendered frames are skipped.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, skipExistingImages, setSkipExistingImages);

    /// Specifies which part of the animation should be rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(RenderingRangeType{CURRENT_FRAME}, renderingRangeType, setRenderingRangeType);

    /// The first frame to render when rendering range is set to CUSTOM_INTERVAL.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customRangeStart, setCustomRangeStart);

    /// The last frame to render when rendering range is set to CUSTOM_INTERVAL.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{100}, customRangeEnd, setCustomRangeEnd);

    /// The frame to render when rendering range is set to CUSTOM_FRAME.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customFrame, setCustomFrame);

    /// Specifies the number of frames to skip when rendering an animation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{1}, everyNthFrame, setEveryNthFrame);

    /// Specifies the base number for filename generation when rendering an animation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, fileNumberBase, setFileNumberBase);

    /// The frames per second for encoding videos.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, framesPerSecond, setFramesPerSecond);

    /// Controls whether all viewports of the current viewport layout are rendered (or just the active viewport).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, renderAllViewports, setRenderAllViewports);

    /// Controls the visibility of separators between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, layoutSeparatorsEnabled, setLayoutSeparatorsEnabled, PROPERTY_FIELD_MEMORIZE);

    /// Controls the width (in pixels) of the separators between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{2}, layoutSeparatorWidth, setLayoutSeparatorWidth, PROPERTY_FIELD_MEMORIZE);

    /// Controls the color of the separator lines between viewports when rendering an entire viewport layout.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.5, 0.5, 0.5}), layoutSeparatorColor, setLayoutSeparatorColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether errors that occur within a data pipeline lead to an abortion of the rendering process.
    bool _stopOnPipelineError = false;

    friend class RenderSettingsEditor;
};

}   // End of namespace
