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
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "AnimationSettings.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(AnimationSettings);
DEFINE_PROPERTY_FIELD(AnimationSettings, currentFrame);
DEFINE_PROPERTY_FIELD(AnimationSettings, firstFrame);
DEFINE_PROPERTY_FIELD(AnimationSettings, lastFrame);
DEFINE_PROPERTY_FIELD(AnimationSettings, framesPerSecond);
DEFINE_PROPERTY_FIELD(AnimationSettings, playbackSpeed);
DEFINE_PROPERTY_FIELD(AnimationSettings, loopPlayback);
DEFINE_PROPERTY_FIELD(AnimationSettings, playbackEveryNthFrame);
DEFINE_PROPERTY_FIELD(AnimationSettings, autoAdjustInterval);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(AnimationSettings, playbackEveryNthFrame, IntegerParameterUnit, 1);

/******************************************************************************
* Constructor.
******************************************************************************/
AnimationSettings::AnimationSettings(ObjectCreationParams params) : RefTarget(params),
		_framesPerSecond(10),
		_playbackSpeed(1),
		_firstFrame(0),
		_lastFrame(0),
		_currentFrame(0),
		_loopPlayback(true),
		_playbackEveryNthFrame(1),
		_autoAdjustInterval(true)
{
}

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void AnimationSettings::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(currentFrame))
		Q_EMIT currentFrameChanged(currentFrame());
	else if(field == PROPERTY_FIELD(firstFrame) || field == PROPERTY_FIELD(lastFrame))
		Q_EMIT intervalChanged(firstFrame(), lastFrame());
	else if(field == PROPERTY_FIELD(framesPerSecond))
		Q_EMIT speedChanged();
	else if(field == PROPERTY_FIELD(autoAdjustInterval) && autoAdjustInterval() && !isBeingLoaded())
		adjustAnimationInterval();

	RefTarget::propertyChanged(field);
}

/******************************************************************************
* Saves the class' contents to an output stream.
******************************************************************************/
void AnimationSettings::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
	RefTarget::saveToStream(stream, excludeRecomputableData);
	stream.beginChunk(0x01);
	stream << _namedFrames;
	stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from an input stream.
******************************************************************************/
void AnimationSettings::loadFromStream(ObjectLoadStream& stream)
{
	RefTarget::loadFromStream(stream);
	stream.expectChunk(0x01);
	stream >> _namedFrames;
	stream.closeChunk();
}

/******************************************************************************
* Creates a copy of this object.
******************************************************************************/
OORef<RefTarget> AnimationSettings::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
	// Let the base class create an instance of this class.
	OORef<AnimationSettings> clone = static_object_cast<AnimationSettings>(RefTarget::clone(deepCopy, cloneHelper));

	// Copy internal data.
	clone->_namedFrames = this->_namedFrames;

	return clone;
}

/******************************************************************************
* Converts a time value to its string representation.
******************************************************************************/
QString AnimationSettings::timeToString(AnimationTime time)
{
	return QString::number(time.frame());
}

/******************************************************************************
* Converts a string to a time value.
* Throws an exception when a parsing error occurs.
******************************************************************************/
AnimationTime AnimationSettings::stringToTime(const QString& stringValue)
{
	bool ok;
	int frame = stringValue.toInt(&ok);
	if(!ok)
		throw Exception(tr("Invalid frame number format: %1").arg(stringValue));
	return AnimationTime::fromFrame(frame);
}

/******************************************************************************
* Sets the current animation time to the start of the animation interval.
******************************************************************************/
void AnimationSettings::jumpToAnimationStart()
{
	setCurrentFrame(firstFrame());
}

/******************************************************************************
* Sets the current animation time to the end of the animation interval.
******************************************************************************/
void AnimationSettings::jumpToAnimationEnd()
{
	setCurrentFrame(lastFrame());
}

/******************************************************************************
* Jumps to the previous animation frame.
******************************************************************************/
void AnimationSettings::jumpToPreviousFrame()
{
	setCurrentFrame(std::max(currentFrame() - 1, firstFrame()));
}

/******************************************************************************
* Jumps to the previous animation frame.
******************************************************************************/
void AnimationSettings::jumpToNextFrame()
{
	setCurrentFrame(std::min(currentFrame() + 1, lastFrame()));
}

/******************************************************************************
* Recalculates the length of the animation interval to accommodate all loaded
* source animations in the scene.
******************************************************************************/
void AnimationSettings::adjustAnimationInterval()
{
	int firstFrame = std::numeric_limits<int>::max();
	int lastFrame = std::numeric_limits<int>::lowest();
	_namedFrames.clear();

	// Visit all scenes that reference this animation settings object.
	visitDependents([&](RefMaker* dependent) {
		if(Scene* scene = dynamic_object_cast<Scene>(dependent)) {
			scene->visitObjectNodes([&](PipelineSceneNode* node) {
				if(node->dataProvider()) {
					int nframes = node->dataProvider()->numberOfSourceFrames();
					if(nframes > 0) {

						// Final animation interval should encompass the local intervals
						// of all animated objects in the scene.
						int start = node->dataProvider()->sourceFrameToAnimationTime(0).frame();
						if(start < firstFrame) firstFrame = start;
						int end = node->dataProvider()->sourceFrameToAnimationTime(nframes - 1).frame();
						if(end > lastFrame) lastFrame = end;

						// Save the list of the named animation frames.
						// Merge with other list(s) from other scene objects if there are any.
						if(_namedFrames.empty())
							_namedFrames = node->dataProvider()->animationFrameLabels();
						else {
							auto additionalLabels = node->dataProvider()->animationFrameLabels();
							if(!additionalLabels.empty())
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
								_namedFrames.insert(additionalLabels);
#else
								_namedFrames.unite(additionalLabels);
#endif
						}
					}
				}
				return true;
			});
		}
	});
	if(firstFrame > lastFrame)
		firstFrame = lastFrame = 0;
	setFirstFrame(firstFrame);
	setLastFrame(lastFrame);
	setCurrentFrame(qBound(firstFrame, currentFrame(), lastFrame));
}

}	// End of namespace
