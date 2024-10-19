////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
DEFINE_PROPERTY_FIELD(FileExporter, exportAnimation);
DEFINE_PROPERTY_FIELD(FileExporter, useWildcardFilename);
DEFINE_PROPERTY_FIELD(FileExporter, wildcardFilename);
DEFINE_PROPERTY_FIELD(FileExporter, startFrame);
DEFINE_PROPERTY_FIELD(FileExporter, endFrame);
DEFINE_PROPERTY_FIELD(FileExporter, everyNthFrame);
DEFINE_PROPERTY_FIELD(FileExporter, floatOutputPrecision);
DEFINE_REFERENCE_FIELD(FileExporter, datasetToExport);
DEFINE_REFERENCE_FIELD(FileExporter, sceneToExport);
DEFINE_REFERENCE_FIELD(FileExporter, sceneNodeToExport);
DEFINE_PROPERTY_FIELD(FileExporter, dataObjectToExport);
SET_PROPERTY_FIELD_LABEL(FileExporter, outputFilename, "Output filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, exportAnimation, "Export animation");
SET_PROPERTY_FIELD_LABEL(FileExporter, useWildcardFilename, "Use wildcard filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, wildcardFilename, "Wildcard filename");
SET_PROPERTY_FIELD_LABEL(FileExporter, startFrame, "Start frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, endFrame, "End frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(FileExporter, floatOutputPrecision, "Numeric output precision");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FileExporter, floatOutputPrecision, IntegerParameterUnit, 1, std::numeric_limits<FloatType>::max_digits10);
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(FileExporter, sceneNodeToExport, "nodeToExport"); // For backward compatibility with OVITO 3.9.2

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
    if(endFrame() < startFrame()) {
        if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(sceneNodeToExport())) {
            if(pipeline->head()) {
                int nframes = pipeline->head()->numberOfSourceFrames();
                int start = pipeline->head()->sourceFrameToAnimationTime(0).frame();
                if(start < startFrame()) setStartFrame(start);
                int end = (pipeline->head()->sourceFrameToAnimationTime(nframes) - 1).frame();
                if(end > endFrame()) setEndFrame(end);
            }
        }
    }

    // Export the entire animation interval of the scene when exporting the entire scene.
    if(sceneToExport() && endFrame() < startFrame()) {
        setStartFrame(sceneToExport()->animationSettings()->firstFrame());
        setEndFrame(sceneToExport()->animationSettings()->lastFrame());
    }

    // By default, export the data of the selected pipeline.
    if(!sceneNodeToExport() && sceneToExport()) {
        if(SceneNode* selectedNode = sceneToExport()->selection()->firstNode()) {
            if(isSuitableSceneNode(selectedNode)) {
                setSceneNodeToExport(selectedNode);
            }
        }
    }

    // If no scene node is currently selected, pick the first suitable node from the scene.
    if(!sceneNodeToExport() && sceneToExport()) {
        if(isSuitableSceneNode(sceneToExport())) {
            setSceneNodeToExport(sceneToExport());
        }
        else {
            sceneToExport()->visitChildren([this](SceneNode* node) {
                if(isSuitableSceneNode(node)) {
                    setSceneNodeToExport(node);
                    return false;
                }
                return true;
            });
        }
    }
}

/******************************************************************************
* Determines whether the given scene node is suitable for this file exporter service.
* By default, all pipeline scene nodes are considered suitable that produce
* suitable data objects of the type specified by the FileExporter::exportableDataObjectClass() method.
******************************************************************************/
bool FileExporter::isSuitableSceneNode(SceneNode* node) const
{
    if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(node)) {
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
bool FileExporter::isSuitablePipelineOutput(const PipelineFlowState& state) const
{
    if(!state)
        return false;
    std::vector<DataObjectClassPtr> objClasses = exportableDataObjectClass();
    if(objClasses.empty())
        return true;
    for(DataObjectClassPtr objClass : objClasses) {
        if(state.containsObjectRecursive(*objClass))
            return true;
    }
    return false;
}

/******************************************************************************
* Evaluates the pipeline whose data is to be exported.
******************************************************************************/
PipelineFlowState FileExporter::getPipelineDataToBeExported(int frame) const
{
    if(!sceneToExport())
        throw Exception(tr("No scene has been specified for file export."));

    Pipeline* pipeline = dynamic_object_cast<Pipeline>(sceneNodeToExport());
    if(!pipeline)
        throw Exception(tr("The scene node to be exported is not a data pipeline."));

    try {
        // Evaluate pipeline.
        PipelineEvaluationRequest request(AnimationTime::fromFrame(frame), this_task::isScripting());
        PipelineEvaluationResult result = pipeline->evaluatePipeline(request);
        const PipelineFlowState& state = result.blockForResult();

        if(this_task::isScripting() && state.status().type() == PipelineStatus::Error)
            throw Exception(state.status().text());

        if(!state)
            throw Exception(tr("The data collection returned by the pipeline is empty."));

        return state;
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Export of frame %1 failed, because data pipeline evaluation has failed.").arg(frame));
    }
}

/******************************************************************************
 * Exports the scene data to the output file(s).
 *****************************************************************************/
void FileExporter::doExport()
{
    if(outputFilename().isEmpty())
        throw Exception(tr("The output filename not been set for the file exporter."));

    if(startFrame() > endFrame())
        throw Exception(tr("The animation interval to be exported is empty or has not been set."));

    if(!sceneToExport())
        throw Exception(tr("No scene has been specified for file export."));

    if(!sceneNodeToExport()) {
        QString errorMsg = tr("There is no data in the current scene that can be exported to the selected file format.");
        const std::vector<DataObjectClassPtr>& objClasses = exportableDataObjectClass();
        if(!objClasses.empty()) {
            errorMsg += tr("\n\nThe selected output format (%1) requires one of the following data types to be present in the pipeline output:\n").arg(getOOMetaClass().fileFilterDescription());
            for(const DataObjectClassPtr& clazz : objClasses)
                errorMsg += QStringLiteral("\n%1").arg(clazz->displayName());
        }
        throw Exception(std::move(errorMsg));
    }

    // Compute the number of frames that need to be exported.
    int firstFrameNumber, numberOfFrames;
    if(exportAnimation()) {
        firstFrameNumber = startFrame();
        numberOfFrames = (endFrame() - startFrame() + everyNthFrame()) / everyNthFrame();
        if(numberOfFrames < 1 || everyNthFrame() < 1)
            throw Exception(tr("Invalid export animation range: Frame %1 to %2").arg(startFrame()).arg(endFrame()));
    }
    else {
        firstFrameNumber = sceneToExport()->animationSettings()->currentFrame();
        numberOfFrames = 1;
    }

    // Validate export settings.
    if(exportAnimation() && useWildcardFilename()) {
        if(wildcardFilename().isEmpty())
            throw Exception(tr("Cannot write animation frame to separate files. Wildcard pattern has not been specified."));
        if(!wildcardFilename().contains(QChar('*')))
            throw Exception(tr("Cannot write animation frames to separate files. The filename must contain the '*' wildcard character, which gets replaced by the frame number."));
    }

    QDir dir = QFileInfo(outputFilename()).dir();
    QString filename = outputFilename();

    // Open output file for writing.
    if(!exportAnimation() || !useWildcardFilename()) {
        openOutputFile(filename, numberOfFrames);
    }

    try {
        // Export animation frames.
        this_task::beginProgressSubSteps(numberOfFrames);
        for(int frameIndex = 0; frameIndex < numberOfFrames; frameIndex++) {
            if(frameIndex != 0)
                this_task::nextProgressSubStep();

            int frameNumber = firstFrameNumber + frameIndex * everyNthFrame();

            // Open per-frame output file.
            if(exportAnimation() && useWildcardFilename()) {
                // Generate an output filename based on the wildcard pattern.
                filename = dir.absoluteFilePath(QFileInfo(wildcardFilename()).fileName());
                filename.replace(QChar('*'), QString::number(frameNumber));
                openOutputFile(filename, 1);
            }

            this_task::setProgressText(tr("Exporting frame %1 to file '%2'").arg(frameNumber).arg(filename));

            exportFrame(frameNumber, filename);

            // Close per-frame output file.
            if(exportAnimation() && useWildcardFilename())
                closeOutputFile(!this_task::isCanceled());

            this_task::throwIfCanceled();
        }
        this_task::endProgressSubSteps();
    }
    catch(...) {
        closeOutputFile(false);
        throw;
    }

    // Close output file.
    if(!exportAnimation() || !useWildcardFilename()) {
        closeOutputFile(true);
    }
}

/******************************************************************************
* Returns a string with the list of available data objects of the given type.
******************************************************************************/
QString FileExporter::getAvailableDataObjectList(const PipelineFlowState& state, const DataObject::OOMetaClass& objectType) const
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

}   // End of namespace
