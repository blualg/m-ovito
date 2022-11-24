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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/StandardSceneRenderer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/DataSet.h>
#include "RenderSettings.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(RenderSettings);
DEFINE_PROPERTY_FIELD(RenderSettings, imageInfo);
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
RenderSettings::RenderSettings(ObjectCreationParams params) : RefTarget(params),
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
	if(params.createSubObjects()) {
		// Setup default background color.
		setBackgroundColorController(ControllerManager::createColorController());
		setBackgroundColor(Color(1,1,1));

		// Create an instance of the default renderer class.
		setRenderer(OORef<StandardSceneRenderer>::create(params));
	}
}

/******************************************************************************
* Sets the output filename of the rendered image.
******************************************************************************/
void RenderSettings::setImageFilename(const QString& filename)
{
	if(filename == imageFilename()) return;
	ImageInfo newInfo = imageInfo();
	newInfo.setFilename(filename);
	setImageInfo(newInfo);
}

}	// End of namespace
