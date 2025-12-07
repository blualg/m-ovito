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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include "FileSource.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FileSource);
OVITO_CLASSINFO(FileSource, "DisplayName", "External file source");
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
* Sets the source location for importing data.
******************************************************************************/
void FileSource::setSource(std::vector<QUrl> sourceUrls, OORef<FileSourceImporter> importer, bool autodetectFileSequences, bool keepExistingDataCollection)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::isMainThread());

    // Make relative file paths absolute.
    for(QUrl& url : sourceUrls) {
        if(url.isLocalFile()) {
            QFileInfo fileInfo(url.toLocalFile());
            if(fileInfo.isRelative())
                url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        }
    }

    if(this->sourceUrls() == sourceUrls && this->importer() == importer)
        return;

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
        if(autoGenerateFilePattern() && sourceUrls.size() == 1 && importer->autoGenerateWildcardPattern() && !FileSourceImporter::isWildcardPattern(_originallySelectedFilename)) {
            if(autodetectFileSequences) {
                QString wildcardPattern = FileSourceImporter::deriveWildcardPatternFromFilename(_originallySelectedFilename);
                if(!wildcardPattern.isEmpty()) {
                    QFileInfo fileInfo(sourceUrls.front().path());
                    fileInfo.setFile(fileInfo.dir(), wildcardPattern);
                    sourceUrls.front().setPath(fileInfo.filePath());
                    OVITO_ASSERT(sourceUrls.front().isValid());
                }
            }
            else {
                turnOffPatternGeneration = true;
            }
        }

        if(this->sourceUrls() == sourceUrls && this->importer() == importer)
            return;
    }

    // Make the call to setSource() undoable.
    class SetSourceOperation : public UndoableOperation {
    public:
        SetSourceOperation(FileSource* obj) : _obj(obj), _oldUrls(obj->sourceUrls()), _oldImporter(obj->importer()) {}
        void undo() override {
            _obj->setSource(
                std::exchange(_oldUrls, _obj->sourceUrls()),
                std::exchange(_oldImporter, _obj->importer()),
                false);
        }
    private:
        std::vector<QUrl> _oldUrls;
        OORef<FileSourceImporter> _oldImporter;
        OORef<FileSource> _obj;
    };
    pushIfUndoRecording<SetSourceOperation>(this);

    _sourceUrls.set(this, PROPERTY_FIELD(sourceUrls), std::move(sourceUrls));
    _importer.set(this, PROPERTY_FIELD(importer), std::move(importer));

    // Discard previously loaded data.
    if(!keepExistingDataCollection && !isUndoingOrRedoing()) {
        UndoSuspender noUndo;
        discardDataCollection();
    }
    setDataCollectionFrame(-1);

    // Trigger a reload of all frames.
    _frames.clear();
    _framesValid = false;
    pipelineCache().invalidate();
    notifyTargetChanged();

    // Scan the input source for animation frames.
    updateListOfFrames();

    if(turnOffPatternGeneration)
        setAutoGenerateFilePattern(false);
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

    // Display any errors that occurred during scan operation to the user.
    framesFuture.finally(ObjectExecutor(this), [ui=this_task::ui()](Task& task) noexcept {
        try { if(!task.isCanceled()) task.throwPossibleException(); }
        catch(const Exception& ex) { ui->reportError(ex); }
        catch(...) {}
    });

    return framesFuture;
}

/******************************************************************************
* Updates the internal list of input frames.
* Invalidates cached frames in case they did change.
******************************************************************************/
void FileSource::setListOfFrames(QVector<FileSourceImporter::Frame> frames)
{
    // Determine the new validity of the existing pipeline state in the cache.
    TimeInterval remainingCacheValidity = TimeInterval::infinite();

    // Invalidate all cached frames that are no longer present.
    if(frames.size() < _frames.size())
        remainingCacheValidity.intersect(TimeInterval(AnimationTime::negativeInfinity(), sourceFrameToAnimationTime(frames.size())-1));

    // When adding additional frames to the end, the cache validity interval of the last frame must be reduced (unless we are loading for the first time).
    if(frames.size() > _frames.size() && !_frames.empty())
        remainingCacheValidity.intersect(TimeInterval(AnimationTime::negativeInfinity(), sourceFrameToAnimationTime(_frames.size())-1));

    // Invalidate all cached frames that have changed.
    for(int frameIndex = 0; frameIndex < _frames.size() && frameIndex < frames.size(); frameIndex++) {
        if(frames[frameIndex] != _frames[frameIndex]) {
            remainingCacheValidity.intersect(TimeInterval(AnimationTime::negativeInfinity(), sourceFrameToAnimationTime(frameIndex)-1));
        }
    }

    // Make sure the frame data can be serialized to a state file.
    OVITO_ASSERT(boost::algorithm::all_of(frames, [](const auto& frame) {
        return !frame.parserData.metaType().isValid() || frame.parserData.metaType().hasRegisteredDataStreamOperators();
    }));

    // Count the number of source files the trajectory frames are coming from.
    _numberOfFiles = countNumberOfFiles(frames);

    // Remember which trajectory frame the time slider is positioned at.
    FileSourceImporter::Frame previouslySelectedFrame;
    if(dataCollectionFrame() >= 0 && dataCollectionFrame() < _frames.size())
        previouslySelectedFrame = _frames[dataCollectionFrame()];

    // Replace our internal list of frames.
    _frames = std::move(frames);
    _framesValid = true;
    // Reset cached frame label list. It will be rebuilt upon request by the method animationFrameLabels().
    _frameLabels.clear();

    // Reduce cache validity to the range of frames that have not changed.
    pipelineCache().invalidate(remainingCacheValidity);
    notifyTargetChangedOutsideInterval(remainingCacheValidity);

    // Adjust the global animation length to match the new number of source frames.
    notifyDependents(ReferenceEvent::AnimationFramesChanged);
    // The active frame is part of the source's UI title.
    notifyDependents(ReferenceEvent::TitleChanged);

    if(this_task::isInteractive()) {
        if(dataCollectionFrame() < 0 && !_originallySelectedFilename.contains(QChar('*'))) {
            // Position time slider to the frame corresponding to the file that was initially picked by the user
            // in the file selection dialog.
            for(int frameIndex = 0; frameIndex < _frames.size(); frameIndex++) {
                if(_frames[frameIndex].sourceFile.fileName() == _originallySelectedFilename) {
                    AnimationTime jumpToTime = sourceFrameToAnimationTime(frameIndex);
                    notifyDependentsImpl(RequestGoToAnimationTimeEvent(this, jumpToTime));
                    break;
                }
            }
        }
        else {
            // If trajectory frames have been inserted, reposition time slider to remain at the previously selected frame.
            if(!previouslySelectedFrame.sourceFile.isEmpty()) {
                int currentFrameIndex = animationTimeToSourceFrame(this_task::ui()->datasetContainer().currentAnimationTime());
                if(currentFrameIndex >= 0 && currentFrameIndex < _frames.size()) {
                    if(_frames[currentFrameIndex].sourceFile != previouslySelectedFrame.sourceFile) {
                        for(int frameIndex = 0; frameIndex < _frames.size(); frameIndex++) {
                            if(_frames[frameIndex].sourceFile == previouslySelectedFrame.sourceFile) {
                                AnimationTime jumpToTime = sourceFrameToAnimationTime(frameIndex);
                                notifyDependentsImpl(RequestGoToAnimationTimeEvent(this, jumpToTime));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
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
int FileSource::animationTimeToSourceFrame(AnimationTime time) const
{
    if(restrictToFrame() >= 0)
        return restrictToFrame();

    return (time - AnimationTime::fromFrame(playbackStartTime())) *
            std::max(1, playbackSpeedNumerator()) /
            std::max(1, playbackSpeedDenominator() * (int)AnimationTime::TicksPerFrame);
}

/******************************************************************************
* Given a source frame index, returns the animation time at which it is shown.
******************************************************************************/
AnimationTime FileSource::sourceFrameToAnimationTime(int frame) const
{
    if(restrictToFrame() >= 0)
        return AnimationTime(0);

    return AnimationTime::fromFrame(playbackStartTime()) + AnimationTime::fromFrame(frame).ticks() *
            std::max(1, playbackSpeedDenominator()) /
            std::max(1, playbackSpeedNumerator());
}

/******************************************************************************
* Returns the human-readable labels associated with the animation frames.
******************************************************************************/
QMap<int, AnimationFrameLabel> FileSource::animationFrameLabels() const
{
    // Check if the cached list of frame labels is still available.
    // If not, rebuild the list here.
    if(_frameLabels.empty() && restrictToFrame() < 0) {
        int frameIndex = 0;
        for(const FileSourceImporter::Frame& frame : _frames) {
            if(frame.label.type == AnimationFrameLabel::LabelType::None)
                break;
            // Convert local source frame index to global animation frame number.
            _frameLabels.insert(FileSource::sourceFrameToAnimationTime(frameIndex).frame(), frame.label);
            frameIndex++;
        }
    }
    return _frameLabels;
}

/******************************************************************************
 * This function is called by the pipeline system before a new modifier evaluation
 * begins to query the validity interval and evaluation result type for this pipeline stage.
 ******************************************************************************/
void FileSource::preevaluateInternal(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval)
{
    if(request.interactiveMode()) {
        // In interactive mode, there is not enough time to wait for file I/O. Therefore, we'll
        // return the current internal state to serve the interactive evaluation request immediately.
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    }
    else {
        // Marking the pipeline evaluation results as "non-interactive", because the loading process takes a longer time.
        // We don't want interactive requests to wait on it.
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;

        // Compute validity interval of the requested trajectory frame.
        int frame = animationTimeToSourceFrame(request.time());
        int frameCount = frames().size();
        if(restrictToFrame() < 0 && frameCount > 1) {
            if(frame > 0)
                validityInterval.intersect(TimeInterval(sourceFrameToAnimationTime(frame), AnimationTime::positiveInfinity()));
            if(frame < frameCount - 1)
                validityInterval.intersect(TimeInterval(AnimationTime::negativeInfinity(), std::max(sourceFrameToAnimationTime(frame + 1) - 1, sourceFrameToAnimationTime(frame))));
        }
        else if(frameCount == 0) {
            // We haven't loaded the frames list yet. In this case, we need to be conservative, because the validity interval of the frame is likely going to change.
            validityInterval.intersect(sourceFrameToAnimationTime(frame));
        }
    }
    OVITO_ASSERT(validityInterval.contains(request.time()));
}

/******************************************************************************
* Asks the object for the result of the data pipeline.
******************************************************************************/
SharedFuture<PipelineFlowState> FileSource::evaluateInternal(const PipelineEvaluationRequest& request)
{
    if(request.interactiveMode()) {
        // In interactive mode, there is not enough time to wait for file I/O. Therefore, directly
        // return the current internal state to serve the evaluation request immediately.
        return PipelineFlowState(getSourceDataCollection(), status());
    }

    // Convert the animation time to a frame number.
    int frame = animationTimeToSourceFrame(request.time());
    int frameCount = frames().size();

    // Clamp to frame range.
    if(frame < 0)
        frame = 0;
    else if(frame >= frameCount && frameCount > 0)
        frame = frameCount - 1;

    // First, request the list of trajectory frames.
    SharedFuture<QVector<FileSourceImporter::Frame>> framesListFuture = requestFrameList(false);

    // Then continue by loading the file that contains the requested frame.
    Future<PipelineFlowState> stateFuture = framesListFuture.then(ObjectExecutor(this), [this, frame](const QVector<FileSourceImporter::Frame>& sourceFrames) -> Future<PipelineFlowState> {

        // Is the requested frame number out of range?
        if(frame >= sourceFrames.size()) {
            TimeInterval interval = TimeInterval::infinite();
            if(frame < 0)
                interval.setEnd(sourceFrameToAnimationTime(0) - 1);
            else if(frame >= sourceFrames.size() && !sourceFrames.empty())
                interval.setStart(sourceFrameToAnimationTime(sourceFrames.size()));
            else if(sourceFrames.empty() && _framesValid)
                return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("No frames found.")), interval);
            return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The file source path is empty or has not been set (no files found).")), interval);
        }
        else if(frame < 0) {
            return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The requested source frame is out of range.")));
        }

        // Retrieve the source file.
        SharedFuture<FileHandle> fileFuture = Application::instance()->fileManager().fetchUrl(sourceFrames[frame].sourceFile);

        // Pass the file to the file importer.
        return fileFuture.then(ObjectExecutor(this), [this, frame](const FileHandle& fileHandle) -> Future<PipelineFlowState> {

            // Without an importer object we have to give up immediately.
            if(!importer()) {
                // In case of an error, just return the stale data that we have cached.
                return PipelineFlowState(dataCollection(), PipelineStatus(PipelineStatus::Error, tr("The file source path has not been set.")));
            }
            if(frame >= frames().size())
                throw Exception(tr("Requested source frame index is out of range."));

            // Compute the validity interval of the returned pipeline s tate.
            TimeInterval interval = frameTimeInterval(frame);
            const FileSourceImporter::Frame& frameInfo = frames()[frame];

            // Set up the load request to be submitted to the FileSourceImporter.
            FileSourceImporter::LoadOperationRequest loadRequest;
            loadRequest.pipelineNode = this;
            loadRequest.fileHandle = fileHandle;
            loadRequest.frame = frameInfo;
            loadRequest.isNewlyImportedFile = (dataCollection() == nullptr);
            loadRequest.state.setData(dataCollection()
                ? DataOORef<const DataCollection>(dataCollection())
                : DataOORef<const DataCollection>::create());

            // Add some standard global attributes to the pipeline state to indicate where it is coming from.
            loadRequest.state.setAttribute(QStringLiteral("SourceFrame"), frame, this);
            loadRequest.state.setAttribute(QStringLiteral("SourceFile"), frameInfo.sourceFile.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded), this);

            // Also give the state the precomputed validity interval.
            loadRequest.state.setStateValidity(interval);

            // Load the frame data and return results to the caller.
            return importer()->loadFrame(std::move(loadRequest));
        });
    });

    // Post-process the results of the loading operation before returning them to the caller.
    postprocessDataCollection(stateFuture, request);

    return std::move(stateFuture);
}

/******************************************************************************
* Scans the external data file and returns the list of discovered input frames.
******************************************************************************/
SharedFuture<QVector<FileSourceImporter::Frame>> FileSource::requestFrameList(bool forceRescan)
{
    OVITO_ASSERT(this_task::get());

    // Without an importer object the list of frames is empty.
    if(!importer())
        return Future<QVector<FileSourceImporter::Frame>>::createImmediateEmplace();

    // Return the active future when the frame loading process is currently in progress.
    if(_framesListFuture) {
        if(!forceRescan)
            return _framesListFuture;
        _framesListFuture.reset();
    }

    // Return the cached frames list if available.
    if(_framesValid && !forceRescan) {
        return _frames;
    }

    // Forward request to the importer object.
    // Intercept future results when they become available and cache them.
    _framesListFuture = importer()->discoverFrames(sourceUrls())
        // Note that storing the frames list in this FileSource is explicitly deferred to some later time,
        // because setListOfFrames() generates a TargetChanged event, which is not allowed during
        // a synchronous call to the pipeline evaluation function.
        .then(DeferredObjectExecutor(this), [this](QVector<FileSourceImporter::Frame>&& frameList) {
            // Store the new list of frames in the FileSource.
            setListOfFrames(frameList);
            // Pass the frame list on to the caller.
            return std::move(frameList);
        });

    // Has loading the frames list already completed?
    // If yes, reset the future before returning from this function.
    if(_framesListFuture.isFinished())
        return std::move(_framesListFuture);

    // Reset _framesListFuture when finished.
    _framesListFuture.finally(ObjectExecutor(this), [this](Task& task) noexcept {
        if(_framesListFuture && _framesListFuture.task().get() == &task)
            _framesListFuture.reset();
    });

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
    OVITO_ASSERT(this_task::get());

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
        unchangedInterval = TimeInterval(AnimationTime::negativeInfinity(), frameTimeInterval(frameIndex-1).end());

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

    // Serialize the list of animation frames.
    stream.beginChunk(0x04);
    stream.writeSizeT(frames().size());

    // To compactly serialize the list of frames, we use a combination of run-length encoding (RLE) and range-based encoding (start,step).
    // For example, in many cases, consecutive frames will come from the same source file URL. And in many cases the line numbers follow a regular pattern.
    if(!frames().empty()) {
        // Helper that checks whether the given array can be range-encoded, i.e., the numeric values follow a regular pattern with a fixed step size.
        auto isRangeEncodable = [&](auto&& getterFunc) {
            if constexpr (std::is_arithmetic_v<std::remove_reference_t<decltype(getterFunc(frames()[0]))>>) {
                if(frames().size() < 4)
                    return false;
                auto baseValue = getterFunc(frames()[1]); // Note: Skipping frame 0 to allow for irregular first values.
                auto nextValue = getterFunc(frames()[2]);
                auto step = nextValue - baseValue;
                for(size_t i = 3; i < frames().size(); i++) {
                    auto expectedValue = baseValue + step * (i-1);
                    if(getterFunc(frames()[i]) != expectedValue)
                        return false;
                }
                return true;
            }
            else return false;
        };
        // Helper that performs range encoding of the value array.
        auto rangeEncode = [&](auto&& getterFunc) {
            if constexpr (std::is_arithmetic_v<std::remove_reference_t<decltype(getterFunc(frames()[0]))>>) {
                stream << getterFunc(frames()[0]);
                stream << getterFunc(frames()[1]);
                stream << (getterFunc(frames()[2]) - getterFunc(frames()[1]));
            }
            else OVITO_ASSERT(false); // should not happen
        };
        // Helper that checks whether the given array should be run-length encoded. This is the case when at least two consecutive values are identical.
        auto isRunLengthEncodable = [&](auto&& getterFunc) {
            return frames().size() > 2 && getterFunc(frames()[1]) == getterFunc(frames()[0]);
        };
        // Helper that performs run-length encoding of the value array.
        auto runLengthEncode = [&](auto&& getterFunc) {
            auto previousValue = getterFunc(frames().front());
            int length = 0;
            for(const FileSourceImporter::Frame& frame : frames()) {
                if(getterFunc(frame) != previousValue) {
                    previousValue = getterFunc(frame);
                    stream << length << previousValue;
                    length = 0;
                }
                length++;
            }
            if(length)
                stream << length << previousValue;
        };
        // Helper that encodes an array of values using either range encoding, RLE, or no encoding.
        auto encodeArray = [&](int chunkId, auto&& getterFunc) {
            stream.beginChunk(chunkId);
            if(isRangeEncodable(getterFunc)) {
                stream.beginChunk(0x01); // indicates range encoding
                rangeEncode(getterFunc);
            }
            else if(isRunLengthEncodable(getterFunc)) {
                stream.beginChunk(0x02); // indicates RLE encoding
                runLengthEncode(getterFunc);
            }
            else {
                stream.beginChunk(0x00); // indicates no encoding
                for(const FileSourceImporter::Frame& frame : frames())
                    stream << getterFunc(frame);
            }
            stream.endChunk();
            stream.endChunk();
        };
        encodeArray(0x01, [](const FileSourceImporter::Frame& frame) { return frame.sourceFile; });
        encodeArray(0x02, [](const FileSourceImporter::Frame& frame) { return frame.byteOffset; });
        encodeArray(0x03, [](const FileSourceImporter::Frame& frame) { return frame.lineNumber; });
        encodeArray(0x04, [](const FileSourceImporter::Frame& frame) { return frame.lastModificationTime; });
        encodeArray(0x05, [](const FileSourceImporter::Frame& frame) { return frame.label.type; });
        encodeArray(0x06, [](const FileSourceImporter::Frame& frame) { return frame.label.numericLabel; });
        encodeArray(0x07, [](const FileSourceImporter::Frame& frame) { return frame.label.stringLabel; });
        encodeArray(0x08, [](const FileSourceImporter::Frame& frame) { return frame.parserData; });
    }

    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void FileSource::loadFromStream(ObjectLoadStream& stream)
{
    BasePipelineSource::loadFromStream(stream);

    // Deserialize the list of animation frames.
    int formatVersion = stream.expectChunkRange(0x03, 1);
    if(formatVersion >= 1) {
        // New serialization format with RLE compression.
        size_t numFrames = stream.readSizeT();
        _frames.resize(numFrames);
        if(numFrames > 0) {
            auto decodeArray = [&](int chunkId, auto&& setterFunc) {
                stream.expectChunk(chunkId);
                switch(stream.expectChunkRange(0x00, 2)) {
                    case 0: // no encoding
                        for(size_t i = 0; i < numFrames; i++)
                            stream >> setterFunc(_frames[i]);
                        break;
                    case 1: // range encoding
                        if constexpr (std::is_arithmetic_v<std::remove_reference_t<decltype(setterFunc(_frames[0]))>>) {
                            std::remove_reference_t<decltype(setterFunc(_frames.front()))> baseValue, step;
                            stream >> setterFunc(_frames[0]) >> baseValue >> step;
                            for(size_t i = 1; i < numFrames; i++) {
                                setterFunc(_frames[i]) = baseValue + step * (i - 1);
                            }
                        }
                        else OVITO_ASSERT(false); // should not happen
                        break;
                    case 2: // RLE encoding
                        for(size_t frameIndex = 0; frameIndex < numFrames;) {
                            int length;
                            std::remove_reference_t<decltype(setterFunc(_frames.front()))> value;
                            stream >> length >> value;
                            for(int i = 0; i < length; i++, frameIndex++)
                                setterFunc(_frames[frameIndex]) = value;
                            OVITO_ASSERT(frameIndex <= numFrames);
                        }
                        break;
                }
                stream.closeChunk();
                stream.closeChunk();
            };
            decodeArray(0x01, [](FileSourceImporter::Frame& frame) -> QUrl& { return frame.sourceFile; });
            decodeArray(0x02, [](FileSourceImporter::Frame& frame) -> qint64& { return frame.byteOffset; });
            decodeArray(0x03, [](FileSourceImporter::Frame& frame) -> int& { return frame.lineNumber; });
            decodeArray(0x04, [](FileSourceImporter::Frame& frame) -> QDateTime& { return frame.lastModificationTime; });
            decodeArray(0x05, [](FileSourceImporter::Frame& frame) -> AnimationFrameLabel::LabelType& { return frame.label.type; });
            decodeArray(0x06, [](FileSourceImporter::Frame& frame) -> FloatType& { return frame.label.numericLabel; });
            decodeArray(0x07, [](FileSourceImporter::Frame& frame) -> QString& { return frame.label.stringLabel; });
            decodeArray(0x08, [](FileSourceImporter::Frame& frame) -> QVariant& { return frame.parserData; });
        }
    }
    else {
        // For backward compatibility with OVITO 3.14 and older:
        stream >> _frames;
    }
    stream.closeChunk();
    _framesValid = !_frames.empty();

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

        // Clear frame label list. It will be regenerated upon request in animationFrameLabels().
        _frameLabels.clear();

        // Invalidate cached frames, because their validity intervals have changed.
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
    else if(field == PROPERTY_FIELD(BasePipelineSource::dataCollectionFrame)) {
        // The active frame is part of the source's UI title.
        if(numberOfFiles() > 1)
            notifyDependents(ReferenceEvent::TitleChanged);
    }
    BasePipelineSource::propertyChanged(field);
}

/******************************************************************************
* Gets called when the data provider of the pipeline has been replaced.
******************************************************************************/
void FileSource::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(importer)) {
        notifyDependents(ReferenceEvent::TitleChanged);
    }
    BasePipelineSource::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* If the file source currently uses a wildcard search pattern, replaces it
* with a single concrete filename.
******************************************************************************/
void FileSource::removeWildcardFilePattern()
{
    for(const QUrl& url : sourceUrls()) {
        if(FileSourceImporter::isWildcardPattern(url)) {
            // If the currently loaded frame is valid, use its filename as new source URL without a wildcard.
            if(dataCollectionFrame() >= 0 && dataCollectionFrame() < frames().size()) {
                const QUrl& currentUrl = frames()[dataCollectionFrame()].sourceFile;
                if(currentUrl != url) {
                    setSource({currentUrl}, importer(), false);
                    return;
                }
            }
            // If no frame has been loaded yet, e.g. during initial import, use the _originallySelectedFilename as new source URL.
            // This is the filename that the user originally picked in the file selection dialog.
            else if(dataCollectionFrame() < 0 && !_originallySelectedFilename.isEmpty() && !_originallySelectedFilename.contains(QChar('*'))) {
                for(const auto frame : frames()) {
                    if(frame.sourceFile.fileName() == _originallySelectedFilename) {
                        const QUrl& currentUrl = frame.sourceFile;
                        if(currentUrl != url) {
                            setSource({currentUrl}, importer(), false);
                            return;
                        }
                    }
                }
                break; // No matching frame found.
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
            QString wildcardPattern = FileSourceImporter::deriveWildcardPatternFromFilename(url.fileName());
            if(!wildcardPattern.isEmpty()) {
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

/******************************************************************************
* Creates a copy of this object.
******************************************************************************/
OORef<RefTarget> FileSource::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<FileSource> clone = static_object_cast<FileSource>(BasePipelineSource::clone(deepCopy, cloneHelper));
    clone->_frames = this->_frames;
    clone->_framesValid = this->_framesValid;
    clone->_frameLabels = this->_frameLabels;
    clone->_numberOfFiles = this->_numberOfFiles;
    return clone;
}

/******************************************************************************
* Builds a list of human-readable frame labels, which can be displayed in the UI.
******************************************************************************/
QStringList FileSource::humanReadableFrameLabels() const
{
    QStringList labels;
    labels.reserve(frames().size());
    for(const auto& frame : frames()) {
        labels.push_back(frame.label.toDisplayString());
    }
    return labels;
}

}   // End of namespace
