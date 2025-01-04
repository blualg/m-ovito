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

IMPLEMENT_CREATABLE_OVITO_CLASS(StandardCameraSource);
OVITO_CLASSINFO(StandardCameraSource, "DisplayName", "Camera");
DEFINE_PROPERTY_FIELD(StandardCameraSource, isPerspective);
DEFINE_REFERENCE_FIELD(StandardCameraSource, fovController);
DEFINE_REFERENCE_FIELD(StandardCameraSource, zoomController);
DEFINE_REFERENCE_FIELD(StandardCameraSource, cameraVis);
DEFINE_VIRTUAL_PROPERTY_FIELD(StandardCameraSource, isTargetCamera, setIsTargetCamera);
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, isPerspective, "Perspective projection");
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, fovController, "FOV angle");
SET_PROPERTY_FIELD_LABEL(StandardCameraSource, zoomController, "FOV size");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardCameraSource, fovController, AngleParameterUnit, FloatType(1e-3), FLOATTYPE_PI - FloatType(1e-2));
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StandardCameraSource, zoomController, WorldParameterUnit, 0);

/******************************************************************************
* Constructs a camera object.
******************************************************************************/
void StandardCameraSource::initializeObject(ObjectInitializationFlags flags)
{
    AbstractCameraSource::initializeObject(flags);

    pipelineCache().setEnabled(false);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setFovController(ControllerManager::createFloatController());
        fovController()->setFloatValue(AnimationTime(0), FLOATTYPE_PI / 4);

        setZoomController(ControllerManager::createFloatController());
        zoomController()->setFloatValue(AnimationTime(0), 200);

        setCameraVis(OORef<CameraVis>::create(flags));

        // Adopt the view parameters from the currently active Viewport.
        if(this_task::isInteractive()) {
            if(Viewport* vp = this_task::ui()->datasetContainer().activeViewport()) {
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
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void StandardCameraSource::loadFromStreamComplete(ObjectLoadStream& stream)
{
    AbstractCameraSource::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.10.6:
    if(!cameraVis())
        setCameraVis(OORef<CameraVis>::create());
}

/******************************************************************************
* Lets the source generate a camera object for the given animation time.
******************************************************************************/
DataOORef<const AbstractCameraObject> StandardCameraSource::cameraObject(AnimationTime time) const
{
    // Set up the camera data object.
    DataOORef<StandardCameraObject> camera = DataOORef<StandardCameraObject>::create(ObjectInitializationFlag::DontCreateVisElement);
    camera->setCreatedByNode(this);
    camera->setVisElement(cameraVis());
    camera->setIsPerspective(isPerspective());
    TimeInterval stateValidity = TimeInterval::infinite();
    if(fovController())
        camera->setFov(fovController()->getFloatValue(time, stateValidity));
    if(zoomController())
        camera->setZoom(zoomController()->getFloatValue(time, stateValidity));
    return camera;
}

/******************************************************************************
* Called by the pipeline system before a new evaluation begins to query the
* validity interval and evaluation result type of this pipeline stage.
******************************************************************************/
void StandardCameraSource::preevaluateInternal(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval)
{
    if(fovController())
        validityInterval.intersect(fovController()->validityInterval(request.time()));
    if(zoomController())
        validityInterval.intersect(zoomController()->validityInterval(request.time()));
}

/******************************************************************************
* Asks the pipeline stage to compute the results.
******************************************************************************/
SharedFuture<PipelineFlowState> StandardCameraSource::evaluateInternal(const PipelineEvaluationRequest& request)
{
    // Create a new DataCollection.
    DataOORef<DataCollection> data = DataOORef<DataCollection>::create();

    // Set up the camera data object.
    DataOORef<StandardCameraObject> camera = DataOORef<StandardCameraObject>::create(ObjectInitializationFlag::DontCreateVisElement);
    camera->setCreatedByNode(this);
    camera->setVisElement(cameraVis());
    camera->setIsPerspective(isPerspective());
    TimeInterval stateValidity = TimeInterval::infinite();
    if(fovController())
        camera->setFov(fovController()->getFloatValue(request.time(), stateValidity));
    if(zoomController())
        camera->setZoom(zoomController()->getFloatValue(request.time(), stateValidity));
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
        for(SceneNode* sceneNode : pipeline->sceneNodes()) {
            if(sceneNode->lookatTargetNode() != nullptr)
                return true;
        }
    }
    return false;
}

/******************************************************************************
* For a target camera, queries the distance between the camera and its target.
******************************************************************************/
FloatType StandardCameraSource::targetDistance(AnimationTime time) const
{
    for(Pipeline* pipeline : pipelines(true)) {
        for(SceneNode* sceneNode : pipeline->sceneNodes()) {
            if(sceneNode->lookatTargetNode() != nullptr) {
                return StandardCameraObject::getTargetDistance(time, sceneNode);
            }
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
    AnimationTime time = this_task::ui()->datasetContainer().currentAnimationTime();

    for(Pipeline* pipeline : pipelines(true)) {
        for(SceneNode* sceneNode : pipeline->sceneNodes()) {
            if(sceneNode->lookatTargetNode() == nullptr && enable) {
                if(SceneNode* parentNode = sceneNode->parentNode()) {
                    DataOORef<DataCollection> dataCollection = DataOORef<DataCollection>::create();
                    dataCollection->addObject(DataOORef<TargetObject>::create());
                    OORef<StaticSource> targetSource = OORef<StaticSource>::create();
                    targetSource->setDataCollection(std::move(dataCollection));
                    OORef<Pipeline> targetPipeline = OORef<Pipeline>::create();
                    targetPipeline->setHead(targetSource);
                    OORef<SceneNode> targetNode = OORef<SceneNode>::create();
                    targetNode->setPipeline(targetPipeline);
                    targetNode->setSceneNodeName(tr("%1.target").arg(sceneNode->sceneNodeName()));
                    parentNode->addChildNode(targetNode);
                    // Position the new target to match the current orientation of the camera.
                    const AffineTransformation& cameraTM = sceneNode->getWorldTransform(time);
                    Vector3 cameraPos = cameraTM.translation();
                    Vector3 cameraDir = cameraTM.column(2).normalized();
                    Vector3 targetPos = cameraPos - targetDistance(time) * cameraDir;
                    targetNode->transformationController()->translate(time, targetPos, AffineTransformation::Identity());
                    sceneNode->setLookatTargetNode(time, targetNode);
                }
            }
            else if(sceneNode->lookatTargetNode() != nullptr && !enable) {
                sceneNode->lookatTargetNode()->requestObjectDeletion();
                OVITO_ASSERT(sceneNode->lookatTargetNode() == nullptr);
            }
        }
    }

    pushIfUndoRecording<TargetChangedRedoOperation>(this);
    notifyTargetChanged();
}

}   // End of namespace
