////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/UndoStack.h>
#include "FileSource.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(FileSource);
DEFINE_REFERENCE_FIELD(FileSource, importer);
DEFINE_PROPERTY_FIELD(FileSource, sourceUrls);
DEFINE_PROPERTY_FIELD(FileSource, playbackSpeedNumerator);
DEFINE_PROPERTY_FIELD(FileSource, playbackSpeedDenominator);
DEFINE_PROPERTY_FIELD(FileSource, playbackStartTime);
DEFINE_PROPERTY_FIELD(FileSource, autoGenerateFilePattern);
DEFINE_PROPERTY_FIELD(FileSource, restrictToFrame);
SET_PROPERTY_FIELD_LABEL(FileSource, importer, "File Importer");
SET_PROPERTY_FIELD_LABEL(FileSource, sourceUrls, "Source location");
SET_PROPERTY_FIELD_LABEL(FileSource, playbackSpeedNumerator, "Playback rate numerator");
SET_PROPERTY_FIELD_LABEL(FileSource, playbackSpeedDenominator, "Playback rate denominator");
SET_PROPERTY_FIELD_LABEL(FileSource, playbackStartTime, "Playback start time");
SET_PROPERTY_FIELD_LABEL(FileSource, autoGenerateFilePattern, "Auto-generate pattern");
SET_PROPERTY_FIELD_LABEL(FileSource, restrictToFrame, "Restrict to frame");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FileSource, playbackSpeedNumerator, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FileSource, playbackSpeedDenominator, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_CHANGE_EVENT(FileSource, sourceUrls, ReferenceEvent::TitleChanged);

/******************************************************************************
* Helper function that counts the number of source files the trajectory frames 
* are loaded from.
******************************************************************************/
static int countNumberOfFiles(const QVector<FileSourceImporter::Frame>& frames)
{
	int numberOfFiles = 0;
	const QUrl* previousUrl = nullptr;
	for(const FileSourceImporter::Frame& frame : frames) {
		if(!previousUrl || (const_cast<QUrl&>(frame.sourceFile).data_ptr() != const_cast<QUrl*>(previousUrl)->data_ptr() && frame.sourceFile != *previousUrl)) {
			numberOfFiles++;
			previousUrl = &frame.sourceFile;
		}
	}
	return numberOfFiles;
}

/******************************************************************************
* Constructs the object.
******************************************************************************/
FileSource::FileSource(ObjectCreationParams params) : BasePipelineSource(params),
	_playbackSpeedNumerator(1),
	_playbackSpeedDenominator(1),
	_playbackStartTime(0),
	_autoGenerateFilePattern(true),
	_restrictToFrame(-1)
{
}

/******************************************************************************
* Sets the source location for importing data.
******************************************************************************/
bool FileSource::setSource(std::vector<QUrl> sourceUrls, FileSourceImporter* importer, bool autodetectFileSequences, bool keepExistingDataCollection)
{
	// Make relative file paths absolute.
	for(QUrl& url : sourceUrls) {
		if(url.isLocalFile()) {
			QFileInfo fileInfo(url.toLocalFile());
			if(fileInfo.isRelative())
				url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
		}
	}

	if(this->sourceUrls() == sourceUrls && this->importer() == importer)
		return true;

	if(!sourceUrls.empty()) {
		QFileInfo fileInfo(sourceUrls.front().path());
		_originallySelectedFilename = fileInfo.fileName();
		if(_originallySelectedFilename.contains('*')) {
			if(dataCollectionFrame() >= 0 && dataCollectionFrame() < frames().size())
				_originallySelectedFilename = QFileInfo(frames()[dataCollectionFrame()].sourceFile.path()).fileName();
		}
	}
	else _originallySelectedFilename.clear();

	bool turnOffPatternGeneration = false;
	if(importer) {

		// If single URL is not already a wildcard pattern, generate a default pattern by
		// replacing the last sequence of numbers in the filename with a wildcard character.
		if(autoGenerateFilePattern() && sourceUrls.size() == 1 && importer->autoGenerateWildcardPattern() && !_originallySelectedFilename.contains('*')) {
			if(autodetectFileSequences) {
				int startIndex, endIndex;
				for(endIndex = _originallySelectedFilename.length() - 1; endIndex >= 0; endIndex--)
					if(_originallySelectedFilename.at(endIndex).isNumber()) break;
				if(endIndex >= 0) {
					for(startIndex = endIndex-1; startIndex >= 0; startIndex--)
						if(!_originallySelectedFilename.at(startIndex).isNumber()) break;
					QString wildcardPattern = _originallySelectedFilename.left(startIndex+1) + '*' + _originallySelectedFilename.mid(endIndex+1);
					QFileInfo fileInfo(sourceUrls.front().path());
					fileInfo.setFile(fileInfo.dir(), wildcardPattern);
					sourceUrls.front().setPath(fileInfo.filePath());
					OVITO_ASSERT(sourceUrls.front().isValid());
				}
			}
			else if(!_originallySelectedFilename.contains('*')) {
				turnOffPatternGeneration = true;
			}
		}

		if(this->sourceUrls() == sourceUrls && this->importer() == importer)
			return true;
	}

	// Make the import process reversible.
	UndoableTransaction transaction(dataset()->undoStack(), tr("Set input file"));

	// Make the call to setSource() undoable.
	class SetSourceOperation : public UndoableOperation {
	public:
		SetSourceOperation(FileSource* obj) : _obj(obj), _oldUrls(obj->sourceUrls()), _oldImporter(obj->importer()) {}
		void undo() override {
			std::vector<QUrl> urls = _obj->sourceUrls();
			OORef<FileSourceImporter> importer = _obj->importer();
			_obj->setSource(std::move(_oldUrls), _oldImporter, false);
			_oldUrls = std::move(urls);
			_oldImporter = importer;
		}
	private:
		std::vector<QUrl> _oldUrls;
		OORef<FileSourceImporter> _oldImporter;
		OORef<FileSource> _obj;
	};
	dataset()->undoStack().pushIfRecording<SetSourceOperation>(this);

	_sourceUrls.set(this, PROPERTY_FIELD(sourceUrls), std::move(sourceUrls));
	_importer.set(this, PROPERTY_FIELD(importer), importer);

	// Discard previously loaded data.
	if(!keepExistingDataCollection && !dataset()->undoStack().isUndoingOrRedoing()) {
		UndoSuspender noUndo(dataset());
		discardDataCollection();
	}
	setDataCollectionFrame(-1);

	// Trigger a reload of all frames.
	_frames.clear();
	pipelineCache().invalidate();
	notifyTargetChanged();

	// Scan the input source for animation frames.
	updateListOfFrames();

	if(turnOffPatternGeneration)
		setAutoGenerateFilePattern(false);

	// Commit all performed actions recorded on the undo stack. 
	transaction.commit();
	return true;
}

/******************************************************************************
* Scans the input source for animation frames and updates the internal list of frames.
******************************************************************************/
SharedFuture<QVector<FileSourceImporter::Frame>> FileSource::updateListOfFrames(bool refetchCurrentFile)
{
	// Remove current data file from local file cache so that it will get downloaded again in case it came from a remote location.
	if(refetchCurrentFile && dataCollectionFrame() >= 0 && dataCollectionFrame() < frames().size()) {
		Application::instance()->fileManager().removeFromCache(frames()[dataCollectionFrame()].sourceFile);
	}

	// Update the list of frames.
	SharedFuture<QVector<FileSourceImporter::Frame>> framesFuture = requestFrameList(true);

	// Display error messages to the user.
	framesFuture.finally(executor(), [](Task& task) {
		try { task.throwPossibleException(); }
		catch(const Exception& ex) { ex.reportError(); }
		catch(...) {}
	});

	// Show progress in the main window status bar.
	taskManager().registerFuture(framesFuture);

	return framesFuture;
}

/******************************************************************************
* Updates the internal list of input frames.
* Invalidates cached frames in case they did change.
******************************************************************************/
void FileSource::setListOfFrames(QVector<FileSourceImporter::Frame> frames)
{
	_framesListFuture.reset();

	// If there are too many frames, time tick values may overflow. Warn the user in this case.
	int frameLimit = TimePositiveInfinity() - std::max(0, dataset()->animationSettings()->frameToTime(playbackStartTime()));
	if(playbackSpeedDenominator() > playbackSpeedNumerator()) {
		frameLimit /= std::max(1, playbackSpeedDenominator()); 
		frameLimit *= std::max(1, playbackSpeedNumerator());
	}
	frameLimit /= dataset()->animationSettings()->ticksPerFrame(); 
	if(restrictToFrame() < 0 && frames.size() >= frameLimit) {
		qWarning() << "Warning: Number of frames in loaded trajectory exceeds the maximum supported by OVITO (" << (frameLimit-1) << " frames). "
			"Note: You can increase the limit by setting the animation frames-per-second parameter to a higher value.";
	}

	// Determine the new validity of the existing pipeline state in the cache.
	TimeInterval remainingCacheValidity = TimeInterval::infinite();

	// Invalidate all cached frames that are no longer present.
	if(frames.size() < _frames.size())
		remainingCacheValidity.intersect(TimeInterval(TimeNegativeInfinity(), sourceFrameToAnimationTime(frames.size())-1));

	// When adding additional frames to the end, the cache validity interval of the last frame must be reduced (unless we are loading for the first time).
	if(frames.size() > _frames.size() && !_frames.empty())
		remainingCacheValidity.intersect(TimeInterval(TimeNegativeInfinity(), sourceFrameToAnimationTime(_frames.size())-1));

	// Invalidate all cached frames that have changed.
	for(int frameIndex = 0; frameIndex < _frames.size() && frameIndex < frames.size(); frameIndex++) {
		if(frames[frameIndex] != _frames[frameIndex]) {
			remainingCacheValidity.intersect(TimeInterval(TimeNegativeInfinity(), sourceFrameToAnimationTime(frameIndex)-1));
		}
	}

	// Count the number of source files the trajectory frames are coming from.
	_numberOfFiles = countNumberOfFiles(frames);

	// Remember which trajectory frame the time slider is positioned at.
	FileSourceImporter::Frame previouslySelectedFrame;
	if(dataCollectionFrame() >= 0 && dataCollectionFrame() < _frames.size())
		previouslySelectedFrame = _frames[dataCollectionFrame()];

	// Replace our internal list of frames.
	_frames = std::move(frames);
	// Reset cached frame label list. It will be rebuilt upon request by the method animationFrameLabels().
	_frameLabels.clear();

	// Reduce cache validity to the range of frames that have not changed.
	pipelineCache().invalidate(remainingCacheValidity);
	notifyTargetChangedOutsideInterval(remainingCacheValidity);

	// Adjust the global animation length to match the new number of source frames.
	notifyDependents(ReferenceEvent::AnimationFramesChanged);

	if(dataCollectionFrame() < 0 && !_originallySelectedFilename.contains(QChar('*'))) {
		// Position time slider to the frame that corresponds to the file initially picked by the user
		// in the file selection dialog.
		for(int frameIndex = 0; frameIndex < _frames.size(); frameIndex++) {
			if(_frames[frameIndex].sourceFile.fileName() == _originallySelectedFilename) {
				TimePoint jumpToTime = sourceFrameToAnimationTime(frameIndex);
				AnimationSettings* animSettings = dataset()->animationSettings();
				if(animSettings->animationInterval().contains(jumpToTime))
					animSettings->setTime(jumpToTime);
				break;
			}
		}
	}
	else {
		// If trajectory frames have been inserted, reposition time slider to remain at the previously selected frame.
		if(!previouslySelectedFrame.sourceFile.isEmpty()) {
			int currentFrameIndex = animationTimeToSourceFrame(dataset()->animationSettings()->time());
			if(currentFrameIndex >= 0 && currentFrameIndex < _frames.size()) {
				if(_frames[currentFrameIndex].sourceFile != previouslySelectedFrame.sourceFile) {
					for(int frameIndex = 0; frameIndex < _frames.size(); frameIndex++) {
						if(_frames[frameIndex].sourceFile == previouslySelectedFrame.sourceFile) {
							TimePoint jumpToTime = sourceFrameToAnimationTime(frameIndex);
							AnimationSettings* animSettings = dataset()->animationSettings();
							if(animSettings->animationInterval().contains(jumpToTime))
								animSettings->setTime(jumpToTime);
							break;
						}
					}
				}
			}
		}
	}

	// Notify UI that the list of source frames has changed.
	Q_EMIT framesListChanged();
}

/******************************************************************************
* Returns the number of animation frames this pipeline object can provide.
******************************************************************************/
int FileSource::numberOfSourceFrames() const 
{ 
	if(restrictToFrame() >= 0)
		return 1;

	return _frames.size(); 
}

/******************************************************************************
* Given an animation time, computes the source frame to show.
******************************************************************************/
int FileSource::animationTimeToSourceFrame(TimePoint time) const
{
	if(restrictToFrame() >= 0)
		return restrictToFrame();

	const AnimationSettings* anim = dataset()->animationSettings();
	return (time - anim->frameToTime(playbackStartTime())) *
			std::max(1, playbackSpeedNumerator()) /
			std::max(1, playbackSpeedDenominator() * anim->ticksPerFrame());
}

/******************************************************************************
* Given a source frame index, returns the animation time at which it is shown.
******************************************************************************/
TimePoint FileSource::sourceFrameToAnimationTime(int frame) const
{
	if(restrictToFrame() >= 0)
		return 0;

	const AnimationSettings* anim = dataset()->animationSettings();
	return frame *
			std::max(1, playbackSpeedDenominator() * anim->ticksPerFrame()) /
			std::max(1, playbackSpeedNumerator()) +
			anim->frameToTime(playbackStartTime());
}

/******************************************************************************
* Returns the human-readable labels associated with the animation frames.
******************************************************************************/
QMap<int, QString> FileSource::animationFrameLabels() const
{
	// Check if the cached list of frame labels is still available.
	// If not, rebuild the list here.
	if(_frameLabels.empty() && restrictToFrame() < 0) {
		AnimationSettings* animSettings = dataset()->animationSettings();
		int frameIndex = 0;
		for(const FileSourceImporter::Frame& frame : _frames) {
			if(frame.label.isEmpty()) break;
			// Convert local source frame index to global animation frame number.
			_frameLabels.insert(animSettings->timeToFrame(FileSource::sourceFrameToAnimationTime(frameIndex)), frame.label);
			frameIndex++;
		}
	}
	return _frameLabels;
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval FileSource::validityInterval(const PipelineEvaluationRequest& request) const
{
	TimeInterval iv = BasePipelineSource::validityInterval(request);

	// Restrict the validity interval to the duration of the requested source frame.
	if(restrictToFrame() < 0 && frames().size() > 1) {
		int frame = animationTimeToSourceFrame(request.time());
		if(frame > 0)
			iv.intersect(TimeInterval(sourceFrameToAnimationTime(frame), TimePositiveInfinity()));
		if(frame < frames().size() - 1)
			iv.intersect(TimeInterval(TimeNegativeInfinity(), std::max(sourceFrameToAnimationTime(frame + 1) - 1, sourceFrameToAnimationTime(frame))));
	}

	return iv;
}

/******************************************************************************
* Asks the object for the result of the data pipeline.
******************************************************************************/
Future<PipelineFlowState> FileSource::evaluateInternal(const PipelineEvaluationRequest& request)
{
	// Convert the animation time to a frame number.
	int frame = animationTimeToSourceFrame(request.time());
	int frameCount = frames().size();

	// Clamp to frame range.
	if(frame < 0) frame = 0;
	else if(frame >= frameCount && frameCount > 0) frame = frameCount - 1;

	OVITO_ASSERT(frameTimeInterval(frame).contains(request.time()));

	// First request the list of source frames and wait until it becomes available.
	Future<PipelineFlowState> stateFuture = requestFrameList(false)
		.then(executor(), [this, frame](const QVector<FileSourceImporter::Frame>& sourceFrames) -> Future<PipelineFlowState> {

			// Is the requested frame out of range?
			if(frame >= sourceFrames.size()) {
				TimeInterval interval = TimeInterval::infinite();
				if(frame < 0)
					interval.setEnd(sourceFrameToAnimationTime(0) - 1);
				else if(frame >= sourceFrames.size() && !sourceFrames.empty())
					interval.setStart(sourceFrameToAnimationTime(sourceFrames.size()));
				return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The file source path is empty or has not been set (no files found).")), interval);
			}
			else if(frame < 0) {
				return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The requested source frame is out of range.")));
			}

			// Retrieve the file.
			Future<PipelineFlowState> loadFrameFuture = Application::instance()->fileManager().fetchUrl(sourceFrames[frame].sourceFile)
				.then(executor(), [this, frame](const FileHandle& fileHandle) -> Future<PipelineFlowState> {

					// Without an importer object we have to give up immediately.
					if(!importer()) {
						// In case of an error, just return the stale data that we have cached.
						return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The file source path has not been set.")));
					}
					if(frame >= frames().size())
						throwException(tr("Requested source frame index is out of range."));

					// Compute the validity interval of the returned pipeline state.
					TimeInterval interval = frameTimeInterval(frame);
					const FileSourceImporter::Frame& frameInfo = frames()[frame];

					// Set up the load request to be submitted to the FileSourceImporter.
					FileSourceImporter::LoadOperationRequest loadRequest;
					loadRequest.dataset = dataset();
					loadRequest.dataSource = this;
					loadRequest.fileHandle = fileHandle;
					loadRequest.frame = frameInfo;
					loadRequest.isNewlyImportedFile = (dataCollection() == nullptr);
					loadRequest.state.setData(dataCollection()
						? DataOORef<const DataCollection>(dataCollection()) 
						: DataOORef<const DataCollection>::create(dataset()));

					// Add some standard global attributes to the pipeline state to indicate where it is coming from.
					loadRequest.state.setAttribute(QStringLiteral("SourceFrame"), frame, this);
					loadRequest.state.setAttribute(QStringLiteral("SourceFile"), frameInfo.sourceFile.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded), this);

					// Also give the state the precomputed validity interval.
					loadRequest.state.setStateValidity(interval);

					// Load the frame data and return results to the caller.
					return importer()->loadFrame(loadRequest);
				});

			// Change activity status during long-running load operations.
			registerActiveFuture(loadFrameFuture);

			return loadFrameFuture;
		});

	// Post-process the results of the loading operation before returning them to the caller.
	return postprocessDataCollection(frame, frameTimeInterval(frame), std::move(stateFuture));
}

/******************************************************************************
* Scans the external data file and returns the list of discovered input frames.
******************************************************************************/
SharedFuture<QVector<FileSourceImporter::Frame>> FileSource::requestFrameList(bool forceRescan)
{
	// Without an importer object the list of frames is empty.
	if(!importer())
		return Future<QVector<FileSourceImporter::Frame>>::createImmediateEmplace();

	// Return the active future when the frame loading process is currently in progress.
	if(_framesListFuture.isValid()) {
		if(!forceRescan || !_framesListFuture.isFinished())
			return _framesListFuture;
		_framesListFuture.reset();
	}

	// Return the cached frames list if available.
	if(!_frames.empty() && !forceRescan) {
		return _frames;
	}

	// Forward request to the importer object.
	// Intercept future results when they become available and cache them.
	_framesListFuture = importer()->discoverFrames(sourceUrls())
		// Note that execution of the following continuation function is explicitly deferred,
		// because setListOfFrames() generates a TargetChanged event, which is not allowed during
		// a synchronous call to the pipeline evaulation function.
		.then(executor(true), [this](QVector<FileSourceImporter::Frame>&& frameList) {
			// Store the new list of frames in the FileSource. 
			setListOfFrames(frameList);
			// Pass the frame list on to the caller.
			return std::move(frameList);
		});

	// Has loading the frames list already completed?
	// If yes, reset the future before returning from this function.
	if(_framesListFuture.isFinished())
		return std::move(_framesListFuture);

	// The status of this pipeline object changes while loading is in progress.
	registerActiveFuture(_framesListFuture);

	return _framesListFuture;
}

/******************************************************************************
* Computes the time interval covered on the timeline by the given source animation frame.
******************************************************************************/
TimeInterval FileSource::frameTimeInterval(int frame) const
{
	OVITO_ASSERT(frame >= 0);
	TimeInterval interval = TimeInterval::infinite();
	if(restrictToFrame() < 0) {
		if(frame > 0)
			interval.setStart(sourceFrameToAnimationTime(frame));
		if(frame < frames().size() - 1)
			interval.setEnd(std::max(sourceFrameToAnimationTime(frame + 1) - 1, sourceFrameToAnimationTime(frame)));
	}
	OVITO_ASSERT(!interval.isEmpty());
	OVITO_ASSERT(interval.contains(sourceFrameToAnimationTime(frame)));
	return interval;
}

/******************************************************************************
* This will trigger a reload of an animation frame upon next request.
******************************************************************************/
void FileSource::reloadFrame(bool refetchFiles, int frameIndex)
{
	if(!importer())
		return;

	// Remove source files from file cache so that they will be downloaded again
	// if they came from a remote location.
	if(refetchFiles) {
		if(frameIndex >= 0 && frameIndex < frames().size()) {
			Application::instance()->fileManager().removeFromCache(frames()[frameIndex].sourceFile);
		}
		else if(frameIndex == -1) {
			for(const FileSourceImporter::Frame& frame : frames())
				Application::instance()->fileManager().removeFromCache(frame.sourceFile);
		}
	}

	// Determine the animation time interval for which the pipeline needs to be updated.
	// When updating a single frame, we can preserve all frames up to the invalidated one.
	TimeInterval unchangedInterval = TimeInterval::empty();
	if(frameIndex > 0 && restrictToFrame() < 0)
		unchangedInterval = TimeInterval(TimeNegativeInfinity(), frameTimeInterval(frameIndex-1).end());
	
	// Throw away cached frame data and notify pipeline that an update is in order.
	pipelineCache().invalidate(unchangedInterval);
	notifyTargetChangedOutsideInterval(unchangedInterval);
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void FileSource::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
	BasePipelineSource::saveToStream(stream, excludeRecomputableData);
	stream.beginChunk(0x03);
	stream << _frames;
	stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void FileSource::loadFromStream(ObjectLoadStream& stream)
{
	BasePipelineSource::loadFromStream(stream);
	stream.expectChunk(0x03);
	stream >> _frames;
	stream.closeChunk();

	// Count the number of source files the trajectory frames are coming from.
	_numberOfFiles = countNumberOfFiles(_frames);
}

/******************************************************************************
* Returns the title of this object.
******************************************************************************/
QString FileSource::objectTitle() const
{
	QString filename;
	int frameIndex = dataCollectionFrame();
	if(frameIndex >= 0 && frameIndex < frames().size()) {
		filename = frames()[frameIndex].sourceFile.fileName();
	}
	else if(!sourceUrls().empty()) {
		filename = sourceUrls().front().fileName();
	}
	if(importer())
		return QString("%2 [%1]").arg(importer()->objectTitle()).arg(filename);
	return BasePipelineSource::objectTitle();
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void FileSource::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(playbackSpeedNumerator) ||
			field == PROPERTY_FIELD(playbackSpeedDenominator) ||
			field == PROPERTY_FIELD(playbackStartTime)) {
	
		// Clear frame label list. It will be regnerated upon request in animationFrameLabels().
		_frameLabels.clear(); 

		// Invalidate cached frames, because their validity intervals have changed.
		int unchangedFromFrame = std::min(playbackStartTime(), 0);
		int unchangedToFrame = std::max(playbackStartTime(), 0);
		TimeInterval unchangedInterval = (field == PROPERTY_FIELD(playbackStartTime)) ?
			TimeInterval::empty() : 
			TimeInterval(sourceFrameToAnimationTime(playbackStartTime()));
		pipelineCache().invalidate(unchangedInterval);

		// Inform animation system that global time line length probably changed.
		notifyDependents(ReferenceEvent::AnimationFramesChanged);
	}
	else if(field == PROPERTY_FIELD(autoGenerateFilePattern)) {
		if(!isBeingLoaded()) {
			if(!autoGenerateFilePattern())
				removeWildcardFilePattern();
			else
				generateWildcardFilePattern();
		}
	}
	else if(field == PROPERTY_FIELD(restrictToFrame)) {
		// Invalidate cached frames, because their validity intervals have changed.
		pipelineCache().invalidate();

		// Inform animation system that global time line length probably changed.
		notifyDependents(ReferenceEvent::AnimationFramesChanged);
	}
	else if(field == PROPERTY_FIELD(sourceUrls)) {
		Q_EMIT currentFileChanged();
	}
	else if(field == PROPERTY_FIELD(BasePipelineSource::dataCollectionFrame)) {
		// The active frame is part of the source's UI title.	
		if(numberOfFiles() > 1)
			notifyDependents(ReferenceEvent::TitleChanged);
		Q_EMIT currentFileChanged();
	}
	BasePipelineSource::propertyChanged(field);
}

/******************************************************************************
* If the file source currently uses a wildcard search pattern, replaces it
* with a single concrete filename. 
******************************************************************************/
void FileSource::removeWildcardFilePattern()
{
	for(const QUrl& url : sourceUrls()) {
		if(FileSourceImporter::isWildcardPattern(url)) {
			if(dataCollectionFrame() >= 0 && dataCollectionFrame() < frames().size()) {
				QUrl currentUrl = frames()[dataCollectionFrame()].sourceFile;
				if(currentUrl != url) {
					setSource({currentUrl}, importer(), false);
					return;
				}
			}
		}
	}
}

/******************************************************************************
* Generates a wildcard file seach pattern unless the file source already uses a pattern. 
******************************************************************************/
void FileSource::generateWildcardFilePattern()
{
	if(sourceUrls().size() == 1) {
		const QUrl& url = sourceUrls().front();
		if(!FileSourceImporter::isWildcardPattern(url)) {
			
			QString filename = url.fileName();
			int startIndex, endIndex;
			for(endIndex = filename.length() - 1; endIndex >= 0; endIndex--)
				if(filename.at(endIndex).isNumber()) break;

			if(endIndex >= 0) {
				for(startIndex = endIndex-1; startIndex >= 0; startIndex--)
					if(!filename.at(startIndex).isNumber()) break;

				QString wildcardPattern = filename.left(startIndex + 1) + '*' + filename.mid(endIndex + 1);
				QFileInfo fileInfo(url.path());
				fileInfo.setFile(fileInfo.dir(), wildcardPattern);
				QUrl newUrl = url;
				newUrl.setPath(fileInfo.filePath());
				OVITO_ASSERT(newUrl.isValid());

				setSource({newUrl}, importer(), true);
			}	
		}
	}
}

/******************************************************************************
* Returns the name of the file loaded by the file source for the current animation frame.
* The filename is displayed in the UI panel of the file source. 
******************************************************************************/
QString FileSource::currentFileName() const
{
	if(dataCollectionFrame() >= 0 && dataCollectionFrame() < frames().size()) {
		const FileSourceImporter::Frame& frameInfo = frames()[dataCollectionFrame()];
		if(frameInfo.sourceFile.isLocalFile()) {
			return QFileInfo(frameInfo.sourceFile.toLocalFile()).fileName();
		}
		else {
			return QFileInfo(frameInfo.sourceFile.path()).fileName();
		}
	}
	return {};
}

/******************************************************************************
* Returns the directory path from which the current animation frame was loaded.
* The path is displayed in the UI panel of the FileSource. 
******************************************************************************/
QString FileSource::currentDirectoryPath() const
{
	if(!sourceUrls().empty()) {
		if(sourceUrls().front().isLocalFile()) {
			QFileInfo fileInfo(sourceUrls().front().toLocalFile());
			return fileInfo.dir().path();
		}
		else {
			QFileInfo fileInfo(sourceUrls().front().path());
			QUrl url = sourceUrls().front();
			url.setPath(fileInfo.path());
			return url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded);
		}
	}
	return {};
}

}	// End of namespace
