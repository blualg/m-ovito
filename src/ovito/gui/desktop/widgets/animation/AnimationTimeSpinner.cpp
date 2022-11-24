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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "AnimationTimeSpinner.h"

namespace Ovito {

using namespace std;

/******************************************************************************
* Constructs the spinner control.
******************************************************************************/
AnimationTimeSpinner::AnimationTimeSpinner(MainWindow* mainWindow, QWidget* parent) : SpinnerWidget(parent)
{
	connect(this, &SpinnerWidget::spinnerValueChanged, this, &AnimationTimeSpinner::onSpinnerValueChanged);
	connect(&mainWindow->datasetContainer(), &DataSetContainer::animationSettingsReplaced, this, &AnimationTimeSpinner::onAnimationSettingsReplaced);

	onAnimationSettingsReplaced(mainWindow->datasetContainer().currentSet() ? mainWindow->datasetContainer().currentSet()->animationSettings() : nullptr);
}

/******************************************************************************
* This is called when new animation settings have been loaded.
******************************************************************************/
void AnimationTimeSpinner::onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings)
{
	disconnect(_animIntervalChangedConnection);
	disconnect(_currentFrameChangedConnection);
	_animSettings = newAnimationSettings;
	if(newAnimationSettings) {
		_animIntervalChangedConnection = connect(newAnimationSettings, &AnimationSettings::intervalChanged, this, &AnimationTimeSpinner::onIntervalChanged);
		_currentFrameChangedConnection = connect(newAnimationSettings, &AnimationSettings::currentFrameChanged, this, &AnimationTimeSpinner::onCurrentFrameChanged);
		onIntervalChanged(newAnimationSettings->firstFrame(), newAnimationSettings->lastFrame());
		onCurrentFrameChanged(newAnimationSettings->currentFrame());
	}
	else {
		onIntervalChanged(0, 0);
		onCurrentFrameChanged(0);
	}
}

/******************************************************************************
* This is called whenever the current animation time has changed.
******************************************************************************/
void AnimationTimeSpinner::onCurrentFrameChanged(int newFrame)
{
	setIntValue(newFrame);
}

/******************************************************************************
* This is called whenever the active animation interval has changed.
******************************************************************************/
void AnimationTimeSpinner::onIntervalChanged(int firstFrame, int lastFrame)
{
	// Set the limits of the spinner to the new animation time interval.
	setMinValue(firstFrame);
	setMaxValue(lastFrame);
	setEnabled(lastFrame > firstFrame);
}

/******************************************************************************
* Is called when the spinner value has been changed by the user.
******************************************************************************/
void AnimationTimeSpinner::onSpinnerValueChanged()
{
	// Set a new animation time.
	if(_animSettings)
		_animSettings->setCurrentFrame(intValue());
}

}	// End of namespace
