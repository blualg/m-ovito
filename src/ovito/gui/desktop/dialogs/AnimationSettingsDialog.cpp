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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/UndoStack.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "AnimationSettingsDialog.h"

namespace Ovito {

/******************************************************************************
* The constructor of the animation settings dialog.
******************************************************************************/
AnimationSettingsDialog::AnimationSettingsDialog(MainWindow& mainWindow, AnimationSettings* animSettings, QWidget* parent) :
		QDialog(parent), _animSettings(animSettings), UndoableTransaction(mainWindow, tr("Change animation settings"))
{
	setWindowTitle(tr("Animation Settings"));

	QVBoxLayout* layout1 = new QVBoxLayout(this);

	QGroupBox* animationBox = new QGroupBox(tr("Animation"));
	layout1->addWidget(animationBox);

	QGridLayout* contentLayout = new QGridLayout(animationBox);
	contentLayout->setHorizontalSpacing(0);
	contentLayout->setVerticalSpacing(2);
	contentLayout->setColumnStretch(1, 1);

	contentLayout->addWidget(new QLabel(tr("Frames per second:"), this), 0, 0);
	fpsBox = new QComboBox(this);
	QLocale locale;
	fpsBox->addItem(locale.toString(0.1), 0.1f);
	fpsBox->addItem(locale.toString(0.2), 0.2f);
	fpsBox->addItem(locale.toString(0.5), 0.5f);
	fpsBox->addItem(locale.toString(1), 1.0f);
	fpsBox->addItem(locale.toString(2), 2.0f);
	fpsBox->addItem(locale.toString(4), 4.0f);
	fpsBox->addItem(locale.toString(5), 5.0f);
	fpsBox->addItem(locale.toString(8), 8.0f);
	fpsBox->addItem(locale.toString(10), 10.0f);
	fpsBox->addItem(locale.toString(12), 12.0f);
	fpsBox->addItem(locale.toString(15), 15.0f);
	fpsBox->addItem(locale.toString(16), 16.0f);
	fpsBox->addItem(locale.toString(20), 20.0f);
	fpsBox->addItem(locale.toString(24), 24.0f);
	fpsBox->addItem(locale.toString(25), 25.0f);
	fpsBox->addItem(locale.toString(30), 30.0f);
	fpsBox->addItem(locale.toString(32), 32.0f);
	fpsBox->addItem(locale.toString(40), 40.0f);
	fpsBox->addItem(locale.toString(50), 50.0f);
	fpsBox->addItem(locale.toString(60), 60.0f);
	contentLayout->addWidget(fpsBox, 0, 1, 1, 2);
	connect(fpsBox, qOverload<int>(&QComboBox::activated), this, &AnimationSettingsDialog::onFramesPerSecondChanged);

	QGroupBox* interactiveBox = new QGroupBox(tr("Playback in interactive viewports"));
	layout1->addWidget(interactiveBox);

	contentLayout = new QGridLayout(interactiveBox);
	contentLayout->setHorizontalSpacing(0);
	contentLayout->setVerticalSpacing(4);
	contentLayout->setColumnMinimumWidth(1, 12);
	contentLayout->setColumnStretch(2, 1);

	contentLayout->addWidget(new QLabel(tr("Playback speed:"), this), 0, 0);
	playbackSpeedBox = new QComboBox(this);
	playbackSpeedBox->addItem(tr("x 1/40"), -40);
	playbackSpeedBox->addItem(tr("x 1/20"), -20);
	playbackSpeedBox->addItem(tr("x 1/10"), -10);
	playbackSpeedBox->addItem(tr("x 1/5"), -5);
	playbackSpeedBox->addItem(tr("x 1/2"), -2);
	playbackSpeedBox->addItem(tr("x 1 (realtime)"), 1);
	playbackSpeedBox->addItem(tr("x 2"), 2);
	playbackSpeedBox->addItem(tr("x 5"), 5);
	playbackSpeedBox->addItem(tr("x 10"), 10);
	playbackSpeedBox->addItem(tr("x 20"), 20);
	contentLayout->addWidget(playbackSpeedBox, 0, 2, 1, 2);
	connect(playbackSpeedBox, qOverload<int>(&QComboBox::activated), this, &AnimationSettingsDialog::onPlaybackSpeedChanged);

	contentLayout->addWidget(new QLabel(tr("Every Nth frame:"), this), 1, 0);
	QLineEdit* everyNthFrameBox = new QLineEdit(this);
	contentLayout->addWidget(everyNthFrameBox, 1, 2);
	everyNthFrameSpinner = new SpinnerWidget(this);
	everyNthFrameSpinner->setTextBox(everyNthFrameBox);
	everyNthFrameSpinner->setMinValue(1);
	contentLayout->addWidget(everyNthFrameSpinner, 1, 3);
	connect(everyNthFrameSpinner, &SpinnerWidget::spinnerValueChanged, this, [this]() {
		_animSettings->setPlaybackEveryNthFrame(everyNthFrameSpinner->intValue());
	});

	loopPlaybackBox = new QCheckBox(tr("Loop playback"));
	contentLayout->addWidget(loopPlaybackBox, 2, 2, 1, 2);
	connect(loopPlaybackBox, &QCheckBox::clicked, this, [this](bool checked) {
		_animSettings->setLoopPlayback(checked);
		loopPlaybackModified = true;
	});

	animIntervalBox = new QGroupBox(tr("Custom animation interval"));
	animIntervalBox->setCheckable(true);
	layout1->addWidget(animIntervalBox);

	contentLayout = new QGridLayout(animIntervalBox);
	contentLayout->setHorizontalSpacing(0);
	contentLayout->setVerticalSpacing(2);
	contentLayout->setColumnStretch(1, 1);

	contentLayout->addWidget(new QLabel(tr("Start frame:"), this), 0, 0);
	QLineEdit* animStartBox = new QLineEdit(this);
	contentLayout->addWidget(animStartBox, 0, 1);
	animStartSpinner = new SpinnerWidget(this);
	animStartSpinner->setTextBox(animStartBox);
	contentLayout->addWidget(animStartSpinner, 0, 2);
	connect(animStartSpinner, &SpinnerWidget::spinnerValueChanged, this, &AnimationSettingsDialog::onAnimationIntervalChanged);

	contentLayout->addWidget(new QLabel(tr("End frame:"), this), 1, 0);
	QLineEdit* animEndBox = new QLineEdit(this);
	contentLayout->addWidget(animEndBox, 1, 1);
	animEndSpinner = new SpinnerWidget(this);
	animEndSpinner->setTextBox(animEndBox);
	contentLayout->addWidget(animEndSpinner, 1, 2);
	connect(animEndSpinner, &SpinnerWidget::spinnerValueChanged, this, &AnimationSettingsDialog::onAnimationIntervalChanged);

	connect(animIntervalBox, &QGroupBox::clicked, this, [this](bool checked) {
		_animSettings->setAutoAdjustInterval(!checked);
		updateUI();
	});

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &AnimationSettingsDialog::onOk);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &AnimationSettingsDialog::reject);

	// Implement Help button.
	connect(buttonBox, &QDialogButtonBox::helpRequested, []() {
		ActionManager::openHelpTopic(QStringLiteral("manual:animation.animation_settings_dialog"));
	});

	layout1->addWidget(buttonBox);
    updateUI();
}

/******************************************************************************
* Event handler for the Ok button.
******************************************************************************/
void AnimationSettingsDialog::onOk()
{
	if(framesPerSecondModified)
		PROPERTY_FIELD(AnimationSettings::framesPerSecond)->memorizeDefaultValue(_animSettings);
	if(playbackSpeedModified)
		PROPERTY_FIELD(AnimationSettings::playbackSpeed)->memorizeDefaultValue(_animSettings);
	if(loopPlaybackModified)
		PROPERTY_FIELD(AnimationSettings::loopPlayback)->memorizeDefaultValue(_animSettings);

	commit();
	accept();
}

/******************************************************************************
* Updates the values shown in the dialog.
******************************************************************************/
void AnimationSettingsDialog::updateUI()
{
	fpsBox->setCurrentIndex(fpsBox->findData(_animSettings->framesPerSecond()));
	playbackSpeedBox->setCurrentIndex(playbackSpeedBox->findData(_animSettings->playbackSpeed()));
	animStartSpinner->setIntValue(_animSettings->firstFrame());
	animEndSpinner->setIntValue(_animSettings->lastFrame());
	loopPlaybackBox->setChecked(_animSettings->loopPlayback());
	animIntervalBox->setChecked(!_animSettings->autoAdjustInterval());
	animStartSpinner->setEnabled(!_animSettings->autoAdjustInterval());
	animEndSpinner->setEnabled(!_animSettings->autoAdjustInterval());
	everyNthFrameSpinner->setIntValue(_animSettings->playbackEveryNthFrame());
}

/******************************************************************************
* Is called when the user has selected a new value for the frames per seconds.
******************************************************************************/
void AnimationSettingsDialog::onFramesPerSecondChanged(int index)
{
	float newFramesPerSecond = fpsBox->itemData(index).toFloat();
	OVITO_ASSERT(newFramesPerSecond != 0.0f);

	_animSettings->setFramesPerSecond(newFramesPerSecond);
	framesPerSecondModified = true;

	// Update dialog controls to reflect new values.
	updateUI();
}

/******************************************************************************
* Is called when the user has selected a new value for the playback speed.
******************************************************************************/
void AnimationSettingsDialog::onPlaybackSpeedChanged(int index)
{
	int newPlaybackSpeed = playbackSpeedBox->itemData(index).toInt();
	OVITO_ASSERT(newPlaybackSpeed != 0);

	// Change the animation speed.
	_animSettings->setPlaybackSpeed(newPlaybackSpeed);
	playbackSpeedModified = true;

	// Update dialog controls to reflect new values.
	updateUI();
}

/******************************************************************************
* Is called when the user changes the start/end values of the animation interval.
******************************************************************************/
void AnimationSettingsDialog::onAnimationIntervalChanged()
{
	int firstFrame = animStartSpinner->intValue();
	int lastFrame = animEndSpinner->intValue();
	if(lastFrame < firstFrame)
		lastFrame = firstFrame;

	_animSettings->setFirstFrame(firstFrame);
	_animSettings->setLastFrame(lastFrame);
	_animSettings->setCurrentFrame(qBound(firstFrame, _animSettings->currentFrame(), lastFrame));

	// Update dialog controls to reflect new values.
	updateUI();
}

}	// End of namespace
