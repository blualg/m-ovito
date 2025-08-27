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
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "FileExporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FileExporter);
DEFINE_PROPERTY_FIELD(FileExporter, outputFilename);
DEFINE_PROPERTY_FIELD(FileExporter, exportTrajectory);
DEFINE_PROPERTY_FIELD(FileExporter, useWildcardFilename);
DEFINE_PROPERTY_FIELD(FileExporter, wildcardFilename);
DEFINE_PROPERTY_FIELD(FileExporter, startFrame);
DEFINE_PROPERTY_FIELD(FileExporter, endFrame);
DEFINE_PROPERTY_FIELD(FileExporter, everyNthFrame);
DEFINE_PROPERTY_FIELD(FileExporter, floatOutputPrecision);
DEFINE_REFERENCE_FIELD(FileExporter, datasetToExport);
DEFINE_REFERENCE_FIELD(FileExporter, sceneToExport);
DEFINE_REFERENCE_FIELD(FileExporter, pipelineToExport);
DEFINE_PROPERTY_FIELD(FileExporter, dataObjectToExport);
SET_PROPERTY_FIELD_LABEL(FileExporter, outputFilename, "Output filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, exportTrajectory, "Export trajectory");
SET_PROPERTY_FIELD_LABEL(FileExporter, useWildcardFilename, "Use wildcard filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, wildcardFilename, "Wildcard filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, startFrame, "Start frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, endFrame, "End frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, floatOutputPrecision, "Numeric output precision");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FileExporter, floatOutputPrecision, IntegerParameterUnit, 1, std::numeric_limits<FloatType>::max_digits10);

IMPLEMENT_ABSTRACT_OVITO_CLASS(FileExportJob);

/******************************************************************************
* Sets the name of the output file that should be written by this exporter.
******************************************************************************/
void FileExporter::setOutputFilename(const QString& filename)
{
    _outputFilename.set(this, PROPERTY_FIELD(outputFilename), filename);

    // Generate a default wildcard pattern from the filename.
    if(wildcardFilename().isEmpty()) {
        QString fn = QFileInfo(filename).fileName();
        if(!fn.contains('*')) {
            int dotIndex = fn.lastIndexOf('.');
            if(dotIndex > 0)
                setWildcardFilename(fn.left(dotIndex) + QStringLiteral(".*") + fn.mid(dotIndex));
            else
                setWildcardFilename(fn + QStringLiteral(".*"));
        }
        else
            setWildcardFilename(fn);
    }
}

/******************************************************************************
* Selects the default scene node to be exported by this exporter.
******************************************************************************/
void FileExporter::selectDefaultExportableData(DataSet* dataset, Scene* scene)
{
    if(!datasetToExport())
        setDatasetToExport(dataset);

    if(!sceneToExport())
        setSceneToExport(scene);

    // Export the entire frame interval of the selected pipeline by default.
    if(endFrame() < startFrame() && pipelineToExport()) {
        if(pipelineToExport()->head()) {
            int nframes = pipelineToExport()->head()->numberOfSourceFrames();
            int start = pipelineToExport()->head()->sourceFrameToAnimationTime(0).frame();
            if(start < startFrame()) setStartFrame(start);
            int end = (pipelineToExport()->head()->sourceFrameToAnimationTime(nframes) - 1).frame();
            if(end > endFrame()) setEndFrame(end);
        }
    }

    // Export the entire animation interval of the scene when exporting the entire scene.
    if(sceneToExport() && endFrame() < startFrame()) {
        setStartFrame(sceneToExport()->animationSettings()->firstFrame());
        setEndFrame(sceneToExport()->animationSettings()->lastFrame());
    }

    // By default, export the data of the selected pipeline.
    if(!pipelineToExport() && sceneToExport()) {
        if(SceneNode* selectedNode = sceneToExport()->selection()->firstNode()) {
            if(isSuitableSceneNode(selectedNode)) {
                setPipelineToExport(selectedNode->pipeline());
            }
        }
    }

    // If no pipeline is currently selected, pick the first suitable pipeline from the scene.
    if(!pipelineToExport() && sceneToExport()) {
        sceneToExport()->visitChildren([this](SceneNode* node) {
            if(isSuitableSceneNode(node)) {
                setPipelineToExport(node->pipeline());
                return false;
            }
            return true;
        });
    }
}

/******************************************************************************
* Determines whether the given scene node is suitable for this file exporter service.
* By default, all pipeline scene nodes are considered suitable that produce
* suitable data objects of the type specified by the FileExporter::exportableDataObjectClass() method.
******************************************************************************/
bool FileExporter::isSuitableSceneNode(SceneNode* node)
{
    if(Pipeline* pipeline = node->pipeline()) {
        if(sceneToExport()) {
            AnimationTime time = sceneToExport()->animationSettings()->currentTime();
            return isSuitablePipelineOutput(pipeline->evaluatePipeline(PipelineEvaluationRequest(time)).blockForResult());
        }
    }
    return false;
}

/******************************************************************************
* Determines whether the given pipeline output is suitable for exporting with
* this exporter service. By default, all data collections are considered suitable
* that contain suitable data objects of the type specified by the
* FileExporter::exportableDataObjectClass() method.
******************************************************************************/
bool FileExporter::isSuitablePipelineOutput(const PipelineFlowState& state)
{
    if(!state)
        return false;
    std::vector<DataObjectClassPtr> objClasses = exportableDataObjectClass();
    if(objClasses.empty())
        return true;
    for(DataObjectClassPtr objClass : objClasses) {
        for(const ConstDataObjectPath& dataPath : state.data()->getObjectsRecursive(*objClass)) {
            if(isSuitableDataObject(dataPath)) {
                return true;
            }
        }
    }
    return false;
}

/******************************************************************************
* Evaluates the pipeline whose data is to be exported.
******************************************************************************/
Future<PipelineFlowState> FileExporter::getPipelineDataToBeExported(int frameNumber) const
{
    if(!sceneToExport())
        throw Exception(tr("No scene has been specified for file export."));
    if(!pipelineToExport())
        throw Exception(tr("No pipeline has been specified for file export."));

    try {
        // Evaluate pipeline.
        PipelineEvaluationRequest request(AnimationTime::fromFrame(frameNumber), this_task::isScripting());
        PipelineFlowState state = co_await pipelineToExport()->evaluatePipeline(request).asFuture();

        if(this_task::isScripting() && state.status().type() == PipelineStatus::Error)
            throw Exception(state.status().text());

        if(!state)
            throw Exception(tr("The data collection returned by the pipeline is empty."));

        co_return state;
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Export of frame %1 failed, because data pipeline evaluation has failed.").arg(frameNumber));
    }
}

/*****************************************************************************
* Exports the scene data to the output file(s).
*****************************************************************************/
Future<void> FileExporter::performExport()
{
    if(outputFilename().isEmpty())
        throw Exception(tr("The output filename not been set for the file exporter."));

    if(startFrame() > endFrame())
        throw Exception(tr("The trajectory interval to be exported is empty or has not been set."));

    if(!sceneToExport())
        throw Exception(tr("No scene has been specified for file export."));

    if(!pipelineToExport() && !isSuitableSceneNode(sceneToExport())) {
        QString errorMsg = tr("There is no object in the current scene that can be exported to the selected file format.");
        const std::vector<DataObjectClassPtr>& objClasses = exportableDataObjectClass();
        if(!objClasses.empty()) {
            errorMsg += tr("\n\nThe selected output format (%1) requires one of the following data types to be present in a pipeline's output:\n").arg(objectTitle());
            for(const DataObjectClassPtr& clazz : objClasses)
                errorMsg += QStringLiteral("\n%1").arg(clazz->displayName());
        }
        throw Exception(std::move(errorMsg));
    }

    // Validate export settings.
    if(exportTrajectory() && useWildcardFilename()) {
        if(wildcardFilename().isEmpty())
            throw Exception(tr("Cannot export trajectory frames to separate files. Wildcard pattern has not been specified."));
        if(!wildcardFilename().contains(QChar('*')))
            throw Exception(tr("Cannot export trajectory frames to separate files. The filename must contain the '*' wildcard character, which gets replaced by the frame number."));
    }

    // Compute the number of frames that need to be exported.
    int firstFrame, lastFrame, frameStepSize;
    if(exportTrajectory()) {
        firstFrame = startFrame();
        lastFrame = endFrame();
        frameStepSize = everyNthFrame();
        if(firstFrame > lastFrame || frameStepSize < 1)
            throw Exception(tr("Invalid export trajectory range: Frame %1 to %2 (step size %3)").arg(firstFrame).arg(lastFrame).arg(frameStepSize));
    }
    else {
        firstFrame = lastFrame = sceneToExport()->animationSettings()->currentFrame();
        frameStepSize = 1;
    }

    return exportFrames(firstFrame, lastFrame, frameStepSize);
}

/*****************************************************************************
* Writes the given sequence of frames to the output file(s).
*****************************************************************************/
Future<void> FileExporter::exportFrames(int firstFrame, int lastFrame, int frameStepSize)
{
    OVITO_ASSERT(firstFrame <= lastFrame && frameStepSize > 0);
    int numberOfFrames = (lastFrame - firstFrame + frameStepSize) / frameStepSize;

    // Keep the FileExporter object alive during the export operation.
    OORef<FileExporter> self(this);

    QDir dir = QFileInfo(outputFilename()).dir();
    QString filename = outputFilename();

    // Open output file for writing.
    OORef<FileExportJob> exportJob;
    if(!exportTrajectory() || !useWildcardFilename()) {
        exportJob = createExportJob(filename, numberOfFrames);
    }

    // Set up progress reporting.
    TaskProgress progress(this_task::ui());
    progress.beginSubSteps(numberOfFrames);

    // Export animation frames.
    for(int frameIndex = 0; frameIndex < numberOfFrames; frameIndex++, progress.nextSubStep()) {
        int frameNumber = firstFrame + frameIndex * everyNthFrame();

        // Open per-frame output file.
        if(exportTrajectory() && useWildcardFilename()) {
            // Generate an output filename based on the wildcard pattern.
            filename = dir.absoluteFilePath(QFileInfo(wildcardFilename()).fileName());
            filename.replace(QChar('*'), QString::number(frameNumber));
            exportJob = createExportJob(filename, 1);
        }

        progress.setText(tr("Exporting frame %1 to file '%2'").arg(frameNumber).arg(filename));

        // Obtain the data to be exported.
        boost::anys::unique_any frameData =
            co_await FutureAwaiter(DeferredObjectExecutor(this), exportJob->getExportableFrameData(frameNumber, progress));
        OVITO_ASSERT(frameData.has_value());

        // Write the exportable data to the output file.
        co_await FutureAwaiter(DeferredObjectExecutor(this), exportJob->exportFrameData(std::move(frameData), frameNumber, filename, progress));

        // Close per-frame output file.
        if(exportTrajectory() && useWildcardFilename()) {
            exportJob->close();
            exportJob.reset();
        }
    }
    progress.endSubSteps();

    // Close output file.
    if(!exportTrajectory() || !useWildcardFilename())
        exportJob->close();
}

/******************************************************************************
* Returns a string with the list of available data objects of the given type.
******************************************************************************/
QString FileExporter::getAvailableDataObjectList(const PipelineFlowState& state, const DataObject::OOMetaClass& objectType)
{
    QString str;
    if(state) {
        for(const ConstDataObjectPath& dataPath : state.data()->getObjectsRecursive(objectType)) {
            QString pathString = dataPath.toString();
            if(!pathString.isEmpty()) {
                if(!str.isEmpty()) str += QStringLiteral(", ");
                str += pathString;
            }
        }
    }
    if(str.isEmpty())
        str = tr("<none>");
    return str;
}

/******************************************************************************
* Produces the data to be exported for a trajectory frame.
******************************************************************************/
SCFuture<boost::anys::unique_any> FileExportJob::getExportableFrameData(int frameNumber, TaskProgress& progress)
{
    co_return co_await exporter()->getPipelineDataToBeExported(frameNumber);
}

/******************************************************************************
* Constructor.
******************************************************************************/
void FileExportJob::initializeObject(const FileExporter* exporter, const QString& filePath, bool openTextStream)
{
    OvitoObject::initializeObject();

    _exporter = exporter;
    _outputFile.setFileName(filePath);
    if(openTextStream) {
        _textStream.emplace(_outputFile);
        _textStream->setFloatPrecision(exporter->floatOutputPrecision());
    }
}

/******************************************************************************
* Destructor.
******************************************************************************/
void FileExportJob::aboutToBeDeleted()
{
    close(false);
    OvitoObject::aboutToBeDeleted();
}

/******************************************************************************
* Called after an output file has been successfully written and before the worker is destroyed.
******************************************************************************/
void FileExportJob::close(bool exportCompleted)
{
    // Close text stream.
    _textStream.reset();

    // Close underlying file.
    if(_outputFile.isOpen()) {
        _outputFile.close();

        // Delete incomplete file from disk if it is still open.
        if(!exportCompleted)
            _outputFile.remove();
    }
}

}   // End of namespace
