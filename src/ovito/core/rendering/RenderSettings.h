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

	Q_PROPERTY(QString imageFilename READ imageFilename WRITE setImageFilename)

public:

	/// This enumeration specifies the animation range that should be rendered.
	enum RenderingRangeType {
		CURRENT_FRAME,		///< Renders the current animation frame.
		ANIMATION_INTERVAL,	///< Renders the complete animation interval.
		CUSTOM_INTERVAL,	///< Renders a user-defined time interval.
		CUSTOM_FRAME,		///< Renders a specific animation frame.
	};
	Q_ENUM(RenderingRangeType);

public:

	/// Constructor.
	/// Creates an instance of the default renderer class which can be accessed via the renderer() method.
	Q_INVOKABLE RenderSettings(ObjectCreationParams params);

	/// Returns the aspect ratio (height/width) of the rendered image.
	FloatType outputImageAspectRatio() const { return (FloatType)outputImageHeight() / (FloatType)outputImageWidth(); }

	/// Returns the background color of the rendered image.
	Color backgroundColor() const { return backgroundColorController() ? backgroundColorController()->getColorValue(AnimationTime(0)) : Color(0,0,0); }
	/// Sets the background color of the rendered image.
	void setBackgroundColor(const Color& color) { if(backgroundColorController()) backgroundColorController()->setColorValue(AnimationTime(0), color); }

	/// Returns the output filename of the rendered image.
	const QString& imageFilename() const { return imageInfo().filename(); }
	/// Sets the output filename of the rendered image.
	void setImageFilename(const QString& filename);

	/// \brief This is the high-level rendering function, which invokes the renderer to generate one or more
	///        output images of the scene. 
	/// \param viewportConfiguration The viewport configuration to render.
	/// \param frameBuffer The frame buffer that will receive the rendered image. When rendering an animation
	///        sequence, the buffer will contain only the last rendered frame when the function returns.
	/// \return true on success; false if operation has been canceled by the user.
	/// \throw Exception on error.
	bool renderScene(const ViewportConfiguration& viewportConfiguration, FrameBuffer& frameBuffer, MainThreadOperation& operation);

	/// \brief This is the high-level rendering function, which invokes the renderer to generate one or more
	///        output images of the scene. 
	/// \param viewportLayout The viewport layout.
	/// \param frameBuffer The frame buffer that will receive the rendered image. 
	/// \return true on success; false if operation has been canceled by the user.
	/// \throw Exception on error.
	bool renderScene(const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, AnimationSettings* animationSettings, FrameBuffer& frameBuffer, MainThreadOperation& operation);

private:

	/// Renders a single frame and saves the output file. This is part of the implementation of the renderScene() method.
	bool renderFrame(int frameNumber, SceneRenderer& renderer, FrameBuffer& frameBuffer, const std::vector<std::pair<Viewport*, QRectF>>& viewportLayout, VideoEncoder* videoEncoder, MainThreadOperation& operation);

private:

	/// Contains the output filename and format of the image to be rendered.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(ImageInfo, imageInfo, setImageInfo);

	/// The instance of the plugin renderer class.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SceneRenderer>, renderer, setRenderer, PROPERTY_FIELD_MEMORIZE);

	/// Controls the background color of the rendered image.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Controller>, backgroundColorController, setBackgroundColorController, PROPERTY_FIELD_MEMORIZE);

	/// The width of the output image in pixels.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, outputImageWidth, setOutputImageWidth, PROPERTY_FIELD_MEMORIZE);

	/// The height of the output image in pixels.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, outputImageHeight, setOutputImageHeight, PROPERTY_FIELD_MEMORIZE);

	/// Controls whether the alpha channel will be included in the output image.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, generateAlphaChannel, setGenerateAlphaChannel, PROPERTY_FIELD_MEMORIZE);

	/// Controls whether the rendered image is saved to the output file.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, saveToFile, setSaveToFile);

	/// Controls whether already rendered frames are skipped.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, skipExistingImages, setSkipExistingImages);

	/// Specifies which part of the animation should be rendered.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(RenderingRangeType, renderingRangeType, setRenderingRangeType);

	/// The first frame to render when rendering range is set to CUSTOM_INTERVAL.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, customRangeStart, setCustomRangeStart);

	/// The last frame to render when rendering range is set to CUSTOM_INTERVAL.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, customRangeEnd, setCustomRangeEnd);

	/// The frame to render when rendering range is set to CUSTOM_FRAME.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, customFrame, setCustomFrame);

	/// Specifies the number of frames to skip when rendering an animation.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, everyNthFrame, setEveryNthFrame);

	/// Specifies the base number for filename generation when rendering an animation.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, fileNumberBase, setFileNumberBase);

	/// The frames per second for encoding videos.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, framesPerSecond, setFramesPerSecond);

	/// Controls whether all viewports of the current viewport layout are rendered (or just the active viewport).
	DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, renderAllViewports, setRenderAllViewports);

	/// Controls the visibility of separators between viewports when rendering an entire viewport layout.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, layoutSeperatorsEnabled, setLayoutSeperatorsEnabled, PROPERTY_FIELD_MEMORIZE);

	/// Controls the width (in pixels) of the separators between viewports when rendering an entire viewport layout.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, layoutSeperatorWidth, setLayoutSeperatorWidth, PROPERTY_FIELD_MEMORIZE);

	/// Controls the color of the separator lines between viewports when rendering an entire viewport layout.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, layoutSeperatorColor, setLayoutSeperatorColor, PROPERTY_FIELD_MEMORIZE);

    friend class RenderSettingsEditor;
};

}	// End of namespace
