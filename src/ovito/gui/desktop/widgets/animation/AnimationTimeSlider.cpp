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
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include "AnimationTimeSlider.h"

namespace Ovito {

using namespace std;

/******************************************************************************
* Constructor.
******************************************************************************/
AnimationTimeSlider::AnimationTimeSlider(MainWindow& mainWindow, QWidget* parent) :
	QFrame(parent), _mainWindow(mainWindow)
{
	updateColorPalettes();

	setFrameShape(QFrame::NoFrame);
	setAutoFillBackground(true);
	setMouseTracking(true);
	setFocusPolicy(Qt::ClickFocus);

	connect(&mainWindow.datasetContainer(), &DataSetContainer::animationSettingsReplaced, this, &AnimationTimeSlider::onAnimationSettingsReplaced);
	connect(mainWindow.actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE), &QAction::toggled, this, &AnimationTimeSlider::onAutoKeyModeChanged);
}

/******************************************************************************
* Creates the color palettes used by the widget.
******************************************************************************/
void AnimationTimeSlider::updateColorPalettes()
{
	_normalPalette = QGuiApplication::palette();
	_autoKeyModePalette = QGuiApplication::palette();
	_autoKeyModePalette.setColor(QPalette::Window, QColor(240, 60, 60));
	_sliderPalette = QGuiApplication::palette();
	_sliderPalette.setColor(QPalette::Button, 
		_mainWindow.darkTheme() ?
		_sliderPalette.color(QPalette::Button).lighter(150) :
		_sliderPalette.color(QPalette::Button).darker(110));
}

/******************************************************************************
* Handles widget state changes.
******************************************************************************/
void AnimationTimeSlider::changeEvent(QEvent* event)
{
	if(event->type() == QEvent::PaletteChange) {
		updateColorPalettes();
	}
	QFrame::changeEvent(event);
}

/******************************************************************************
* This is called when new animation settings have been loaded.
******************************************************************************/
void AnimationTimeSlider::onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings)
{
	disconnect(_animIntervalChangedConnection);
	disconnect(_timeFormatChangedConnection);
	disconnect(_currentFrameChangedConnection);
	_animSettings = newAnimationSettings;
	if(newAnimationSettings) {
		_animIntervalChangedConnection = connect(newAnimationSettings, &AnimationSettings::intervalChanged, this, qOverload<>(&AnimationTimeSlider::update));
		_timeFormatChangedConnection = connect(newAnimationSettings, &AnimationSettings::timeFormatChanged, this, qOverload<>(&AnimationTimeSlider::update));
		_currentFrameChangedConnection = connect(newAnimationSettings, &AnimationSettings::currentFrameChanged, this, qOverload<>(&AnimationTimeSlider::update));
	}
	update();
}

/******************************************************************************
* Handles paint events.
******************************************************************************/
void AnimationTimeSlider::paintEvent(QPaintEvent* event)
{
	QFrame::paintEvent(event);
	if(!_animSettings) return;

	// Show slider only if there is more than one animation frame.
	int numFrames = _animSettings->lastFrame() - _animSettings->firstFrame() + 1;
	if(numFrames > 1) {
		QStylePainter painter(this);

		QRect clientRect = frameRect();
		clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
		int thumbWidth = this->thumbWidth();
		int startFrame, frameStep, endFrame;
		std::tie(startFrame, frameStep, endFrame) = tickRange(maxTickLabelWidth());

		painter.setPen(QPen(QColor(180,180,220)));
		for(int frame = startFrame; frame <= endFrame; frame += frameStep) {
			QString labelText = QString::number(frame);
			painter.drawText(frameToPos(frame) - thumbWidth/2, clientRect.y(), thumbWidth, clientRect.height(), Qt::AlignCenter, labelText);
		}

		QStyleOptionButton btnOption;
		btnOption.initFrom(this);
		btnOption.rect = thumbRectangle();
		if(_animSettings->firstFrame() == 0)
			btnOption.text = QStringLiteral("%1 / %2").arg(_animSettings->currentFrame()).arg(_animSettings->lastFrame());
		else
			btnOption.text = QString::number(_animSettings->currentFrame());
		btnOption.state = ((_dragPos >= 0) ? QStyle::State_Sunken : QStyle::State_Raised) | QStyle::State_Enabled;
		btnOption.palette = _sliderPalette;
		painter.drawPrimitive(QStyle::PE_PanelButtonCommand, btnOption);
		btnOption.palette = palette();
		painter.drawControl(QStyle::CE_PushButtonLabel, btnOption);
	}
}

/******************************************************************************
* Computes the maximum width of a frame tick label.
******************************************************************************/
int AnimationTimeSlider::maxTickLabelWidth()
{
	if(!_animSettings) return 0;
	QString label = QString::number(_animSettings->timeToFrame(_animSettings->animationInterval().end()));
	return fontMetrics().boundingRect(label).width() + 20;
}

/******************************************************************************
* Computes the time ticks to draw.
******************************************************************************/
std::tuple<int,int,int> AnimationTimeSlider::tickRange(int tickWidth)
{
	if(_animSettings) {
		QRect clientRect = frameRect();
		clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
		int thumbWidth = this->thumbWidth();
		int clientWidth = clientRect.width() - thumbWidth;
		int firstFrame = _animSettings->firstFrame();
		int lastFrame = _animSettings->lastFrame();
		int numFrames = lastFrame - firstFrame + 1;
		int nticks = std::min(clientWidth / tickWidth, numFrames);
		int ticksevery = numFrames / std::max(nticks, 1);
		if(ticksevery <= 1) ticksevery = ticksevery;
		else if(ticksevery <= 5) ticksevery = 5;
		else if(ticksevery <= 10) ticksevery = 10;
		else if(ticksevery <= 20) ticksevery = 20;
		else if(ticksevery <= 50) ticksevery = 50;
		else if(ticksevery <= 100) ticksevery = 100;
		else if(ticksevery <= 500) ticksevery = 500;
		else if(ticksevery <= 1000) ticksevery = 1000;
		else if(ticksevery <= 2000) ticksevery = 2000;
		else if(ticksevery <= 5000) ticksevery = 5000;
		else if(ticksevery <= 10000) ticksevery = 10000;
		if(ticksevery > 0) {
			return std::make_tuple(firstFrame, ticksevery, lastFrame);
		}
	}
	return std::tuple<int,int,int>(0, 1, 0);
}

/******************************************************************************
* Computes the x position within the widget corresponding to the given animation time.
******************************************************************************/
int AnimationTimeSlider::frameToPos(int frame)
{
	if(!_animSettings) 
		return 0;
	FloatType fraction = (FloatType)(frame - _animSettings->firstFrame()) / (FloatType)(_animSettings->lastFrame() - _animSettings->firstFrame() + 1);
	QRect clientRect = frameRect();
	int tw = thumbWidth();
	int space = clientRect.width() - 2*frameWidth() - tw;
	return clientRect.x() + frameWidth() + (int)(fraction * space) + tw / 2;
}

/******************************************************************************
* Converts a distance in pixels to a frame difference.
******************************************************************************/
int AnimationTimeSlider::distanceToFrameDifference(int distance)
{
	if(!_animSettings) return 0;
	QRect clientRect = frameRect();
	int tw = thumbWidth();
	int space = clientRect.width() - 2 * frameWidth() - tw;
	return (int)((qint64)(_animSettings->lastFrame() - _animSettings->firstFrame() + 1) * distance / space);
}

/******************************************************************************
* Returns the recommended size for the widget.
******************************************************************************/
QSize AnimationTimeSlider::sizeHint() const
{
	return QSize(QFrame::sizeHint().width(), fontMetrics().height() + frameWidth() * 2 + 6);
}

/******************************************************************************
* Handles mouse down events.
******************************************************************************/
void AnimationTimeSlider::mousePressEvent(QMouseEvent* event)
{
	QRect thumbRect = thumbRectangle();
	if(thumbRect.contains(event->pos())) {
		_dragPos = ViewportInputMode::getMousePosition(event).x() - thumbRect.x();
	}
	else {
		_dragPos = thumbRect.width() / 2;
		mouseMoveEvent(event);
	}
	event->accept();
	update();
}

/******************************************************************************
* Is called when the widgets looses the input focus.
******************************************************************************/
void AnimationTimeSlider::focusOutEvent(QFocusEvent* event)
{
	_dragPos = -1;
	QFrame::focusOutEvent(event);
}

/******************************************************************************
* Handles mouse up events.
******************************************************************************/
void AnimationTimeSlider::mouseReleaseEvent(QMouseEvent* event)
{
	_dragPos = -1;
	event->accept();
	update();
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void AnimationTimeSlider::mouseMoveEvent(QMouseEvent* event)
{
	event->accept();
	if(!_animSettings) return;

	int newPos;
	int thumbSize = thumbWidth();

	if(_dragPos < 0)
		newPos = ViewportInputMode::getMousePosition(event).x() - thumbSize / 2;
	else
		newPos = ViewportInputMode::getMousePosition(event).x() - _dragPos;

	int rectWidth = frameRect().width() - 2*frameWidth();
	int newFrame = ((qint64)newPos * (qint64)(_animSettings->lastFrame() - _animSettings->firstFrame() + 1) / (qint64)(rectWidth - thumbSize)) + _animSettings->firstFrame();

	// Clamp new frame to animation interval.
	newFrame = qBound(_animSettings->firstFrame(), newFrame, _animSettings->lastFrame());

	if(_dragPos >= 0) {

		if(newFrame == _animSettings->currentFrame()) 
			return;

		// Set new time.
		_animSettings->setCurrentFrame(newFrame);

		// Force immediate viewport update.
		_mainWindow->processViewportUpdates();
		repaint();
	}
	else if(_animSettings->lastFrame() > _animSettings->firstFrame()) {
		if(thumbRectangle().contains(event->pos()) == false) {
			FloatType fraction = (FloatType)(newFrame - _animSettings->firstFrame())
					/ (FloatType)(_animSettings->lastFrame() - _animSettings->firstFrame() + 1);
			QRect clientRect = frameRect();
			clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
			int clientWidth = clientRect.width() - thumbWidth();
			QPoint pos(clientRect.x() + (int)(fraction * clientWidth) + thumbWidth() / 2, clientRect.height() / 2);
			QString frameName = _animSettings->namedFrames()[newFrame];
			QString tooltipText;
			if(!frameName.isEmpty())
				tooltipText = QString("%1 - %2").arg(newFrame).arg(frameName);
			else
				tooltipText = QString::number(newFrame);
			QToolTip::showText(mapToGlobal(pos), tooltipText, this);
		}
		else QToolTip::hideText();
	}
}

/******************************************************************************
* Computes the width of the thumb.
******************************************************************************/
int AnimationTimeSlider::thumbWidth() const
{
	int standardWidth = 70;
	// Expand the thumb width for animations with a large number of frames.
	if(_animSettings) {
		int nframes = _animSettings->lastFrame() - _animSettings->firstFrame();
		if(nframes > 1) {
			standardWidth += 10 * (int)std::log10(nframes);
		}
	}
	int clientWidth = frameRect().width() - 2*frameWidth();
	return std::min(clientWidth / 2, standardWidth);
}

/******************************************************************************
* Computes the coordinates of the slider thumb.
******************************************************************************/
QRect AnimationTimeSlider::thumbRectangle()
{
	if(!_animSettings)
		return QRect(0,0,0,0);

	int value = qBound(_animSettings->firstFrame(), _animSettings->currentFrame(), _animSettings->lastFrame());
	FloatType fraction = (FloatType)(value - _animSettings->firstFrame()) / (FloatType)(_animSettings->lastFrame() - _animSettings->firstFrame() + 1);

	QRect clientRect = frameRect();
	clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
	int thumbSize = thumbWidth();
	int thumbPos = (int)((clientRect.width() - thumbSize) * fraction);
	return QRect(thumbPos + clientRect.x(), clientRect.y(), thumbSize, clientRect.height());
}

/******************************************************************************
* Is called whenever the Auto Key mode is activated or deactivated.
******************************************************************************/
void AnimationTimeSlider::onAutoKeyModeChanged(bool active)
{
	setPalette(active ? _autoKeyModePalette : _normalPalette);
	update();
}

}	// End of namespace
