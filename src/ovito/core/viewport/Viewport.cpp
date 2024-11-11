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
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/data/camera/AbstractCameraObject.h>
#include <ovito/core/dataset/data/camera/AbstractCameraSource.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>

/// The default field of view in world units used for orthogonal view types when the scene is empty.
#define DEFAULT_ORTHOGONAL_FIELD_OF_VIEW        FloatType(200)

/// The default field of view angle in radians used for perspective view types when the scene is empty.
#define DEFAULT_PERSPECTIVE_FIELD_OF_VIEW       FloatType(35*FLOATTYPE_PI/180)

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Viewport);
DEFINE_PROPERTY_FIELD(Viewport, viewType);
DEFINE_PROPERTY_FIELD(Viewport, gridMatrix);
DEFINE_PROPERTY_FIELD(Viewport, fieldOfView);
DEFINE_PROPERTY_FIELD(Viewport, cameraTransformation);
DEFINE_PROPERTY_FIELD(Viewport, cameraUpDirection);
DEFINE_PROPERTY_FIELD(Viewport, renderPreviewMode);
DEFINE_PROPERTY_FIELD(Viewport, isGridVisible);
DEFINE_PROPERTY_FIELD(Viewport, viewportTitle);
DEFINE_REFERENCE_FIELD(Viewport, viewNode);
DEFINE_REFERENCE_FIELD(Viewport, scene);
DEFINE_VECTOR_REFERENCE_FIELD(Viewport, overlays);
DEFINE_VECTOR_REFERENCE_FIELD(Viewport, underlays);
SET_PROPERTY_FIELD_CHANGE_EVENT(Viewport, viewportTitle, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
void Viewport::initializeObject(ObjectInitializationFlags flags)
{
    RefTarget::initializeObject(flags);

    // Get notified when the global viewport settings change.
    _viewportSettingsChangedConnection = QObject::connect(&ViewportSettings::getSettings(), &ViewportSettings::settingsChanged, [this](ViewportSettings* newSettings) { viewportSettingsChanged(newSettings); });

    // Automatically associate the new viewport with the global scene (if there is one).
    // This is needed for the Python interface, where each viewport created by the user must be automatically
    // associated with some scene.
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject) && this_task::isScripting()) {
        setScene(this_task::ui()->datasetContainer().activeScene());
    }
}

/******************************************************************************
* Destructor.
******************************************************************************/
Viewport::~Viewport()
{
    QObject::disconnect(_viewportSettingsChangedConnection);
}

/******************************************************************************
* Changes the view type.
******************************************************************************/
void Viewport::setViewType(ViewType type, bool keepCameraTransformation, bool keepFieldOfView)
{
    if(type == viewType())
        return;

    // Remember the original projection type.
    bool wasPerspectiveProjection = isPerspectiveProjection();

    // Reset camera node.
    if(type != VIEW_SCENENODE)
        setViewNode(nullptr);

    // Setup default view.
    Matrix3 coordSys = ViewportSettings::getSettings().coordinateSystemOrientation();
    switch(type) {
        case VIEW_TOP:
            setCameraTransformation(AffineTransformation(coordSys));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_BOTTOM:
            setCameraTransformation(AffineTransformation(coordSys * Matrix3(-1,0,0, 0,1,0, 0,0,-1)));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_LEFT:
            setCameraTransformation(AffineTransformation(coordSys * Matrix3(0,0,-1, -1,0,0, 0,1,0)));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_RIGHT:
            setCameraTransformation(AffineTransformation(coordSys * Matrix3(0,0,1, 1,0,0, 0,1,0)));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_FRONT:
            setCameraTransformation(AffineTransformation(coordSys * Matrix3(1,0,0, 0,0,-1, 0,1,0)));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_BACK:
            setCameraTransformation(AffineTransformation(coordSys * Matrix3(-1,0,0, 0,0,1, 0,1,0)));
            setGridMatrix(cameraTransformation());
            break;
        case VIEW_ORTHO:
            if(!keepCameraTransformation) {
                setCameraPosition(Point3::Origin());
                if(viewType() == VIEW_NONE)
                    setCameraTransformation(AffineTransformation(coordSys));
            }
            setGridMatrix(AffineTransformation(coordSys));
            break;
        case VIEW_PERSPECTIVE:
            if(!keepCameraTransformation) {
                if(viewType() >= VIEW_TOP && viewType() <= VIEW_ORTHO) {
                    setCameraPosition(cameraPosition() - (cameraDirection().normalized() * fieldOfView()));
                }
                else if(viewType() != VIEW_PERSPECTIVE) {
                    setCameraPosition(ViewportSettings::getSettings().coordinateSystemOrientation() * Point3(0,0,-50));
                    setCameraDirection(ViewportSettings::getSettings().coordinateSystemOrientation() * Vector3(0,0,1));
                }
            }
            setGridMatrix(AffineTransformation(coordSys));
            break;
        case VIEW_SCENENODE:
            if(!keepCameraTransformation && viewNode() && scene()) {
                setCameraTransformation(viewNode()->getWorldTransform(scene()->animationSettings()->currentTime()));
            }
            setGridMatrix(AffineTransformation(coordSys));
            break;
        case VIEW_NONE:
            setGridMatrix(AffineTransformation(coordSys));
            break;
    }

    if(!keepFieldOfView) {
        // Reset to standard fov/zoom value when switching between perspective and parallel projections.
        if(type == VIEW_PERSPECTIVE) {
            if(!wasPerspectiveProjection || viewType() == VIEW_NONE)
                setFieldOfView(DEFAULT_PERSPECTIVE_FIELD_OF_VIEW);
        }
        else if(type != VIEW_SCENENODE) {
            if(wasPerspectiveProjection || viewType() == VIEW_NONE)
                setFieldOfView(DEFAULT_ORTHOGONAL_FIELD_OF_VIEW);
        }
        else if(type == VIEW_SCENENODE && viewNode() && scene()) {
            if(DataOORef<const AbstractCameraObject> camera = cameraObject(scene()->animationSettings()->currentTime())) {
                TimeInterval iv;
                setFieldOfView(camera->fieldOfView(scene()->animationSettings()->currentTime(), iv));
            }
        }
    }
    else {
        if(type == VIEW_PERSPECTIVE && fieldOfView() >= qDegreesToRadians(90.0)) {
            setFieldOfView(DEFAULT_PERSPECTIVE_FIELD_OF_VIEW);
        }
    }

    _viewType.set(this, PROPERTY_FIELD(viewType), type);
}

/******************************************************************************
* Returns the viewing direction of the camera.
******************************************************************************/
Vector3 Viewport::cameraDirection() const
{
    if(cameraTransformation().column(2) == Vector3::Zero())
        return Vector3(0,0,1);
    else
        return -cameraTransformation().column(2);
}

/******************************************************************************
* Changes the viewing direction of the camera.
******************************************************************************/
void Viewport::setCameraDirection(const Vector3& newDir)
{
    if(newDir != Vector3::Zero()) {
        Vector3 upVector = cameraUpDirection();
        if(upVector.isZero()) {
            upVector = ViewportSettings::getSettings().upVector();
        }
        setCameraTransformation(AffineTransformation::lookAlong(cameraPosition(), newDir, upVector).inverse());
    }
}

/******************************************************************************
* Returns the position of the camera.
******************************************************************************/
Point3 Viewport::cameraPosition() const
{
    return Point3::Origin() + cameraTransformation().translation();
}

/******************************************************************************
* Changes the viewing direction of the camera.
******************************************************************************/
void Viewport::setCameraPosition(const Point3& p)
{
    AffineTransformation tm = cameraTransformation();
    tm.translation() = p - Point3::Origin();
    setCameraTransformation(tm);
}

/******************************************************************************
* Returns true if the viewport is using a perspective projection;
* returns false if it is using an orthogonal projection.
******************************************************************************/
bool Viewport::isPerspectiveProjection() const
{
    if(viewType() <= VIEW_ORTHO)
        return false;
    else if(viewType() == VIEW_PERSPECTIVE)
        return true;
    else if(viewType() == VIEW_SCENENODE && viewNode() && scene()) {
        if(DataOORef<const AbstractCameraObject> camera = cameraObject(scene()->animationSettings()->currentTime())) {
            return camera->isPerspectiveCamera();
        }
    }
    return false;
}

/******************************************************************************
* Obtains the camera description from the view node.
******************************************************************************/
DataOORef<const AbstractCameraObject> Viewport::cameraObject(AnimationTime time) const
{
    if(viewNode()) {
        if(const AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(viewNode()->source())) {
            return cameraSource->cameraObject(time);
        }
    }
    return {};
}

/******************************************************************************
* Computes the projection matrix and other parameters.
******************************************************************************/
ViewProjectionParameters Viewport::computeProjectionParameters(AnimationTime time, FloatType aspectRatio, const Box3& sceneBoundingBox)
{
    OVITO_ASSERT(aspectRatio > FLOATTYPE_EPSILON);

    ViewProjectionParameters params;
    params.aspectRatio = aspectRatio;
    params.validityInterval.setInfinite();
    if(!sceneBoundingBox.isEmpty())
        params.boundingBox = sceneBoundingBox;
    else
        params.boundingBox = Box3(Point3::Origin(), 1);

    // Get transformation from view scene node.
    if(viewType() == VIEW_SCENENODE && viewNode()) {
        // Get camera transformation.
        params.inverseViewMatrix = viewNode()->getWorldTransform(time, params.validityInterval);
        params.viewMatrix = params.inverseViewMatrix.inverse();

        // The camera description from the view node pipeline.
        if(DataOORef<const AbstractCameraObject> camera = cameraObject(time)) {
            // Get remaining parameters from camera object.
            camera->projectionParameters(time, params);
        }
        else {
            params.fieldOfView = 1;
            params.isPerspective = false;
        }
    }
    else {
        params.inverseViewMatrix = cameraTransformation();
        params.viewMatrix = params.inverseViewMatrix.inverse();
        params.fieldOfView = fieldOfView();
        params.isPerspective = (viewType() == VIEW_PERSPECTIVE);
    }

    // Transform scene bounding box to camera space.
    Box3 bb = params.boundingBox.transformed(params.viewMatrix).centerScale(FloatType(1.01));

    // Compute projection matrix.
    if(params.isPerspective) {
        if(bb.minc.z() < 0) {
            params.zfar = -bb.minc.z();
            params.znear = std::max(-bb.maxc.z(), params.zfar * FloatType(1e-4));
        }
        else {
            params.zfar = std::max(params.boundingBox.size().length(), FloatType(1));
            params.znear = params.zfar * FloatType(1e-4);
        }
        params.zfar = std::max(params.zfar, params.znear * FloatType(1.01));
        params.projectionMatrix = Matrix4::perspective(params.fieldOfView, FloatType(1) / params.aspectRatio, params.znear, params.zfar);
    }
    else {
        if(!bb.isEmpty()) {
            params.znear = -bb.maxc.z();
            params.zfar  = -bb.minc.z();
            if(params.zfar <= params.znear)
                params.zfar  = params.znear + FloatType(1);
        }
        else {
            params.znear = 1;
            params.zfar = 100;
        }
        params.projectionMatrix = Matrix4::ortho(-params.fieldOfView / params.aspectRatio, params.fieldOfView / params.aspectRatio,
                            -params.fieldOfView, params.fieldOfView,
                            params.znear, params.zfar);
    }
    params.inverseProjectionMatrix = params.projectionMatrix.inverse();

    return params;
}

/******************************************************************************
* Zooms to the extents of the given bounding box.
******************************************************************************/
void Viewport::zoomToBox(const Box3& box, FloatType viewportAspectRatio)
{
    if(box.isEmpty() || !scene() || viewportAspectRatio <= 0)
        return;

    if(viewType() == VIEW_SCENENODE)
        return; // Do not reposition the camera object.

    if(isPerspectiveProjection()) {
        FloatType dist = box.size().length() * FloatType(0.5) * std::max(viewportAspectRatio, FloatType(1)) / std::tan(fieldOfView() * FloatType(0.5));
        setCameraPosition(box.center() - cameraDirection().resized(dist));
    }
    else {
        // Set up projection.
        ViewProjectionParameters projParams = computeProjectionParameters(scene()->animationSettings()->currentTime(), viewportAspectRatio, box);

        FloatType minX = FLOATTYPE_MAX, minY = FLOATTYPE_MAX;
        FloatType maxX = FLOATTYPE_MIN, maxY = FLOATTYPE_MIN;
        for(int i = 0; i < 8; i++) {
            Point3 trans = projParams.viewMatrix * box[i];
            if(trans.x() < minX) minX = trans.x();
            if(trans.x() > maxX) maxX = trans.x();
            if(trans.y() < minY) minY = trans.y();
            if(trans.y() > maxY) maxY = trans.y();
        }
        FloatType w = std::max(maxX - minX, FloatType(1e-12));
        FloatType h = std::max(maxY - minY, FloatType(1e-12));
        if(viewportAspectRatio > h/w)
            setFieldOfView(w * viewportAspectRatio * FloatType(0.55));
        else
            setFieldOfView(h * FloatType(0.55));
        setCameraPosition(box.center());
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool Viewport::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == viewNode()) {
            // Adopt camera information from view node.
            if(viewType() == VIEW_SCENENODE && !isBeingLoaded() && !isBeingDeleted() && scene()) {
                // Get camera transformation and settings (FOV etc.).
                AnimationTime time = scene()->animationSettings()->currentTime();
                TimeInterval iv;
                setCameraTransformation(viewNode()->getWorldTransform(time, iv));
                if(DataOORef<const AbstractCameraObject> camera = cameraObject(time)) {
                    setFieldOfView(camera->fieldOfView(time, iv));
                }
            }

            // Update viewport when camera node has moved or modified.
            updateViewport();
        }
        else if(_overlays.contains(source) || _underlays.contains(source)) {
            // Update viewport when one of the layers has changed.
            updateViewport();
        }
        else if(source == scene()) {
            // Repaint viewport if the scene's camera orbit center has changed.
            const TargetChangedEvent& changeEvent = static_cast<const TargetChangedEvent&>(event);
            if(changeEvent.field() == PROPERTY_FIELD(Scene::orbitCenterMode) || changeEvent.field() == PROPERTY_FIELD(Scene::userOrbitCenter)) {
                updateViewport();
            }
        }
    }
    else if(source == viewNode() && event.type() == ReferenceEvent::TitleChanged && !isBeingLoaded()) {
        // Update viewport title when camera node has been renamed.
        updateViewportTitle();
        updateViewport();
    }
    else if(source == scene() && event.type() == ReferenceEvent::ReferenceAdded) {
        // If a new pipeline is being added to the scene, inform all viewport overlays.
        // In case they are not associated any pipeline, they can automatically attach to the new pipeline.
        const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
        if(refEvent.field() == PROPERTY_FIELD(SceneNode::children) && !isUndoingOrRedoing() && !isBeingLoaded()) {
            for(ViewportOverlay* overlay : overlays())
                overlay->sceneNodeAdded(static_object_cast<SceneNode>(refEvent.newTarget()));
            for(ViewportOverlay* overlay : underlays())
                overlay->sceneNodeAdded(static_object_cast<SceneNode>(refEvent.newTarget()));
        }
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void Viewport::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(viewNode) && !isBeingLoaded()) {
        if(viewType() == VIEW_SCENENODE && newTarget == nullptr) {
            // If the camera node has been deleted, switch to Orthographic or Perspective view type.
            setViewType(isPerspectiveProjection() ? VIEW_PERSPECTIVE : VIEW_ORTHO, true);
        }
        else if(viewType() != VIEW_SCENENODE && newTarget != nullptr) {
            setViewType(VIEW_SCENENODE);
        }

        // Update viewport when the camera has been replaced by another scene node.
        updateViewportTitle();
    }
    else if(field == PROPERTY_FIELD(overlays) || field == PROPERTY_FIELD(underlays)) {
        updateViewport();
    }
    RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField.
******************************************************************************/
void Viewport::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(overlays) || field == PROPERTY_FIELD(underlays)) {
        if(ViewportOverlay* overlay = static_object_cast<ViewportOverlay>(newTarget)) {
            if(!isUndoingOrRedoing() && !isBeingLoaded())
                overlay->initializeOverlay(this);
        }
        updateViewport();
    }
    RefTarget::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been removed from a VectorReferenceField.
******************************************************************************/
void Viewport::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(overlays) || field == PROPERTY_FIELD(underlays)) {
        updateViewport();
    }
    RefTarget::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a property field of this object has changed.
******************************************************************************/
void Viewport::propertyChanged(const PropertyFieldDescriptor* field)
{
    RefTarget::propertyChanged(field);
    if(field == PROPERTY_FIELD(viewType) && !isBeingLoaded()) {
        updateViewportTitle();
    }
    else if(field == PROPERTY_FIELD(cameraUpDirection) && !isBeingLoaded()) {
        // Update view matrix when the up-vector has been changed.
        setCameraDirection(cameraDirection());
    }
    updateViewport();
}

/******************************************************************************
* This method is called once for this object after it has been completely deserialized from a data stream.
******************************************************************************/
void Viewport::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);
    updateViewportTitle();
}

/******************************************************************************
* This is called when the global viewport settings have changed.
******************************************************************************/
void Viewport::viewportSettingsChanged(ViewportSettings* newSettings)
{
    // Update camera TM if "up" axis has changed to make it point upward.
    if(ViewportSettings::getSettings().constrainCameraRotation())
        setCameraDirection(cameraDirection());

    // Redraw viewport.
    updateViewport();
}

/******************************************************************************
* Updates the title text of the viewport based on the current view type.
******************************************************************************/
void Viewport::updateViewportTitle()
{
    // Load viewport caption string.
    QString newTitle;
    switch(viewType()) {
        case VIEW_TOP: newTitle = tr("Top"); break;
        case VIEW_BOTTOM: newTitle = tr("Bottom"); break;
        case VIEW_FRONT: newTitle = tr("Front"); break;
        case VIEW_BACK: newTitle = tr("Back"); break;
        case VIEW_LEFT: newTitle = tr("Left"); break;
        case VIEW_RIGHT: newTitle = tr("Right"); break;
        case VIEW_ORTHO: newTitle = tr("Ortho"); break;
        case VIEW_PERSPECTIVE: newTitle = tr("Perspective"); break;
        case VIEW_SCENENODE: newTitle = viewNode() ? viewNode()->sceneNodeName() : tr("No view node"); break;
        default: OVITO_ASSERT(false); // Unknown viewport type
    }
    _viewportTitle.set(this, PROPERTY_FIELD(viewportTitle), std::move(newTitle));
}

/******************************************************************************
* Requests a refresh of all viewport windows associated with this viewport.
******************************************************************************/
void Viewport::updateViewport()
{
    notifyDependents(Viewport::ViewportWindowUpdateRequested);
}

/******************************************************************************
* Determines the aspect ratio of this viewport's area in the rendered output image.
******************************************************************************/
FloatType Viewport::renderAspectRatio(DataSet* dataset) const
{
    if(!dataset || !dataset->renderSettings())
        return 0;

    QRect rect = dataset->renderSettings()->viewportFramebufferArea(this, dataset->viewportConfig());
    if(rect.isEmpty())
        return 0;

    return (FloatType)rect.height() / rect.width();
}

/******************************************************************************
* Returns the current orbit center for this viewport.
******************************************************************************/
Point3 Viewport::orbitCenter()
{
    // Use the target of a camera as the orbit center.
    if(viewNode() && viewType() == Viewport::VIEW_SCENENODE && viewNode()->lookatTargetNode()) {
        AnimationTime time = scene()->animationSettings()->currentTime();
        return Point3::Origin() + viewNode()->lookatTargetNode()->getWorldTransform(time).translation();
    }

    Point3 center = Point3::Origin();
    if(scene()) {
        // Compute scene's orbiting center.
        if(scene()->orbitCenterMode() == Scene::OrbitCenterMode::ORBIT_SELECTION_CENTER) {
            AnimationTime time = scene()->animationSettings()->currentTime();
            Box3 selectionBoundingBox;
            for(SceneNode* node : scene()->selection()->nodes()) {
                selectionBoundingBox.addBox(node->worldBoundingBox(time, this));
            }
            if(!selectionBoundingBox.isEmpty()) {
                center = selectionBoundingBox.center();
            }
            else {
                Box3 sceneBoundingBox = scene()->worldBoundingBox(time, this);
                if(!sceneBoundingBox.isEmpty())
                    center = sceneBoundingBox.center();
            }
        }
        else if(scene()->orbitCenterMode() == Scene::OrbitCenterMode::ORBIT_USER_DEFINED) {
            center = scene()->userOrbitCenter();
        }

        if(viewType() == VIEW_SCENENODE && viewNode() && scene()) {
            if(DataOORef<const AbstractCameraObject> camera = cameraObject(scene()->animationSettings()->currentTime())) {
                if(camera->isPerspectiveCamera()) {
                    // If a free camera node is selected, the current orbit center is at the same location as the camera.
                    // In this case, we should shift the orbit center such that it is in front of the camera.
                    TimeInterval iv;
                    const AffineTransformation cameraTM = viewNode()->getWorldTransform(scene()->animationSettings()->currentTime(), iv);
                    Point3 camPos = Point3::Origin() + cameraTM.translation();
                    if(center.equals(camPos))
                        center = camPos - FloatType(50) * cameraTM.column(2);
                }
            }
        }
    }

    return center;
}

/******************************************************************************
* Returns the nested layout cell this viewport's window is currently in (if any).
******************************************************************************/
ViewportLayoutCell* Viewport::layoutCell() const
{
    ViewportLayoutCell* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        if(ViewportLayoutCell* cell = dynamic_object_cast<ViewportLayoutCell>(dependent)) {
            OVITO_ASSERT(cell->viewport() == this);
            OVITO_ASSERT(!result);
            result = cell;
        }
    });
    return result;
}

/******************************************************************************
* Adds a gizmo to be shown in all interactive viewports.
******************************************************************************/
void Viewport::addViewportGizmo(ViewportGizmo* gizmo)
{
    OVITO_ASSERT(gizmo);
    if(std::find(viewportGizmos().begin(), viewportGizmos().end(), gizmo) == viewportGizmos().end()) {
        _viewportGizmos.push_back(gizmo);

        // Update viewport to show gizmo overlay.
        updateViewport();
    }
}

/******************************************************************************
* Removes a gizmo, which will no longer be shown in the interactive viewports.
******************************************************************************/
void Viewport::removeViewportGizmo(ViewportGizmo* gizmo)
{
    OVITO_ASSERT(gizmo);
    auto iter = std::find(_viewportGizmos.begin(), _viewportGizmos.end(), gizmo);
    if(iter != _viewportGizmos.end()) {
        _viewportGizmos.erase(iter);

        // Update viewport to hide gizmo.
        updateViewport();
    }
}

}   // End of namespace
