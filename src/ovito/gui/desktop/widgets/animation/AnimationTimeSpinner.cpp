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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include "AnimationTimeSpinner.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AnimationTimeSpinner::AnimationTimeSpinner(MainWindowUI& ui, QWidget* parent) : SpinnerWidget(parent), UserInterfaceComponent<MainWindowUI>(ui)
{
    setUnit(new AnimationTimeSpinnerUnit(this, ui));
    connect(this, &SpinnerWidget::valueChanged, this, &AnimationTimeSpinner::onSpinnerValueChanged);
    connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &AnimationTimeSpinner::onCurrentFrameChanged);
    connect(&datasetContainer(), &DataSetContainer::animationIntervalChanged, this, &AnimationTimeSpinner::onIntervalChanged);
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
    if(AnimationSettings* anim = activeAnimationSettings()) {
        handleExceptions([&]() {
            anim->setCurrentFrame(intValue());
        });
    }
}

/******************************************************************************
* Constructor.
******************************************************************************/
AnimationTimeSpinnerUnit::AnimationTimeSpinnerUnit(QObject* parent, MainWindowUI& ui) : IntegerParameterUnit(parent), UserInterfaceComponent<MainWindowUI>(ui)
{
    connect(&datasetContainer(), &DataSetContainer::timeFormatChanged, this, &AnimationTimeSpinnerUnit::formatChanged);
    connect(&datasetContainer(), &DataSetContainer::animationIntervalChanged, this, &AnimationTimeSpinnerUnit::formatChanged);
}

/******************************************************************************
* Converts a numeric value to a string.
******************************************************************************/
QString AnimationTimeSpinnerUnit::formatValue(FloatType value)
{
    int frame = static_cast<int>(value);

    if(const AnimationSettings* settings = activeAnimationSettings()) {
        if(settings->preferSimulationTimeDisplay()) {
            const auto& frameLabels = settings->frameLabels();
            auto iter = frameLabels.upperBound(frame);
            if(iter == frameLabels.end() || iter.key() != frame) {
                if(iter != frameLabels.begin())
                    --iter;  // Get the previous frame if the current one is not named.
                else
                    iter = frameLabels.end();
            }
            if(iter != frameLabels.end()) {
                auto labelType = iter.value().type;
                if(labelType == AnimationFrameLabel::LabelType::Time || labelType == AnimationFrameLabel::LabelType::Timestep || labelType == AnimationFrameLabel::LabelType::String) {
                    QString frameName = iter.value().toDisplayString();
                    if(!frameName.isEmpty())
                        return frameName;
                }
            }
        }
    }

    return QString::number(frame);
}

/******************************************************************************
* Converts the given string to a value.
******************************************************************************/
FloatType AnimationTimeSpinnerUnit::parseString(const QString& valueString)
{
    const AnimationSettings* settings = activeAnimationSettings();
    // Try reverse lookup from frame name to frame number.
    if(settings && settings->preferSimulationTimeDisplay()) {
        const auto& frameLabels = settings->frameLabels();
        if(!frameLabels.empty()) {
            const QString searchString = valueString.trimmed();
            for(auto item = frameLabels.cbegin(), end = frameLabels.cend(); item != end; ++item) {
                switch(item.value().type) {
                    case AnimationFrameLabel::LabelType::Time:
                    case AnimationFrameLabel::LabelType::Timestep:
                        if(QString::number(item.value().numericLabel) == searchString || item.value().toDisplayString() == searchString)
                            return item.key();
                        break;
                    case AnimationFrameLabel::LabelType::String:
                        if(item.value().stringLabel == searchString)
                            return item.key();
                        break;
                    default:
                        break;
                }
            }
        }
    }

    int value;
    bool ok;
    value = valueString.toInt(&ok);
    if(!ok) {
        if(!settings || !settings->preferSimulationTimeDisplay())
            throw Exception(tr("Invalid integer value: %1").arg(valueString));
        else
            throw Exception(tr("Unknown frame: %1").arg(valueString));
    }
    return (FloatType)value;
}

}   // End of namespace
