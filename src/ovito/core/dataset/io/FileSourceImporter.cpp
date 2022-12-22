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
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/Application.h>
#include "FileSourceImporter.h"
#include "FileSource.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(FileSourceImporter);
DEFINE_PROPERTY_FIELD(FileSourceImporter, isMultiTimestepFile);
SET_PROPERTY_FIELD_LABEL(FileSourceImporter, isMultiTimestepFile, "File contains multiple timesteps");

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void FileSourceImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
	FileImporter::propertyChanged(field);

	if(field == PROPERTY_FIELD(isMultiTimestepFile)) {
		// Automatically rescan input file for animation frames when this option is changed.
		requestFramesUpdate();

		// Also notify the UI explicitly, because target-changed messages are supressed for this property field.
		Q_EMIT isMultiTimestepFileChanged();
	}
}

/******************************************************************************
* Sends a request to the FileSource owning this importer to reload
* the input file.
******************************************************************************/
void FileSourceImporter::requestReload(bool refetchFiles, int frame)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::requestReload", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	// Retrieve the FileSource that owns this importer by looking it up in the list of dependents.
	visitDependents([&](RefMaker* dependent) {
		if(FileSource* fileSource = dynamic_object_cast<FileSource>(dependent)) {
			ExecutionContext::current().ui().handleExceptions([&] {
				fileSource->reloadFrame(refetchFiles, frame);
			});
		}
		else if(FileSourceImporter* parentImporter = dynamic_object_cast<FileSourceImporter>(dependent)) {
			// If this importer is a child of another importer, forward the reload request to the parent importer.
			parentImporter->requestReload(refetchFiles, frame);
		}
	});
}

/******************************************************************************
* Sends a request to the FileSource owning this importer to refresh the
* animation frame sequence.
******************************************************************************/
void FileSourceImporter::requestFramesUpdate(bool refetchCurrentFile)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::requestReload", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	// Retrieve the FileSource that owns this importer by looking it up in the list of dependents.
	visitDependents([&](RefMaker* dependent) {
		if(FileSource* fileSource = dynamic_object_cast<FileSource>(dependent)) {
			// Scan input source for animation frames.
			ExecutionContext::current().ui().handleExceptions([&] {
				fileSource->updateListOfFrames(refetchCurrentFile);
			});
		}
		else if(FileSourceImporter* parentImporter = dynamic_object_cast<FileSourceImporter>(dependent)) {
			// If this importer is a child of another importer, forward the update request to the parent importer.
			parentImporter->requestFramesUpdate(refetchCurrentFile);
		}
	});
}

/******************************************************************************
* Returns the FileSource that manages this importer object (if any).
******************************************************************************/
FileSource* FileSourceImporter::fileSource() const
{
	FileSource* source = nullptr;
	visitDependents([&](RefMaker* dependent) {
		if(FileSource* fileSource = dynamic_object_cast<FileSource>(dependent))
			source = fileSource;
	});
	return source;
}

/******************************************************************************
* Determines if the option to replace the currently selected object
* with the new file is available.
******************************************************************************/
bool FileSourceImporter::isReplaceExistingPossible(Scene* scene, const std::vector<QUrl>& sourceUrls)
{
	if(scene) {
		// Look for an existing FileSource in the scene whose
		// data source we can replace with the new file.
		for(SceneNode* node : scene->selection()->nodes()) {
			if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(node)) {
				if(dynamic_object_cast<FileSource>(pipeline->pipelineSource()))
					return true;
			}
		}
	}
	return false;
}

/******************************************************************************
* Imports the given file into the scene.
* Return true if the file has been imported.
* Return false if the import has been aborted by the user.
* Throws an exception when the import has failed.
******************************************************************************/
OORef<PipelineSceneNode> FileSourceImporter::importFileSet(Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode)
{
	OVITO_ASSERT(!sourceUrlsAndImporters.empty());
	OORef<FileSource> existingFileSource;
	PipelineSceneNode* existingPipeline = nullptr;

	if(importMode == ReplaceSelected) {
		// Look for an existing FileSource in the scene whose
		// data source can be replaced with the newly imported file.
		if(scene) {
			for(SceneNode* node : scene->selection()->nodes()) {
				if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(node)) {
					existingFileSource = dynamic_object_cast<FileSource>(pipeline->pipelineSource());
					if(existingFileSource) {
						existingPipeline = pipeline;
						break;
					}
				}
			}
		}
	}
	else if(importMode == ResetScene) {
		if(scene)
			scene->clear();
	}
	else if(importMode == AddToScene) {
		OVITO_ASSERT(scene != nullptr);
		if(scene->children().empty())
			importMode = ResetScene;
		else {
#ifndef OVITO_BUILD_PROFESSIONAL
			throw Exception(tr("Sorry, this operation cannot be performed in OVITO Basic. Importing multiple datasets into the same scene is supported by <a href=\"https://www.ovito.org/about/ovito-pro/\">OVITO Pro</a>."));
#endif
		}
	}

	OORef<FileSource> fileSource = existingFileSource;

	// Create the object that will insert the imported data into the scene.
	if(!fileSource)
		fileSource = OORef<FileSource>::create();

	// Inherit multi-timestep flag from existing importer, but only if the old importer
	// was for the same file format. That's because the new importer may not support multi-timestep
	// files at all.
	if(existingFileSource && existingFileSource->importer() && existingFileSource->importer()->getOOClass() == this->getOOClass() && existingFileSource->importer()->isMultiTimestepFile())
		setMultiTimestepFile(true);

	// Create a new pipeline node in the scene for the linked data.
	OORef<PipelineSceneNode> pipeline;
	if(existingPipeline == nullptr) {
		{
			UndoSuspender unsoSuspender;	// Do not create undo records for this part.

			// Add object to scene.
			pipeline = OORef<PipelineSceneNode>::create();
			pipeline->setDataProvider(fileSource);

			// Let the importer subclass customize the pipeline scene node.
			setupPipeline(pipeline, fileSource);
		}

		// Insert pipeline into scene.
		if(importMode != DontAddToScene && scene != nullptr)
			scene->addChildNode(pipeline);
	}
	else pipeline = existingPipeline;

	// Select new object in the scene.
	if(importMode != DontAddToScene && scene != nullptr)
		scene->selection()->setNode(pipeline);

	// Concatenate all files from the input list having the same file format into one sequence,
	// which gets handled by this importer.
	std::vector<QUrl> sourceUrls;
	OVITO_ASSERT(sourceUrlsAndImporters.front().second == this);
	sourceUrls.push_back(std::move(sourceUrlsAndImporters.front().first));
	auto iter = std::next(sourceUrlsAndImporters.begin());
	if(multiFileImportMode == ImportAsTrajectory) {
		for(; iter != sourceUrlsAndImporters.end(); ++iter) {
			if(iter->second->getOOClass() != this->getOOClass())
				break;
			sourceUrls.push_back(std::move(iter->first));		
		}
	}
	sourceUrlsAndImporters.erase(sourceUrlsAndImporters.begin(), iter); 

	// Set the input file location(s) and importer.
	bool keepExistingDataCollection = true;
	if(!fileSource->setSource(std::move(sourceUrls), this, autodetectFileSequences && (sourceUrls.size() == 1 && sourceUrlsAndImporters.empty()), keepExistingDataCollection))
		return {};

	if(importMode != ReplaceSelected && importMode != DontAddToScene) {
		// Adjust viewports to completely show the newly imported object.
		// This needs to be deferred until after the data has been completely loaded and its extents are known.
		ExecutionContext::current().ui().zoomToSceneExtentsWhenReady();
	}

	// If this importer did not handle all supplied input files, 
	// continue importing the remaining files.
	if(!sourceUrlsAndImporters.empty()) {
		if(!importFurtherFiles(scene, std::move(sourceUrlsAndImporters), importMode, autodetectFileSequences, multiFileImportMode, pipeline))
			return {};
	}

	return pipeline;
}

/******************************************************************************
* Is called when importing multiple files of different formats.
******************************************************************************/
bool FileSourceImporter::importFurtherFiles(Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode, PipelineSceneNode* pipeline)
{
	if(importMode == DontAddToScene)
		return true;	// It doesn't make sense to import additional datasets if they are not being added to the scene. They would get lost.

	OVITO_ASSERT(!sourceUrlsAndImporters.empty());
	OORef<FileImporter> importer = sourceUrlsAndImporters.front().second;
	return importer->importFileSet(scene, std::move(sourceUrlsAndImporters), AddToScene, autodetectFileSequences, multiFileImportMode);
}

/******************************************************************************
* Determines whether the URL contains a wildcard pattern.
******************************************************************************/
bool FileSourceImporter::isWildcardPattern(const QUrl& sourceUrl)
{
	return QFileInfo(sourceUrl.path()).fileName().contains('*');
}

/******************************************************************************
* Scans the given external path(s) (which may be a directory and a wild-card pattern,
* or a single file containing multiple frames) to find all available animation frames.
******************************************************************************/
Future<QVector<FileSourceImporter::Frame>> FileSourceImporter::discoverFrames(const std::vector<QUrl>& sourceUrls)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::discoverFrames", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	// No output if there is no input.
	if(sourceUrls.empty())
		return QVector<Frame>();

	// If there is only a single input path, call sub-routine handling single paths.
	if(sourceUrls.size() == 1)
		return discoverFrames(sourceUrls.front());

	// Sequentually invoke single-path routine for each input path and compile results
	// into one big list that is returned to the caller.
	auto combinedList = std::make_shared<QVector<Frame>>();
	Future<QVector<Frame>> future;
	for(const QUrl& url : sourceUrls) {
		if(!future.isValid()) {
			future = discoverFrames(url);
		}
		else {
			future = future.then(*this, [this, combinedList, url](const QVector<Frame>& frames) {
				*combinedList += frames;
				return discoverFrames(url);
			});
		}
	}
	return future.then([combinedList](const QVector<Frame>& frames) {
		*combinedList += frames;
		return std::move(*combinedList);
	});
}


/******************************************************************************
* Scans the given external path (which may be a directory and a wild-card pattern,
* or a single file containing multiple frames) to find all available animation frames.
******************************************************************************/
Future<QVector<FileSourceImporter::Frame>> FileSourceImporter::discoverFrames(const QUrl& sourceUrl)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::discoverFrames", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	if(shouldScanFileForFrames(sourceUrl)) {

		// Check if filename is a wildcard pattern.
		// If yes, find all matching files and scan each one of them.
		if(isWildcardPattern(sourceUrl)) {
			return findWildcardMatches(sourceUrl)
				.then(*this, [this](const std::vector<QUrl>& fileList) {
					return discoverFrames(fileList);
				});
		}

		// Fetch file, then scan it.
		return Application::instance()->fileManager().fetchUrl(sourceUrl)
			.then(*this, [this](const FileHandle& fileHandle) {
				return discoverFrames(fileHandle);
			});
	}
	else {
		if(isWildcardPattern(sourceUrl)) {
			// Find all files matching the file pattern.
			return findWildcardMatches(sourceUrl)
				.then(*this, [](const std::vector<QUrl>& fileList) {
					// Turn the file list into a frame list.
					QVector<Frame> frames;
					frames.reserve(fileList.size());
					for(const QUrl& url : fileList) {
						QFileInfo fileInfo(url.path());
						QDateTime dateTime = url.isLocalFile() ? fileInfo.lastModified() : QDateTime();
						frames.push_back(Frame(url, 0, 1, dateTime, fileInfo.fileName()));
					}
					return frames;
				});
		}
		else {
			// Build just a single frame from the source URL.
			QFileInfo fileInfo(sourceUrl.path());
			QDateTime dateTime = sourceUrl.isLocalFile() ? fileInfo.lastModified() : QDateTime();
			return QVector<Frame>{{ Frame(sourceUrl, 0, 1, dateTime, fileInfo.fileName()) }};
		}
	}
}

/******************************************************************************
* Scans the given external path (which may be a directory and a wild-card pattern,
* or a single file containing multiple frames) to find all available animation frames.
******************************************************************************/
Future<QVector<FileSourceImporter::Frame>> FileSourceImporter::discoverFrames(const FileHandle& fileHandle)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::discoverFrames", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	// Scan file.
	if(FrameFinderPtr frameFinder = createFrameFinder(fileHandle))
		return frameFinder->runAsync(true);
	else
		return QVector<Frame>{{ Frame(fileHandle) }};
}

/******************************************************************************
* Loads the data for the given frame from the external file.
******************************************************************************/
Future<PipelineFlowState> FileSourceImporter::loadFrame(const LoadOperationRequest& request)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "FileSourceImporter::loadFrame", "This function may only be called from the main thread.");
	OVITO_ASSERT(ExecutionContext::current().isValid());

	// Note: FileSourceImporter::loadFrame() may not be called while undo recording is active.
	OVITO_ASSERT(!isUndoRecording());

	// Create the frame loader for the requested frame.
	FrameLoaderPtr frameLoader = createFrameLoader(request);
	OVITO_ASSERT(frameLoader);

	// Execute the loader in a background thread.
	Future<PipelineFlowState> future = frameLoader->runAsync(true);

	// If the parser has detects additional frames following the first frame in the 
	// input file being loaded, automatically turn on scanning of the input file.
	// Only automatically turn scanning on if the file is being newly imported, i.e. if the file source has not loaded a data collection yet.
	if(request.isNewlyImportedFile) {
		// Note: Changing a parameter of the file importer must be done in the correct thread.
		future.finally(*this, [this](Task& task) noexcept {
			if(!task.isCanceled()) {
				FrameLoader& frameLoader = static_cast<FrameLoader&>(task);
				if(frameLoader.additionalFramesDetected() && !isMultiTimestepFile()) {
					ExecutionContext::current().ui().handleExceptions([&] {
						setMultiTimestepFile(true);
					});
				}
			}
		});
	}

	return future;
}

/******************************************************************************
* Scans the source URL for input frames.
******************************************************************************/
void FileSourceImporter::FrameFinder::perform()
{
	QVector<Frame> frameList;
	try {		
		discoverFramesInFile(frameList);
	}
	catch(const Exception&) {
		// Silently ignore parsing and I/O errors if at least two frames have been read.
		// Keep all frames read up to where the error occurred.
		if(frameList.size() <= 1)
			throw;
		else
			frameList.pop_back();		// Remove last discovered frame because it may be corrupted or only partially written.
	}
	setResult(std::move(frameList));
}

/******************************************************************************
* Scans the given file for source frames
******************************************************************************/
void FileSourceImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
	// By default, register a single frame.
	frames.push_back(Frame(fileHandle()));
}

/******************************************************************************
* Returns the list of files that match the given wildcard pattern.
******************************************************************************/
Future<std::vector<QUrl>> FileSourceImporter::findWildcardMatches(const QUrl& sourceUrl)
{
	// Determine whether the filename contains a wildcard character.
	if(!isWildcardPattern(sourceUrl)) {

		// It's not a wildcard pattern. Register just a single frame.
		return std::vector<QUrl>{ sourceUrl };
	}
	else {
		QFileInfo fileInfo(sourceUrl.path());
		QString pattern = fileInfo.fileName();

		QDir directory;
		bool isLocalPath = false;
		Future<QStringList> entriesFuture;

		// Scan the directory for files matching the wildcard pattern.
		if(sourceUrl.isLocalFile()) {

			QStringList entries;
			isLocalPath = true;
			directory = QFileInfo(sourceUrl.toLocalFile()).dir();
			for(const QString& filename : directory.entryList(QDir::Files|QDir::NoDotAndDotDot|QDir::Hidden, QDir::Name)) {
				if(matchesWildcardPattern(pattern, filename))
					entries << filename;
			}
			entriesFuture = Future<QStringList>::createImmediate(std::move(entries));
		}
		else {

			directory = fileInfo.dir();
			QUrl directoryUrl = sourceUrl;
			directoryUrl.setPath(fileInfo.path());

			// Retrieve list of files in remote directory.
			Future<QStringList> remoteFileListFuture = Application::instance()->fileManager().listDirectoryContents(directoryUrl);

			// Filter file names.
			entriesFuture = remoteFileListFuture.then([pattern](QStringList&& remoteFileList) {
				QStringList entries;
				for(const QString& filename : remoteFileList) {
					if(matchesWildcardPattern(pattern, filename))
						entries << filename;
				}
				return entries;
			});
		}

		// Sort the file list.
		return entriesFuture.then([isLocalPath, sourceUrl, directory](QStringList&& entries) {

			// A file called "abc9.xyz" must come before a file named "abc10.xyz", which is not
			// the default lexicographic ordering.
			QMap<QString, QString> sortedFilenames;
			for(QString& oldName : entries) {
				// Generate a new name from the original filename that yields the correct ordering.
				QString newName;
				QString number;
				for(QChar c : oldName) {
					if(!c.isDigit()) {
						if(!number.isEmpty()) {
							newName.append(number.rightJustified(12, '0'));
							number.clear();
						}
						newName.append(c);
					}
					else number.append(c);
				}
				if(!number.isEmpty())
					newName.append(number.rightJustified(12, '0'));
				if(!sortedFilenames.contains(newName))
					sortedFilenames[newName] = std::move(oldName);
				else
					sortedFilenames[oldName] = oldName;
			}

			// Generate final list of frames.
			std::vector<QUrl> urls;
			urls.reserve(sortedFilenames.size());
			for(auto iter = sortedFilenames.constBegin(); iter != sortedFilenames.constEnd(); ++iter) {
				QFileInfo fileInfo(directory, iter.value());
				QUrl url = sourceUrl;
				if(isLocalPath)
					url = QUrl::fromLocalFile(fileInfo.filePath());
				else
					url.setPath(fileInfo.filePath());
				urls.push_back(url);
			}

			return urls;
		});
	}
}

/******************************************************************************
* Checks if a filename matches to the given wildcard pattern.
******************************************************************************/
bool FileSourceImporter::matchesWildcardPattern(const QString& pattern, const QString& filename)
{
	QString::const_iterator p = pattern.constBegin();
	QString::const_iterator f = filename.constBegin();
	while(p != pattern.constEnd() && f != filename.constEnd()) {
		if(*p == QChar('*')) {
			if(!f->isDigit())
				return false;
			do { ++f; }
			while(f != filename.constEnd() && f->isDigit());
			++p;
			continue;
		}
		else if(*p != *f)
			return false;
		++p;
		++f;
	}
	return p == pattern.constEnd() && f == filename.constEnd();
}

/******************************************************************************
* Writes an animation frame information record to a binary output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const FileSourceImporter::Frame& frame)
{
	stream.beginChunk(0x03);
	stream << frame.sourceFile << frame.byteOffset << frame.lineNumber << frame.lastModificationTime << frame.label << frame.parserData;
	stream.endChunk();
	return stream;
}

/******************************************************************************
* Reads an animation frame information record from a binary input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, FileSourceImporter::Frame& frame)
{
	stream.expectChunk(0x03);
	stream >> frame.sourceFile >> frame.byteOffset >> frame.lineNumber >> frame.lastModificationTime >> frame.label >> frame.parserData;
	stream.closeChunk();
	return stream;
}

/******************************************************************************
* Calls loadFile() and sets the returned frame data as result of the 
* asynchronous task.
******************************************************************************/
void FileSourceImporter::FrameLoader::perform()
{
	// Let the subclass implementation parse the file.
	loadFile();

	// Pass the constructed pipeline state back to the caller.
	setResult(std::move(_loadRequest.state));
}

}	// End of namespace
