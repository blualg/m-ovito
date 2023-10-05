////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/undo/RefTargetOperations.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/StaticSource.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/stdobj/camera/TargetObject.h>
#include "StandardCameraSource.h"
#include "StandardCameraObject.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(StandardCameraSource);
DEFINE_PROPERTY_FIELD(StandardCameraSource, isPerspective);
DEFINE_REFERENCE_FIELD(StandardCameraSource, fovController);
DEFINE_REFERENCE_FIELD(StandardCameraSource, zoomController);
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, isPerspective, "Perspective projection");
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, fovController, "FOV angle");
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, zoomController, "FOV size");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardCameraSource, fovController, AngleParameterUnit, FloatType(1e-3), FLOATTYPE_PI - FloatType(1e-2));
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StandardCameraSource, zoomController, WorldParameterUnit, 0);

/******************************************************************************
* Constructs a camera object.
******************************************************************************/
StandardCameraSource::StandardCameraSource(ObjectInitializationFlags flags) : PipelineNode(flags),
    _isPerspective(true)
{
    pipelineCache().setEnabled(false);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setFovController(ControllerManager::createFloatController());
        fovController()->setFloatValue(AnimationTime(0), FLOATTYPE_PI/4);

        setZoomController(ControllerManager::createFloatController());
        zoomController()->setFloatValue(AnimationTime(0), 200);

        // Adopt the view parameters from the currently active Viewport.
        if(ExecutionContext::current().isInteractive()) {
            if(Viewport* vp = ExecutionContext::current().ui().datasetContainer().activeViewport()) {
                setIsPerspective(vp->isPerspectiveProjection());
                if(vp->isPerspectiveProjection())
                    fovController()->setFloatValue(AnimationTime(0), vp->fieldOfView());
                else
                    zoomController()->setFloatValue(AnimationTime(0), vp->fieldOfView());
            }
        }
    }
}

/******************************************************************************
* Asks the object for its validity interval at the given time.
******************************************************************************/
TimeInterval StandardCameraSource::validityInterval(const PipelineEvaluationRequest& request) const
{
    TimeInterval interval = PipelineNode::validityInterval(request);
    if(isPerspective() && fovController())
        interval.intersect(fovController()->validityInterval(request.time()));
    if(!isPerspective() && zoomController())
        interval.intersect(zoomController()->validityInterval(request.time()));
    return interval;
}

/******************************************************************************
* Asks the pipeline stage to compute the results.
******************************************************************************/
PipelineFlowState StandardCameraSource::evaluateInternalSynchronous(const PipelineEvaluationRequest& request)
{
    // Create a new DataCollection.
    DataOORef<DataCollection> data = DataOORef<DataCollection>::create();

    // Set up the camera data object.
    DataOORef<StandardCameraObject> camera = DataOORef<StandardCameraObject>::create();
    camera->setCreatedByNode(this);
    camera->setIsPerspective(isPerspective());
    TimeInterval stateValidity = TimeInterval::infinite();
    if(fovController()) camera->setFov(fovController()->getFloatValue(request.time(), stateValidity));
    if(zoomController()) camera->setZoom(zoomController()->getFloatValue(request.time(), stateValidity));
    data->addObject(std::move(camera));

    // Wrap the DataCollection in a PipelineFlowState.
    return PipelineFlowState(std::move(data), PipelineStatus::Success, stateValidity);
}

/******************************************************************************
* Returns the current orthogonal field of view.
******************************************************************************/
FloatType StandardCameraSource::zoom() const
{
    if(zoomController())
        return zoomController()->getFloatValue(AnimationTime(0));
    return 200;
}

/******************************************************************************
* Sets the field of view of a parallel projection camera.
******************************************************************************/
void StandardCameraSource::setZoom(FloatType newFOV)
{
    if(zoomController())
        zoomController()->setFloatValue(AnimationTime(0), newFOV);
}

/******************************************************************************
* Returns the current perspective field of view angle.
******************************************************************************/
FloatType StandardCameraSource::fov() const
{
    if(fovController())
        return fovController()->getFloatValue(AnimationTime(0));
    return FLOATTYPE_PI / 4;
}

/******************************************************************************
* Sets the field of view angle of a perspective projection camera.
******************************************************************************/
void StandardCameraSource::setFov(FloatType newFOV)
{
    if(fovController())
        fovController()->setFloatValue(AnimationTime(0), newFOV);
}

/******************************************************************************
* Returns whether this camera is a target camera directed at a target object.
******************************************************************************/
bool StandardCameraSource::isTargetCamera() const
{
    for(Pipeline* pipeline : pipelines(true)) {
        if(pipeline->lookatTargetNode() != nullptr)
            return true;
    }
    return false;
}

/******************************************************************************
* For a target camera, queries the distance between the camera and its target.
******************************************************************************/
FloatType StandardCameraSource::targetDistance(AnimationTime time) const
{
    for(Pipeline* pipeline : pipelines(true)) {
        if(pipeline->lookatTargetNode() != nullptr) {
            return StandardCameraObject::getTargetDistance(time, pipeline);
        }
    }

    return StandardCameraObject::getTargetDistance(time, nullptr);
}

/******************************************************************************
* Changes the type of the camera to a target camera or a free camera.
******************************************************************************/
void StandardCameraSource::setIsTargetCamera(bool enable)
{
    pushIfUndoRecording<TargetChangedUndoOperation>(this);

    // Use current scene animation time to initialize camera objects.
    AnimationTime time = ExecutionContext::current().ui().datasetContainer().currentAnimationTime();

    for(Pipeline* pipeline : pipelines(true)) {
        if(pipeline->lookatTargetNode() == nullptr && enable) {
            if(SceneNode* parentNode = pipeline->parentNode()) {
                DataOORef<DataCollection> dataCollection = DataOORef<DataCollection>::create();
                dataCollection->addObject(DataOORef<TargetObject>::create());
                OORef<StaticSource> targetSource = OORef<StaticSource>::create(dataCollection);
                OORef<Pipeline> targetNode = OORef<Pipeline>::create();
                targetNode->setHead(targetSource);
                targetNode->setSceneNodeName(tr("%1.target").arg(pipeline->sceneNodeName()));
                parentNode->addChildNode(targetNode);
                // Position the new target to match the current orientation of the camera.
                TimeInterval iv;
                const AffineTransformation& cameraTM = pipeline->getWorldTransform(time, iv);
                Vector3 cameraPos = cameraTM.translation();
                Vector3 cameraDir = cameraTM.column(2).normalized();
                Vector3 targetPos = cameraPos - targetDistance(time) * cameraDir;
                targetNode->transformationController()->translate(time, targetPos, AffineTransformation::Identity());
                pipeline->setLookatTargetNode(time, targetNode);
            }
        }
        else if(pipeline->lookatTargetNode() != nullptr && !enable) {
            pipeline->lookatTargetNode()->deleteSceneNode();
            OVITO_ASSERT(pipeline->lookatTargetNode() == nullptr);
        }
    }

    pushIfUndoRecording<TargetChangedRedoOperation>(this);
    notifyTargetChanged();
}

}   // End of namespace
