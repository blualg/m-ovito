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
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/core/utilities/io/CompressedTextWriter.h>
#include <ovito/core/utilities/MoveOnlyAny.h>

namespace Ovito {

/**
 * \brief A meta-class for file exporters (i.e. classes derived from FileExporter).
 */
class OVITO_CORE_EXPORT FileExporterClass : public RefTarget::OOMetaClass
{
public:

    /// Inherit standard constructor from base meta class.
    using RefTarget::OOMetaClass::OOMetaClass;

    /// \brief Returns the filename filter that specifies the file extension that can be exported by this service.
    /// \return A wild-card pattern for the file types that can be produced by the FileExporter class (e.g. \c "*.xyz" or \c "*").
    virtual QString fileFilter() const {
        OVITO_ASSERT_MSG(false, "FileExporterClass::fileFilter()", "This method should be overridden by a meta-subclass of FileExporterClass.");
        return {};
    }

    /// \brief Returns the file type description that is displayed in the drop-down box of the export file dialog.
    /// \return A human-readable string describing the file format written by the FileExporter class.
    virtual QString fileFilterDescription() const {
        OVITO_ASSERT_MSG(false, "FileExporterClass::fileFilterDescription()", "This method should be overridden by a meta-subclass of FileExporterClass.");
        return {};
    }
};

/**
 * \brief Abstract base class for file writers that export data from OVITO to an external file in a specific format.
 */
class OVITO_CORE_EXPORT FileExporter : public RefTarget
{
    OVITO_CLASS_META(FileExporter, FileExporterClass)

public:

    /// Selects the default scene node to be exported by this exporter.
    virtual void selectDefaultExportableData(DataSet* dataset, Scene* scene);

    /// Determines whether the given scene node is suitable for this file exporter service.
    /// By default, all pipeline scene nodes are considered suitable that produce
    /// suitable data objects of the type specified by the FileExporter::exportableDataObjectClass() method.
    virtual bool isSuitableSceneNode(SceneNode* node);

    /// Determines whether the given pipeline output is suitable for exporting with this exporter service.
    /// By default, all data collections are considered suitable that contain suitable data objects
    /// of the type(s) specified by the FileExporter::exportableDataObjectClass() method.
    /// Subclasses can refine this behavior as needed.
    virtual bool isSuitablePipelineOutput(const PipelineFlowState& state);

    /// Determines whether the given data object is suitable for being exported by this file exporter service.
    virtual bool isSuitableDataObject(const ConstDataObjectPath& dataPath) { return true; }

    /// Returns the specific type(s) of data objects that this exporter service can export.
    /// The default implementation returns an empty list to indicate that the exporter is not restricted to
    /// a specific class of data objects. Subclasses should override this behavior.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() { return {}; }

    /// Sets the name of the output file that should be written by this exporter.
    virtual void setOutputFilename(const QString& filename);

    /// Exports the scene data to the output file(s).
    [[nodiscard]] Future<void> performExport();

    /// Indicates whether this file exporter can write more than one animation frame (i.e. a trajectory) into a single output file.
    virtual bool supportsMultiFrameFiles() { return false; }

    /// Evaluates the pipeline whose data is to be exported.
    [[nodiscard]] virtual Future<PipelineFlowState> getPipelineDataToBeExported(int frameNumber) const;

    /// Returns a string with the list of available data objects of the given type.
    static QString getAvailableDataObjectList(const PipelineFlowState& state, const DataObject::OOMetaClass& objectType);

protected:

    /// Writes the given sequence of frames to the output file(s).
    [[nodiscard]] virtual Future<void> exportFrames(int firstFrame, int lastFrame, int frameStepSize);

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) = 0;

private:

    /// The output file path.
    DECLARE_PROPERTY_FIELD(QString{}, outputFilename);

    /// Controls whether only the current trajectory frame or an entire trajectory interval should be exported.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, exportTrajectory, setExportTrajectory);

    /// Indicates that the exporter should produce a separate file for each timestep.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useWildcardFilename, setUseWildcardFilename);

    /// The wildcard name that is used to generate the output filenames.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, wildcardFilename, setWildcardFilename);

    /// The first animation frame that should be exported.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, startFrame, setStartFrame);

    /// The last animation frame that should be exported.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{-1}, endFrame, setEndFrame);

    /// Controls the interval between exported frames.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{1}, everyNthFrame, setEveryNthFrame);

    /// Controls the desired precision with which floating-point numbers are written if the format is text-based.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{10}, floatOutputPrecision, setFloatOutputPrecision);

    /// The dataset to be exported.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<DataSet>, datasetToExport, setDatasetToExport, PROPERTY_FIELD_NO_SUB_ANIM);

    /// The scene to be exported.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Scene>, sceneToExport, setSceneToExport, PROPERTY_FIELD_NO_SUB_ANIM);

    /// The pipeline to be exported.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Pipeline>, pipelineToExport, setPipelineToExport, PROPERTY_FIELD_NO_SUB_ANIM);

    /// The specific data object from the pipeline output to be exported.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, dataObjectToExport, setDataObjectToExport);
};

/**
 * Abstract base class for file export implementations.
 */
class OVITO_CORE_EXPORT FileExportJob : public OvitoObject
{
    OVITO_CLASS(FileExportJob)

public:

    /// Constructor.
    void initializeObject(const FileExporter* exporter, const QString& filePath, bool openTextStream);

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    virtual void aboutToBeDeleted() override;

    /// This is called after an output file has been successfully written and before the worker is destroyed.
    virtual void close(bool exportCompleted = true);

    /// Produces the data to be exported for a trajectory frame.
    [[nodiscard]] virtual SCFuture<any_moveonly> getExportableFrameData(int frameNumber, TaskProgress& progress);

    /// Writes the exportable data of a single trajectory frame to the output file.
    [[nodiscard]] virtual SCFuture<void> exportFrameData(any_moveonly&& frameData, int frameNumber, const QString& filePath, TaskProgress& progress) = 0;

    /// Returns a pointer to the exporter to which this worker belongs.
    const FileExporter* exporter() const { return _exporter; }

    /// Returns the current file this exporter is writing to.
    QFile& outputFile() { return _outputFile; }

    /// Returns the text stream used to write into the current output file.
    CompressedTextWriter& textStream() { OVITO_ASSERT(_textStream.has_value()); return *_textStream; }

    /// Returns a reference to the specific data object to be exported.
    const DataObjectReference& dataObjectToExport() const { return exporter()->dataObjectToExport(); }

private:

    /// The exporter object this job belongs to.
    OORef<const FileExporter> _exporter;

    /// The output file.
    QFile _outputFile;

    /// The stream for writing text into the output file (optional).
    std::optional<CompressedTextWriter> _textStream;
};

}   // End of namespace
