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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/pipeline/PipelineStatus.h>
#include <ovito/core/utilities/io/FileManager.h>

namespace Ovito {

/**
 * \brief Base class for file parsers that can reload a file that has been imported into the scene.
 */
class OVITO_CORE_EXPORT FileSourceImporter : public FileImporter
{
    OVITO_CLASS(FileSourceImporter)

public:

    /// Data structure that stores meta information about a source trajectory frame.
    struct Frame {

        /// Default constructor.
        Frame() = default;

        /// Constructor taking a FileHandle.
        Frame(const FileHandle& fileHandle, qint64 offset = 0, int linenum = 1) :
                sourceFile(fileHandle.sourceUrl()), byteOffset(offset), lineNumber(linenum) {
            label.setToFilename(fileHandle.sourceUrl().fileName());
            if(!fileHandle.localFilePath().isEmpty())
                lastModificationTime = QFileInfo(fileHandle.localFilePath()).lastModified();
        }

        /// Constructor taking a QUrl.
        Frame(const QUrl& url, qint64 offset = 0, int linenum = 1, const QDateTime& modTime = QDateTime()) :
            sourceFile(url), byteOffset(offset), lineNumber(linenum), lastModificationTime(modTime) {
            label.setToFilename(url.fileName());
        }

        /// Constructor taking a QUrl and a timestep frame label.
        Frame(QUrl&& url, qlonglong timestep) : sourceFile(std::move(url)) {
            label.setToTimestep(timestep);
            if(sourceFile.isLocalFile())
                lastModificationTime = QFileInfo(sourceFile.path()).lastModified();
        }

        /// The source file that contains the data of the animation frame.
        QUrl sourceFile;

        /// The byte offset into the source file where the frame's data is stored.
        qint64 byteOffset = 0;

        /// The line number in the source file where the frame data is stored, if the file has a text-based format.
        int lineNumber = 1;

        /// The last modification time of the source file.
        /// This is used to detect changes of the source file, which let the stored byte offset become invalid.
        QDateTime lastModificationTime;

        /// The name or label of the source frame.
        AnimationFrameLabel label;

        /// Auxiliary field that can be used by the file parser to store additional info about the frame.
        QVariant parserData;

        /// Compares two frame descriptors.
        bool operator!=(const Frame& other) const {
            return (const_cast<QUrl&>(sourceFile).data_ptr() != const_cast<QUrl&>(other.sourceFile).data_ptr() && sourceFile != other.sourceFile) ||
                    (byteOffset != other.byteOffset) ||
                    (lineNumber != other.lineNumber) ||
                    (lastModificationTime != other.lastModificationTime) ||
                    (parserData != other.parserData);
        }
    };
    static_assert(std::is_move_constructible<Frame>::value, "Frame class must be move-constructible");
    static_assert(std::is_move_assignable<Frame>::value, "Frame class must be move-constructible");

    struct LoadOperationRequest {

        /// The source file information.
        Frame frame;

        /// The local handle to the input file.
        FileHandle fileHandle;

        /// Holds the data objects loaded from the file. DataCollection contains the data from a previous trajectory frame if any.
        PipelineFlowState state;

        /// The FileSource that initiated the load operation.
        OOWeakRef<const PipelineNode> pipelineNode;

        /// If a loaded data collection consists of sub-collections, this string specifies the
        /// prefix to be prepended to the identifiers of data objects loaded by the file reader.
        QString dataBlockPrefix;

        /// Indicates that the file reader should append the loaded data to existing data objects
        /// instead of replacing their contents. This is used for loading multi-block datasets
        /// consisting of several files.
        bool appendData = false;

        /// Indicates whether the file is being loaded for the first time or a subsequent frame is being loaded.
        bool isNewlyImportedFile = true;
    };

    /**
     * Abstract base class for frame loader implementations that run in a worker thread.
     */
    class OVITO_CORE_EXPORT FrameLoader
    {
    public:

        /// Constructor.
        explicit FrameLoader(const LoadOperationRequest& request) : _loadRequest(request) {}

        /// Destructor.
        virtual ~FrameLoader() = default;

        /// Returns the source file information.
        const Frame& frame() const { return loadRequest().frame; }

        /// Returns the local handle to the input data file.
        const FileHandle& fileHandle() const { return loadRequest().fileHandle; }

        /// Returns a reference to the pipeline state that receives the loaded file data.
        PipelineFlowState& state() { return _loadRequest.state; }

        /// Returns the FileSource that owns the file importer.
        const OOWeakRef<const PipelineNode>& pipelineNode() const { return _loadRequest.pipelineNode; }

        /// Returns a data structure describing the current load operation.
        const LoadOperationRequest& loadRequest() const { return _loadRequest; }

        /// File parser implementations call this method to indicate that the input file contains
        /// additional frames stored back to back with the currently loaded one.
        void signalAdditionalFrames() { _additionalFramesDetected = true; }

        /// Indicate whether the input file contains more than one animation frame.
        bool additionalFramesDetected() const { return _additionalFramesDetected; }

        /// Reads the frame data from the file. This method must be implemented by subclasses.
        virtual void loadFile() = 0;

    private:

        /// Data structure holding information about the load operation.
        LoadOperationRequest _loadRequest;

        /// Flag that is set by the parser to indicate that the input file contains more than one animation frame.
        bool _additionalFramesDetected = false;
    };

    /// A managed pointer to a FrameLoader instance.
    using FrameLoaderPtr = std::unique_ptr<FrameLoader>;

public:

    /// Custom notification event types generated by this class:
    enum {
        /// This event is generated whenever the value of the isMultiTimestepFile property changes.
        MultiTimestepFileChanged = FileImporter::NEXT_AVAILABLE_EVENT_ID,

        /// End-of-list value indicating the next available event type that can be used by sub-classes for custom notifications.
        NEXT_AVAILABLE_EVENT_ID
    };

    ///////////////////////////// from FileImporter /////////////////////////////

    /// Asks the importer if the option to replace the currently selected object
    /// with the new file(s) is available.
    virtual bool isReplaceExistingPossible(Scene* scene, const std::vector<QUrl>& sourceUrls) override;

    /// Imports the given file(s) into the scene.
    virtual Future<OORef<Pipeline>> importFileSet(OORef<Scene> scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode) override;

    //////////////////////////// Specific methods ////////////////////////////////

    /// This method indicates whether a wildcard pattern should be automatically generated
    /// when the user picks a new input filename. The default implementation returns true if isMultiTimestepFile is set to false.
    /// Subclasses can override this method to disable generation of wildcard patterns.
    virtual bool autoGenerateWildcardPattern() { return !isMultiTimestepFile(); }

    /// Scans the given external path(s) (which may be a directory and a wild-card pattern,
    /// or a single file containing multiple frames) to find all available animation frames.
    ///
    /// \param sourceUrls The list of source files or wild-card patterns to scan for animation frames.
    /// \return A Future that will yield the list of discovered animation frames.
    ///
    /// The default implementation of this method checks if the given URLs contain a wild-card pattern.
    /// If yes, it scans the directory to find all matching files.
    virtual Future<QVector<Frame>> discoverFrames(const std::vector<QUrl>& sourceUrls);

    /// Scans the given external path (which may be a directory and a wild-card pattern,
    /// or a single file containing multiple frames) to find all available animation frames.
    ///
    /// \param sourceUrl The source file or wild-card patterns to scan for animation frames.
    /// \return A Future that will yield the list of discovered animation frames.
    ///
    /// The default implementation of this method checks if the given URLs contain a wild-card pattern.
    /// If yes, it scans the directory to find all matching files.
    Future<QVector<Frame>> discoverFrames(const QUrl& sourceUrl);

    /// Scans the given data file to find all available animation frames.
    virtual Future<QVector<Frame>> discoverFrames(const FileHandle& fileHandle);

    /// Queries the filesystem and returns a list of files matching the given wildcard pattern.
    /// The filename pattern must contain exactly one '*' wildcard character, which represents a sequence of digits of arbitrary length.
    /// The function returns the numeric part extracted from each matching filename.
    static Future<QStringList> findWildcardMatches(const QUrl& urlPattern);

    /// Queries the filesystem and returns a list of files matching the given wildcard pattern.
    /// The filename pattern can contain exactly one '*' wildcard character, which represents a sequence of digits of arbitrary length.
    /// This version of the function returns the list of fully resolved file URLs.
    static Future<std::vector<QUrl>> findWildcardMatchesResolved(const QUrl& urlPattern);

    /// Sends a request to the FileSource owning this importer to reload the input file.
    void requestReload(bool refetchFiles = false, int frame = -1);

    /// Sends a request to the FileSource owning this importer to refresh the animation frame sequence.
    void requestFramesUpdate(bool refetchCurrentFile = false);

    /// Loads the data for the given frame from the external file.
    virtual Future<PipelineFlowState> loadFrame(const LoadOperationRequest& request);

    /// Returns the FileSource that manages this importer object (if any).
    FileSource* fileSource() const;

    /// Determines whether the URL contains a wildcard pattern.
    static bool isWildcardPattern(const QUrl& sourceUrl) {
        return isWildcardPattern(sourceUrl.fileName());
    }

    /// Determines whether a filename contains a wildcard pattern.
    static bool isWildcardPattern(const QString& filename);

	/// Tries to derive a sensible wildcard pattern from a filename by replacing a
	/// numeric character sequence with a '*'.
	static QString deriveWildcardPatternFromFilename(const QString& filename);

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// This method is called when the pipeline scene node for the FileSource is created.
    /// It can be overwritten by importer subclasses to customize the initial pipeline, add modifiers, etc.
    /// The default implementation does nothing.
    virtual Future<void> setupPipeline(OORef<Pipeline> pipeline, OORef<FileSource> importObj) { return {}; }

    /// Checks if a filename matches to the given wildcard pattern.
    /// If yes, returns the numeric part extracted from the filename.
    static std::optional<QStringView> matchesWildcardPattern(const QStringView pattern, const QStringView filename);

    /// Given a URL pattern and a list of entries, resolves the wildcard matches into full file URLs.
    static std::vector<QUrl> resolveWildcardMatches(const QUrl& urlPattern, const QStringList& entries);

    /// Determines whether the input file should be scanned to discover all contained frames.
    /// The default implementation returns the value of isMultiTimestepFile().
    virtual bool shouldScanFileForFrames(const QUrl& sourceUrl) const { return isMultiTimestepFile(); }

    /// Scans a given file and builds a list of trajectory frames.
    /// This method is called by the system from a worker thread.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<Frame>& frames) const {}

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) { return {}; }

    /// Is called when importing multiple files of different formats.
    virtual Future<OORef<Pipeline>> importFurtherFiles(OORef<Scene> scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode, Pipeline* pipeline);

private:

    /// Indicates that the input file contains multiple timesteps.
    /// Note: Suppressing change messages for this field, because it may be set to true during
    /// a file import operation and we don't want to trigger a reload of the file.
    /// Instead, a MultiTimestepFileChanged notification event is generated whenever this field changes.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, isMultiTimestepFile, setMultiTimestepFile, PROPERTY_FIELD_NO_CHANGE_MESSAGE);
};

/// \brief Reads an animation frame information record from a binary input stream.
/// \relates FileSourceImporter::Frame
OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, FileSourceImporter::Frame& frame);

}   // End of namespace
