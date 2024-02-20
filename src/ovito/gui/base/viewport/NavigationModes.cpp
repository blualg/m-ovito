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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/data/camera/AbstractCameraObject.h>
#include <ovito/core/dataset/data/camera/AbstractCameraSource.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include "ViewportInputManager.h"
#include "NavigationModes.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(NavigationMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(OrbitMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(PanMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(ZoomMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(FOVMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(PickOrbitCenterMode);

/******************************************************************************
* This is called by the system after the input handler has
* become the active handler.
******************************************************************************/
void NavigationMode::activated(bool temporaryActivation)
{
    _temporaryActivation = temporaryActivation;
    inputManager()->addViewportGizmo(inputManager()->pickOrbitCenterMode());
    ViewportInputMode::activated(temporaryActivation);
}

/******************************************************************************
* This is called by the system after the input handler is
* no longer the active handler.
******************************************************************************/
void NavigationMode::deactivated(bool temporary)
{
    if(_viewport) {
        // Restore old settings if view change has not been committed.
        _viewport->setCameraTransformation(_oldCameraTM);
        _viewport->setFieldOfView(_oldFieldOfView);
        _undoTransaction.cancel();
        _viewport = nullptr;
    }
    inputManager()->removeViewportGizmo(inputManager()->pickOrbitCenterMode());
    ViewportInputMode::deactivated(temporary);
}

/******************************************************************************
* Applies a step-wise change of the view orientation.
******************************************************************************/
void NavigationMode::discreteStep(ViewportWindow* vpwin, QPointF delta)
{
    Viewport* vp = vpwin->viewport();
    if(_viewport == nullptr) {
        std::swap(_viewport, vp);
        _startPoint = QPointF(0,0);
        _oldCameraTM = _viewport->cameraTransformation();
        _oldCameraPosition = _viewport->cameraPosition();
        _oldCameraDirection = _viewport->cameraDirection();
        _oldFieldOfView = _viewport->fieldOfView();
        _oldViewMatrix = vpwin->projectionParams().viewMatrix;
        _oldInverseViewMatrix = vpwin->projectionParams().inverseViewMatrix;
        _currentOrbitCenter = _viewport->orbitCenter();
    }
    modifyView(vpwin, vpwin->viewport(), delta, true);
    std::swap(_viewport, vp);
}

/******************************************************************************
* Handles the mouse down event for the given viewport.
******************************************************************************/
void NavigationMode::mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(event->button() == Qt::RightButton) {
        ViewportInputMode::mousePressEvent(vpwin, event);
        return;
    }

    if(_viewport == nullptr) {
        _viewport = vpwin->viewport();
        _startPoint = getMousePosition(event);
        _oldCameraTM = _viewport->cameraTransformation();
        _oldCameraPosition = _viewport->cameraPosition();
        _oldCameraDirection = _viewport->cameraDirection();
        _oldFieldOfView = _viewport->fieldOfView();
        _oldViewMatrix = vpwin->projectionParams().viewMatrix;
        _oldInverseViewMatrix = vpwin->projectionParams().inverseViewMatrix;
        _currentOrbitCenter = _viewport->orbitCenter();
        _undoTransaction.begin(inputManager()->userInterface(), tr("Modify camera"));
    }
}

/******************************************************************************
* Handles the mouse up event for the given viewport.
******************************************************************************/
void NavigationMode::mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(_viewport) {
        // Commit view change.
        _undoTransaction.commit();
        _viewport = nullptr;

        if(_temporaryActivation)
            inputManager()->removeInputMode(this);
    }
}

/******************************************************************************
* Is called when a viewport looses the input focus.
******************************************************************************/
void NavigationMode::focusOutEvent(ViewportWindow* vpwin, QFocusEvent* event)
{
    if(_viewport) {
        if(_temporaryActivation)
            inputManager()->removeInputMode(this);
    }
}

/******************************************************************************
* Handles the mouse move event for the given viewport.
******************************************************************************/
void NavigationMode::mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(_viewport == vpwin->viewport()) {
        QPointF pos = getMousePosition(event);

        _undoTransaction.revert();
        inputManager()->userInterface().performActions(_undoTransaction, [&] {
            modifyView(vpwin, _viewport, pos - _startPoint, false);
        });

        // Force immediate viewport repaint.
        inputManager()->userInterface().processViewportUpdateRequests();
    }
}

/******************************************************************************
* Returns the camera object associated with the given viewport.
******************************************************************************/
PipelineNode* NavigationMode::getViewportCamera(Viewport* vp)
{
    if(vp->viewNode() && vp->viewType() == Viewport::VIEW_SCENENODE) {
        return vp->viewNode()->source();
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Pan Mode ///////////////////////////////////

/******************************************************************************
* Computes the new view matrix based on the new mouse position.
******************************************************************************/
void PanMode::modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep)
{
    FloatType scaling;
    FloatType normalization = discreteStep ? 20.0 : vpwin->viewportWindowDeviceIndependentSize().height();
    if(vp->isPerspectiveProjection())
        scaling = FloatType(10) * vpwin->projectionParams().nonScalingSize(_currentOrbitCenter, vpwin->viewportWindowDeviceIndependentSize()) / normalization;
    else
        scaling = FloatType(2) * _oldFieldOfView / normalization;
    FloatType deltaX = -scaling * delta.x();
    FloatType deltaY =  scaling * delta.y();
    Vector3 displacement = _oldInverseViewMatrix * Vector3(deltaX, deltaY, 0);
    if(vp->viewNode() == nullptr || vp->viewType() != Viewport::VIEW_SCENENODE || !vp->scene()) {
        vp->setCameraPosition(_oldCameraPosition + displacement);
    }
    else {
        // Get parent's system.
        TimeInterval iv;
        const AffineTransformation& parentSys =
                vp->viewNode()->parentNode()->getWorldTransform(vp->scene()->animationSettings()->currentTime(), iv);

        // Move node in parent's system.
        vp->viewNode()->transformationController()->translate(
                vp->scene()->animationSettings()->currentTime(), displacement, parentSys.inverse());

        // If it's a target camera, move target as well.
        if(vp->viewNode()->lookatTargetNode()) {
            vp->viewNode()->lookatTargetNode()->transformationController()->translate(
                    vp->scene()->animationSettings()->currentTime(), displacement, parentSys.inverse());
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Zoom Mode ///////////////////////////////////

/******************************************************************************
* Computes the new view matrix based on the new mouse position.
******************************************************************************/
void ZoomMode::modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep)
{
    if(vp->isPerspectiveProjection()) {
        FloatType amount =  FloatType(-5) * sceneSizeFactor(vp) * delta.y();
        if(vp->viewNode() == nullptr || vp->viewType() != Viewport::VIEW_SCENENODE || !vp->scene()) {
            vp->setCameraPosition(_oldCameraPosition + _oldCameraDirection.resized(amount));
        }
        else {
            TimeInterval iv;
            const AffineTransformation& sys = vp->viewNode()->getWorldTransform(vp->scene()->animationSettings()->currentTime(), iv);
            vp->viewNode()->transformationController()->translate(
                    vp->scene()->animationSettings()->currentTime(), Vector3(0,0,-amount), sys);
        }
    }
    else {
        if(AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(getViewportCamera(vp))) {
            FloatType newFOV = cameraSource->zoom() * (FloatType)std::exp(0.003 * delta.y());
            cameraSource->setZoom(newFOV);
        }
        else {
            FloatType newFOV = _oldFieldOfView * (FloatType)std::exp(0.003 * delta.y());
            vp->setFieldOfView(newFOV);
        }
    }
}

/******************************************************************************
* Computes a scaling factor that depends on the total size of the scene
* which is used to control the zoom sensitivity in perspective mode.
******************************************************************************/
FloatType ZoomMode::sceneSizeFactor(Viewport* vp)
{
    OVITO_CHECK_OBJECT_POINTER(vp);
    if(vp->scene()) {
        Box3 sceneBoundingBox = vp->scene()->worldBoundingBox(vp->scene()->animationSettings()->currentTime(), vp);
        if(!sceneBoundingBox.isEmpty())
            return sceneBoundingBox.size().length() * FloatType(5e-4);
    }
    return FloatType(0.1);
}

/******************************************************************************
* Zooms the viewport in or out.
******************************************************************************/
void ZoomMode::zoom(Viewport* vp, FloatType steps, UserInterface& ui)
{
    if(vp->viewNode() == nullptr || vp->viewType() != Viewport::VIEW_SCENENODE || !vp->scene()) {
        if(vp->isPerspectiveProjection()) {
            vp->setCameraPosition(vp->cameraPosition() + vp->cameraDirection().resized(sceneSizeFactor(vp) * steps));
        }
        else {
            vp->setFieldOfView(vp->fieldOfView() * std::exp(-steps * FloatType(1e-3)));
        }
    }
    else {
        ui.performTransaction(tr("Zoom viewport"), [this, steps, vp]() {
            if(vp->isPerspectiveProjection()) {
                FloatType amount = sceneSizeFactor(vp) * steps;
                TimeInterval iv;
                const AffineTransformation& sys = vp->viewNode()->getWorldTransform(vp->scene()->animationSettings()->currentTime(), iv);
                vp->viewNode()->transformationController()->translate(vp->scene()->animationSettings()->currentTime(), Vector3(0,0,-amount), sys);
            }
            else {
                if(AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(getViewportCamera(vp))) {
                    FloatType oldFOV = cameraSource->zoom();
                    cameraSource->setZoom(oldFOV * std::exp(-steps * FloatType(1e-3)));
                }
            }
        });
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////// FOV Mode ///////////////////////////////////

/******************************************************************************
* Computes the new field of view based on the new mouse position.
******************************************************************************/
void FOVMode::modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep)
{
    FloatType oldFOV = _oldFieldOfView;
    if(AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(getViewportCamera(vp))) {
        oldFOV = vp->isPerspectiveProjection() ? cameraSource->fov() : cameraSource->zoom();
    }

    FloatType newFOV;
    if(vp->isPerspectiveProjection()) {
        newFOV = oldFOV + (FloatType)delta.y() * FloatType(2e-3);
        newFOV = std::max(newFOV, qDegreesToRadians(FloatType(5.0)));
        newFOV = std::min(newFOV, qDegreesToRadians(FloatType(170.0)));
    }
    else {
        newFOV = oldFOV * (FloatType)exp(FloatType(6e-3) * delta.y());
    }

    if(AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(getViewportCamera(vp))) {
        if(vp->isPerspectiveProjection())
            cameraSource->setFov(newFOV);
        else
            cameraSource->setZoom(newFOV);
    }
    else {
        vp->setFieldOfView(newFOV);
    }
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Orbit Mode ///////////////////////////////////

/******************************************************************************
* Computes the new view matrix based on the new mouse position.
******************************************************************************/
void OrbitMode::modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep)
{
    if(vp->viewType() < Viewport::VIEW_ORTHO)
        vp->setViewType(Viewport::VIEW_ORTHO, true);

    FloatType speed = discreteStep ? 0.05 : (5.0 / vpwin->viewportWindowDeviceIndependentSize().height());
    FloatType deltaTheta = speed * delta.x();
    FloatType deltaPhi = -speed * delta.y();

    Vector3 t1 = _currentOrbitCenter - Point3::Origin();
    Vector3 t2 = (_oldViewMatrix * _currentOrbitCenter) - Point3::Origin();

    if(ViewportSettings::getSettings().constrainCameraRotation()) {
        const Matrix3& coordSys = ViewportSettings::getSettings().coordinateSystemOrientation();
        Vector3 v = _oldViewMatrix * coordSys.column(2);

        FloatType theta, phi;
        if(v.x() == 0 && v.y() == 0)
            theta = FLOATTYPE_PI;
        else
            theta = atan2(v.x(), v.y());
        phi = atan2(sqrt(v.x() * v.x() + v.y() * v.y()), v.z());

        // Restrict rotation to keep the major axis pointing upward (prevent camera from becoming upside down).
        if(phi + deltaPhi < FLOATTYPE_EPSILON)
            deltaPhi = -phi + FLOATTYPE_EPSILON;
        else if(phi + deltaPhi > FLOATTYPE_PI - FLOATTYPE_EPSILON)
            deltaPhi = FLOATTYPE_PI - FLOATTYPE_EPSILON - phi;

        if(vp->viewNode() == nullptr || vp->viewType() != Viewport::VIEW_SCENENODE || !vp->scene()) {
            AffineTransformation newTM =
                    AffineTransformation::translation(t1) *
                    AffineTransformation::rotation(Rotation(ViewportSettings::getSettings().upVector(), -deltaTheta)) *
                    AffineTransformation::translation(-t1) * _oldInverseViewMatrix *
                    AffineTransformation::translation(t2) *
                    AffineTransformation::rotationX(deltaPhi) *
                    AffineTransformation::translation(-t2);
            newTM.orthonormalize();
            vp->setCameraTransformation(newTM);
        }
        else {
            Controller* ctrl = vp->viewNode()->transformationController();
            AnimationTime time = vp->scene()->animationSettings()->currentTime();
            Rotation rotX(Vector3(1,0,0), deltaPhi, false);
            ctrl->rotate(time, rotX, _oldInverseViewMatrix);
            Rotation rotZ(ViewportSettings::getSettings().upVector(), -deltaTheta);
            ctrl->rotate(time, rotZ, AffineTransformation::Identity());
            Vector3 shiftVector = _oldInverseViewMatrix.translation() - (_currentOrbitCenter - Point3::Origin());
            Vector3 translationZ = (Matrix3::rotation(rotZ) * shiftVector) - shiftVector;
            Vector3 translationX = Matrix3::rotation(rotZ) * _oldInverseViewMatrix * ((Matrix3::rotation(rotX) * t2) - t2);
            ctrl->translate(time, translationZ - translationX, AffineTransformation::Identity());
        }
    }
    else {
        if(vp->viewNode() == nullptr || vp->viewType() != Viewport::VIEW_SCENENODE || !vp->scene()) {
            AffineTransformation newTM = _oldInverseViewMatrix *
                    AffineTransformation::translation(t2) *
                    AffineTransformation::rotationY(-deltaTheta) *
                    AffineTransformation::rotationX(deltaPhi) *
                    AffineTransformation::translation(-t2);
            newTM.orthonormalize();
            vp->setCameraTransformation(newTM);
        }
        else {
            Controller* ctrl = vp->viewNode()->transformationController();
            AnimationTime time = vp->scene()->animationSettings()->currentTime();
            Rotation rot = Rotation(Vector3(0,1,0), -deltaTheta, false) * Rotation(Vector3(1,0,0), deltaPhi, false);
            ctrl->rotate(time, rot, _oldInverseViewMatrix);
            Vector3 translation = t2 - (Matrix3::rotation(rot) * t2);
            ctrl->translate(time, translation, _oldInverseViewMatrix);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Pick Orbit Center Mode ////////////////////////////////

/******************************************************************************
* Sets the orbit rotation center to the space location under given mouse coordinates.
******************************************************************************/
bool PickOrbitCenterMode::pickOrbitCenter(ViewportWindow* vpwin, const QPointF& pos)
{
    Point3 p;
    Viewport* vp = vpwin->viewport();
    if(!vp || !vp->scene())
        return false;
    if(findIntersection(vpwin, pos, p)) {
        vp->scene()->setOrbitCenterMode(Scene::ORBIT_USER_DEFINED);
        vp->scene()->setUserOrbitCenter(p);
        return true;
    }
    else {
        vp->scene()->setOrbitCenterMode(Scene::ORBIT_SELECTION_CENTER);
        vp->scene()->setUserOrbitCenter(Point3::Origin());
        vpwin->userInterface().showStatusBarMessage(tr("No object has been picked. Resetting orbit center to default position."), 1200);
        return false;
    }
}

/******************************************************************************
* Handles the mouse down events for a Viewport.
******************************************************************************/
void PickOrbitCenterMode::mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        if(pickOrbitCenter(vpwin, getMousePosition(event)))
            return;
    }
    ViewportInputMode::mousePressEvent(vpwin, event);
}

/******************************************************************************
* Is called when the user moves the mouse while the operation is not active.
******************************************************************************/
void PickOrbitCenterMode::mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    ViewportInputMode::mouseMoveEvent(vpwin, event);

    Point3 p;
    bool isOverObject = findIntersection(vpwin, getMousePosition(event), p);

    if(!isOverObject && _showCursor) {
        _showCursor = false;
        setCursor(QCursor());
    }
    else if(isOverObject && !_showCursor) {
        _showCursor = true;
        setCursor(_hoverCursor);
    }
}

/******************************************************************************
* Finds the closest intersection point between a ray originating from the
* current mouse cursor position and the whole scene.
******************************************************************************/
bool PickOrbitCenterMode::findIntersection(ViewportWindow* vpwin, const QPointF& mousePos, Point3& intersectionPoint)
{
    if(std::optional<ViewportWindow::PickResult> pickResult = vpwin->pick(mousePos)) {
        intersectionPoint = pickResult->hitLocation();
        return true;
    }
    return false;
}

/******************************************************************************
* Lets the input mode render its overlay content in a viewport.
******************************************************************************/
void PickOrbitCenterMode::renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset)
{
    // Prepare the cylinder graphics primitive for rendering the orbit center marker as a crosshair symbol.
    // The graphics primitive is immutable and can be reused across multiple frames.
    auto& orbitCenterMarker = frameGraph.visCache().lookup<CylinderPrimitive>(RendererResourceKey<struct OrbitGlyphCache>{});
    if(!orbitCenterMarker.basePositions()) {
        BufferFactory<Point3G> basePositions(3);
        BufferFactory<Point3G> headPositions(3);
        BufferFactory<ColorG> colors(3);
        basePositions[0] = Point3G(-1,0,0); headPositions[0] = Point3G(1,0,0); colors[0] = ColorG(1,0,0);
        basePositions[1] = Point3G(0,-1,0); headPositions[1] = Point3G(0,1,0); colors[1] = ColorG(0,1,0);
        basePositions[2] = Point3G(0,0,-1); headPositions[2] = Point3G(0,0,1); colors[2] = ColorG(0.4f,0.4f,1);
        orbitCenterMarker.setShape(CylinderPrimitive::CylinderShape);
        orbitCenterMarker.setShadingMode(CylinderPrimitive::NormalShading);
        orbitCenterMarker.setUniformWidth(0.1);
        orbitCenterMarker.setPositions(basePositions.take(), headPositions.take());
        orbitCenterMarker.setColors(colors.take());
    }

    // Determine current center of rotation.
    // Note: Always using the active viewport's orbit center here to show a consistent picture across all viewports.
    Viewport* activeViewport = vp;
    if(dataset) {
        if(dataset->viewportConfig() && dataset->viewportConfig()->activeViewport())
            activeViewport = dataset->viewportConfig()->activeViewport();
    }
    const Point3 center = activeViewport->orbitCenter();

    // The cross-hair should appear always the same size on the screen.
    const FloatType symbolSize = frameGraph.projectionParams().nonScalingSize(center, vpWin->viewportWindowDeviceIndependentSize());

    // Emit the graphics primitive.
    frameGraph.addPrimitive(
        std::make_unique<CylinderPrimitive>(orbitCenterMarker),
        AffineTransformation::translation(center - Point3::Origin()) * AffineTransformation::scaling(symbolSize),
        Box3(Point3(-1), Point3(1)));
}

}   // End of namespace
