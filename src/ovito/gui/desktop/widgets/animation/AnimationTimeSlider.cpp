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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AnimationTimeSlider.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AnimationTimeSlider::AnimationTimeSlider(MainWindowUI& ui, QWidget* parent) :
    QFrame(parent), UserInterfaceComponent<MainWindowUI>(ui)
{
    updateColorPalettes();

    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    connect(&datasetContainer(), &DataSetContainer::animationIntervalChanged, this, qOverload<>(&AnimationTimeSlider::update));
    connect(&datasetContainer(), &DataSetContainer::timeFormatChanged, this, qOverload<>(&AnimationTimeSlider::update));
    connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, qOverload<>(&AnimationTimeSlider::update));
    connect(actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE), &QAction::toggled, this, &AnimationTimeSlider::onAutoKeyModeChanged);
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
        GuiApplication::instance()->usingDarkTheme() ?
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
* Handles paint events.
******************************************************************************/
void AnimationTimeSlider::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    if(!activeAnimationSettings())
        return;

    // Show slider only if there is more than one animation frame.
    int numFrames = activeAnimationSettings()->numberOfFrames();
    if(numFrames > 1) {
        QStylePainter painter(this);

        QRect clientRect = frameRect();
        clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
        int thumbWidth = this->thumbWidth();
        int startFrame, frameStep, endFrame;
        std::tie(startFrame, frameStep, endFrame) = tickRange(maxTickLabelWidth());

        painter.setPen(QPen(QColor(180,180,220)));
        for(int frame = startFrame; frame <= endFrame; frame += frameStep) {
            painter.drawText(frameToPos(frame) - thumbWidth/2, clientRect.y(), thumbWidth, clientRect.height(), Qt::AlignCenter, tickLabel(frame));
        }

        QStyleOptionButton btnOption;
        btnOption.initFrom(this);
        btnOption.rect = thumbRectangle();
        btnOption.text = thumbText(activeAnimationSettings()->currentFrame());
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
int AnimationTimeSlider::maxTickLabelWidth() const
{
    if(!activeAnimationSettings())
        return 0;

    // Compute the maximum width of a frame tick label.
    const auto& metrics = fontMetrics();
    int maxWidth = 0;

    QString label = tickLabel(activeAnimationSettings()->lastFrame());
    maxWidth = metrics.horizontalAdvance(label);

    // When showing named frames, consider a few more frames in addition to the
    // last one - to account for the fact that the length of the labels may not
    // be monotonically increasing in this case.
    if(activeAnimationSettings()->preferSimulationTimeDisplay()) {
        const auto& frameLabels = activeAnimationSettings()->frameLabels();
        if(!frameLabels.empty()) {
            for(int frame = std::max(activeAnimationSettings()->firstFrame(), activeAnimationSettings()->lastFrame() - 5); frame < activeAnimationSettings()->lastFrame(); ++frame) {
                maxWidth = std::max(maxWidth, metrics.horizontalAdvance(tickLabel(frame)));
            }
        }
    }

    return maxWidth + 8; // add some margin
}

/******************************************************************************
* Computes the optimal distribution of tick marks along the timeline.
******************************************************************************/
std::tuple<int,int,int> AnimationTimeSlider::tickRange(int minTickSeparation)
{
    if(activeAnimationSettings() && minTickSeparation > 0) {
        int firstFrame = activeAnimationSettings()->firstFrame();
        int lastFrame = activeAnimationSettings()->lastFrame();
        int availableSpace = frameToPos(lastFrame) - frameToPos(firstFrame);
        int numFrames = lastFrame - firstFrame + 1;
        int nticks = std::max(1, std::min((availableSpace + minTickSeparation - 1) / minTickSeparation, numFrames));
        int ticksevery = (numFrames + nticks - 1) / nticks;
        if(ticksevery <= 1) ticksevery = 1;
        else if(ticksevery <= 5) ticksevery = 5;
        else if(ticksevery <= 10) ticksevery = 10;
        else if(ticksevery <= 20) ticksevery = 20;
        else if(ticksevery <= 50) ticksevery = 50;
        else if(ticksevery <= 100) ticksevery = 100;
        else if(ticksevery <= 200) ticksevery = 200;
        else if(ticksevery <= 500) ticksevery = 500;
        else if(ticksevery <= 1000) ticksevery = 1000;
        else if(ticksevery <= 2000) ticksevery = 2000;
        else if(ticksevery <= 5000) ticksevery = 5000;
        else if(ticksevery <= 10000) ticksevery = 10000;
        else if(ticksevery <= 20000) ticksevery = 20000;
        else if(ticksevery <= 50000) ticksevery = 50000;
        else if(ticksevery <= 100000) ticksevery = 100000;
        else if(ticksevery <= 200000) ticksevery = 200000;
        else if(ticksevery <= 500000) ticksevery = 500000;
        if(ticksevery > 0) {
            return std::make_tuple(firstFrame, ticksevery, lastFrame);
        }
    }
    return std::make_tuple(0, 1, 0);
}

/******************************************************************************
* Computes the x position within the widget corresponding to the given animation time.
******************************************************************************/
int AnimationTimeSlider::frameToPos(int frame)
{
    if(!activeAnimationSettings())
        return 0;
    FloatType fraction = (FloatType)(frame - activeAnimationSettings()->firstFrame()) / std::max(1, activeAnimationSettings()->numberOfFrames() - 1);
    QRect clientRect = frameRect();
    int tw = thumbWidth();
    int space = clientRect.width() - 2 * frameWidth() - tw;
    return clientRect.x() + frameWidth() + (int)(fraction * space) + tw / 2;
}

/******************************************************************************
* Converts a distance in pixels to a frame difference.
******************************************************************************/
int AnimationTimeSlider::distanceToFrameDifference(int distance)
{
    if(!activeAnimationSettings())
        return 0;
    QRect clientRect = frameRect();
    int tw = thumbWidth();
    int space = clientRect.width() - 2 * frameWidth() - tw;
    return (int)((qint64)(activeAnimationSettings()->numberOfFrames() - 1) * distance / space);
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

    AnimationSettings* settings = activeAnimationSettings();
    if(!settings) return;

    int newPos;
    int thumbSize = thumbWidth();

    if(_dragPos < 0)
        newPos = ViewportInputMode::getMousePosition(event).x() - thumbSize / 2;
    else
        newPos = ViewportInputMode::getMousePosition(event).x() - _dragPos;

    int rectWidth = frameRect().width() - 2*frameWidth();
    int newFrame = ((qint64)newPos * (qint64)(settings->numberOfFrames()) / (qint64)(rectWidth - thumbSize)) + settings->firstFrame();

    // Clamp new frame to animation interval.
    newFrame = std::clamp(newFrame, settings->firstFrame(), settings->lastFrame());

    if(_dragPos >= 0) {

        if(newFrame == settings->currentFrame())
            return;

        handleExceptions([&] {
            // Set new time.
            settings->setCurrentFrame(newFrame);
        });

        update();
    }
    if(!settings->isSingleFrame()) {
        if(_dragPos >= 0 || thumbRectangle().contains(event->pos()) == false) {
            FloatType fraction = (FloatType)(newFrame - settings->firstFrame())
                    / std::max(1, settings->numberOfFrames() - 1);
            QRect clientRect = frameRect();
            clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
            int clientWidth = clientRect.width() - thumbWidth();
            QPoint pos(clientRect.x() + (int)(fraction * clientWidth) + thumbWidth() / 2, clientRect.height() / 2);

            // Determine the text to be displayed by the tooltip.
            const auto& frameLabels = settings->frameLabels();
            auto iter = frameLabels.upperBound(newFrame);
            if(iter == frameLabels.end() || iter.key() != newFrame) {
                if(iter != frameLabels.begin())
                    --iter;  // Get the previous frame if the current one is not named.
                else
                    iter = frameLabels.end();
            }
            QString tooltipText;
            if(iter != frameLabels.end()) {
                if(frameLabels.size() == settings->numberOfFrames() && iter.value().type == AnimationFrameLabel::LabelType::FilenameAndFrame) {
                    // Tooltip text format: <Filename> (<Frame>)
                    tooltipText = iter.value().toDisplayString();
                }
                else {
                    const QString frameName = iter.value().toDisplayString();
                    if(!frameName.isEmpty()) {
                        if(settings->preferSimulationTimeDisplay()) {
                            // Tooltip text format: <Label> - <Frame>
                            tooltipText = QStringLiteral("%1 - Frame %2").arg(frameName).arg(newFrame);
                        }
                        else {
                            // Tooltip text format: <Frame> - <Label>
                            tooltipText = QStringLiteral("%1 - %2").arg(newFrame).arg(frameName);
                        }
                    }
                }
            }
            if(tooltipText.isEmpty() && _dragPos < 0)
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
    const AnimationSettings* settings = activeAnimationSettings();
    int standardWidth = 70;

    // Expand the thumb width for animations with a large number of frames.
    if(settings) {
        int maxWidth = 0;
        if(settings->preferSimulationTimeDisplay()) {
            // Compute the maximum width of a label.
            const auto& metrics = fontMetrics();
            // When showing named frames, consider a few more frames in addition to the
            // last one - to account for the fact that the length of the labels may not
            // be monotonically increasing in this case.
            const auto& frameLabels = settings->frameLabels();
            if(!frameLabels.empty()) {
                for(int frame = std::max(settings->firstFrame(), settings->lastFrame() - 5); frame <= settings->lastFrame(); ++frame) {
                    maxWidth = std::max(maxWidth, metrics.horizontalAdvance(thumbText(frame)));
                }
            }
        }

        if(maxWidth != 0) {
            standardWidth = std::max(standardWidth, maxWidth + 20);
        }
        else {
            int nframes = settings->numberOfFrames();
            if(nframes > 1)
                standardWidth += 10 * (int)std::log10(nframes);
        }
    }
    int clientWidth = frameRect().width() - 2 * frameWidth();
    return std::min(clientWidth / 2, standardWidth);
}

/******************************************************************************
* Computes the coordinates of the slider thumb.
******************************************************************************/
QRect AnimationTimeSlider::thumbRectangle()
{
    const AnimationSettings* settings = activeAnimationSettings();
    if(!settings)
        return QRect(0,0,0,0);

    int value = qBound(settings->firstFrame(), settings->currentFrame(), settings->lastFrame());
    FloatType fraction = (FloatType)(value - settings->firstFrame()) / std::max(1, settings->numberOfFrames() - 1);

    QRect clientRect = frameRect();
    clientRect.adjust(frameWidth(), frameWidth(), -frameWidth(), -frameWidth());
    int thumbSize = thumbWidth();
    int thumbPos = (int)((clientRect.width() - thumbSize) * fraction);
    return QRect(thumbPos + clientRect.x(), clientRect.y(), thumbSize, clientRect.height());
}

/******************************************************************************
* Computes the text to display on the thumb widget.
******************************************************************************/
QString AnimationTimeSlider::thumbText(int frame) const
{
    const AnimationSettings* settings = activeAnimationSettings();
    if(settings && settings->preferSimulationTimeDisplay()) {
        const auto& frameLabels = settings->frameLabels();
        auto iter = frameLabels.upperBound(frame);
        if(iter == frameLabels.end() || iter.key() != frame) {
            if(iter != frameLabels.begin())
                --iter;  // Get the previous frame if the current one is not named.
            else
                iter = frameLabels.end();
        }
        if(iter != frameLabels.end()) {
            const AnimationFrameLabel& label = iter.value();
            switch(label.type) {
                case AnimationFrameLabel::LabelType::Time:
                    return QString::number(label.numericLabel);
                case AnimationFrameLabel::LabelType::Timestep:
                case AnimationFrameLabel::LabelType::Index:
                    return QString::number((qlonglong)label.numericLabel);
                case AnimationFrameLabel::LabelType::String:
                    if(!label.stringLabel.isEmpty())
                        return label.stringLabel;
                default:
                    break; // Fall through
            }
        }
    }

    if(settings && settings->firstFrame() == 0)
        return QStringLiteral("%1 / %2").arg(frame).arg(settings->lastFrame());
    else
        return QString::number(frame);
}

/******************************************************************************
* Computes the text to display next to a timeline tick.
******************************************************************************/
QString AnimationTimeSlider::tickLabel(int frame) const
{
    const AnimationSettings* settings = activeAnimationSettings();
    if(settings && settings->preferSimulationTimeDisplay()) {
        const auto& frameLabels = settings->frameLabels();
        auto iter = frameLabels.upperBound(frame);
        if(iter == frameLabels.end() || iter.key() != frame) {
            if(iter != frameLabels.begin())
                --iter;  // Get the previous frame if the current one is not named.
            else
                iter = frameLabels.end();
        }
        if(iter != frameLabels.end()) {
            const AnimationFrameLabel& label = iter.value();
            switch(label.type) {
                case AnimationFrameLabel::LabelType::Time:
                    return QString::number(label.numericLabel);
                case AnimationFrameLabel::LabelType::Timestep:
                case AnimationFrameLabel::LabelType::Index:
                    return QString::number((qlonglong)label.numericLabel);
                case AnimationFrameLabel::LabelType::String:
                    if(!label.stringLabel.isEmpty())
                        return label.stringLabel;
                default:
                    break; // Fall through
            }
        }
    }

    return QString::number(frame);
}

/******************************************************************************
* Is called whenever the Auto Key mode is activated or deactivated.
******************************************************************************/
void AnimationTimeSlider::onAutoKeyModeChanged(bool active)
{
    setPalette(active ? _autoKeyModePalette : _normalPalette);
    update();
}

}   // End of namespace
