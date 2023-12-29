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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/display/CoordinateDisplayWidget.h>
#include <ovito/gui/desktop/dialogs/AnimationKeyEditorDialog.h>
#include <ovito/gui/desktop/mainwin/ViewportsPanel.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>
#include <ovito/core/dataset/animation/controller/KeyframeController.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindowInterface.h>
#include "XFormModes.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(XFormMode);
DEFINE_REFERENCE_FIELD(XFormMode, selectedNode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(MoveMode);
IMPLEMENT_ABSTRACT_OVITO_CLASS(RotateMode);

/******************************************************************************
* This is called by the system after the input handler has
* become the active handler.
******************************************************************************/
void XFormMode::activated(bool temporaryActivation)
{
    ViewportInputMode::activated(temporaryActivation);

    // Listen to selection change events to update the coordinate display.
    DataSetContainer& datasetContainer = inputManager()->datasetContainer();
    connect(&datasetContainer, &DataSetContainer::selectionChangeComplete, this, &XFormMode::onSelectionChangeComplete);
    connect(&datasetContainer, &DataSetContainer::currentFrameChanged, this, &XFormMode::onCurrentFrameChanged);
    onSelectionChangeComplete(datasetContainer.activeSelectionSet());
}

/******************************************************************************
* This is called by the system after the input handler is
* no longer the active handler.
******************************************************************************/
void XFormMode::deactivated(bool temporary)
{
    if(viewport()) {
        // Restore old state if change has not been committed.
        _undoTransaction.cancel();
        _viewport = nullptr;
    }
    disconnect(&inputManager()->datasetContainer(), &DataSetContainer::selectionChangeComplete, this, &XFormMode::onSelectionChangeComplete);
    disconnect(&inputManager()->datasetContainer(), &DataSetContainer::currentFrameChanged, this, &XFormMode::onCurrentFrameChanged);
    setSelectedNode(nullptr);
    onSelectionChangeComplete(nullptr);
    ViewportInputMode::deactivated(temporary);
}

/******************************************************************************
* Is called when the user has selected a different scene node.
******************************************************************************/
void XFormMode::onSelectionChangeComplete(SelectionSet* selection)
{
    MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface());
    CoordinateDisplayWidget* coordDisplay = mainWindow ? mainWindow->coordinateDisplay() : nullptr;

    if(selection) {
        if(selection->nodes().size() == 1) {
            setSelectedNode(selection->nodes().front());
            if(coordDisplay) {
                updateCoordinateDisplay(coordDisplay);
                coordDisplay->activate(undoDisplayName());
                connect(coordDisplay, &CoordinateDisplayWidget::valueEntered, this, &XFormMode::onCoordinateValueEntered, Qt::ConnectionType(Qt::AutoConnection | Qt::UniqueConnection));
                connect(coordDisplay, &CoordinateDisplayWidget::animatePressed, this, &XFormMode::onAnimateTransformationButton, Qt::ConnectionType(Qt::AutoConnection | Qt::UniqueConnection));
            }
            return;
        }
    }
    setSelectedNode(nullptr);
    if(coordDisplay) {
        disconnect(coordDisplay, &CoordinateDisplayWidget::valueEntered, this, &XFormMode::onCoordinateValueEntered);
        disconnect(coordDisplay, &CoordinateDisplayWidget::animatePressed, this, &XFormMode::onAnimateTransformationButton);
        coordDisplay->deactivate();
    }
}

/******************************************************************************
* Is called when the selected scene node generates a notification event.
******************************************************************************/
bool XFormMode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == selectedNode() && event.type() == SceneNode::TransformationChanged) {
        if(MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface()))
            updateCoordinateDisplay(mainWindow->coordinateDisplay());
    }
    return ViewportInputMode::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the current animation frame has changed.
******************************************************************************/
void XFormMode::onCurrentFrameChanged(int frame)
{
    if(MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface()))
        updateCoordinateDisplay(mainWindow->coordinateDisplay());
}

/******************************************************************************
* Handles the mouse down event for the given viewport.
******************************************************************************/
void XFormMode::mousePressEvent(ViewportWindowInterface* vpwin, QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        if(viewport() == nullptr) {

            // Select object under mouse cursor.
            ViewportPickResult pickResult = vpwin->pick(getMousePosition(event));
            if(pickResult.isValid()) {
                _viewport = vpwin->viewport();
                _startPoint = getMousePosition(event);
                _undoTransaction.begin(inputManager()->userInterface(), undoDisplayName());
                inputManager()->userInterface().performActions(_undoTransaction, [&] {
                    viewport()->scene()->selection()->setNode(pickResult.pipeline());
                });
                _undoSelectionOperation = _undoTransaction.snapshot();
                startXForm();
            }
        }
        return;
    }
    else if(event->button() == Qt::RightButton) {
        if(viewport()) {
            // Restore old state when aborting the operation.
            _undoTransaction.cancel();
            _viewport = nullptr;
            return;
        }
    }
    ViewportInputMode::mousePressEvent(vpwin, event);
}

/******************************************************************************
* Handles the mouse up event for the given viewport.
******************************************************************************/
void XFormMode::mouseReleaseEvent(ViewportWindowInterface* vpwin, QMouseEvent* event)
{
    if(viewport()) {
        // Commit change.
        _undoTransaction.commit();
        _viewport = nullptr;
    }
    ViewportInputMode::mouseReleaseEvent(vpwin, event);
}

/******************************************************************************
* Handles the mouse move event for the given viewport.
******************************************************************************/
void XFormMode::mouseMoveEvent(ViewportWindowInterface* vpwin, QMouseEvent* event)
{
    if(viewport() == vpwin->viewport()) {
        // Take the current mouse cursor position to make the input mode
        // look more responsive. The cursor position recorded when the mouse event was
        // generates may be too old.
        _currentPoint = vpwin->getCurrentMousePos();

        // Revert the previous x-form operation.
        _undoTransaction.revertTo(_undoSelectionOperation);

        inputManager()->userInterface().performActions(_undoTransaction, [&] {
            doXForm();
        });

        // Force immediate viewport repaints.
        inputManager()->userInterface().processViewportUpdateRequests();
    }
    else {
        // Change mouse cursor while hovering over an object.
        setCursor(vpwin->pick(getMousePosition(event)).isValid() ? _xformCursor : QCursor());
    }
    ViewportInputMode::mouseMoveEvent(vpwin, event);
}

/******************************************************************************
* Is called when a viewport looses the input focus.
******************************************************************************/
void XFormMode::focusOutEvent(ViewportWindowInterface* vpwin, QFocusEvent* event)
{
    if(viewport()) {
        // Restore old state if change has not been committed.
        _undoTransaction.cancel();
        _viewport = nullptr;
    }
}

/******************************************************************************
* Returns the origin of the transformation system to use for xform modes.
******************************************************************************/
Point3 XFormMode::transformationCenter()
{
    OVITO_ASSERT(viewport());
    OVITO_ASSERT(viewport()->scene());
    if(!viewport() || !viewport()->scene())
        return Point3::Origin();

    Point3 center = Point3::Origin();
    SelectionSet* selection = viewport()->scene()->selection();
    if(selection && !selection->nodes().empty()) {
        TimeInterval interval;
        AnimationTime time = viewport()->scene()->animationSettings()->currentTime();
        for(SceneNode* node : selection->nodes()) {
            const AffineTransformation& nodeTM = node->getWorldTransform(time, interval);
            center += nodeTM.translation();
        }
        center /= (FloatType)selection->nodes().size();
    }
    return center;
}

/******************************************************************************
* Determines the coordinate system to use for transformation.
*******************************************************************************/
AffineTransformation XFormMode::transformationSystem()
{
    return viewport()->gridMatrix();
}

/******************************************************************************
* Is called when the transformation operation begins.
******************************************************************************/
void MoveMode::startXForm()
{
    _translationSystem = transformationSystem();
    _initialPoint = Point3::Origin();
    viewport()->snapPoint(_startPoint, _initialPoint, _translationSystem);
}

/******************************************************************************
* Is repeatedly called during the transformation operation.
******************************************************************************/
void MoveMode::doXForm()
{
    Point3 point2;
    if(viewport()->snapPoint(_currentPoint, point2, _translationSystem)) {

        // Get movement in world space.
        _delta = _translationSystem * (point2 - _initialPoint);

        // Apply transformation to selected nodes.
        applyXForm(viewport()->scene()->animationSettings()->currentTime(), viewport()->scene()->selection()->nodes(), 1);
    }
}

/******************************************************************************
* Applies the current transformation to a set of nodes.
******************************************************************************/
void MoveMode::applyXForm(AnimationTime time, const QVector<SceneNode*>& nodeSet, FloatType multiplier)
{
    for(SceneNode* node : nodeSet) {
        OVITO_CHECK_OBJECT_POINTER(node);
        OVITO_CHECK_OBJECT_POINTER(node->transformationController());

        // Get node's transformation system.
        AffineTransformation transformSystem = _translationSystem;

        // Get parent's system.
        TimeInterval iv;
        const AffineTransformation& translationSys = node->parentNode()->getWorldTransform(time, iv);

        // Move node in parent's system.
        node->transformationController()->translate(time, _delta * multiplier, translationSys.inverse());
    }
}

/******************************************************************************
* Updates the values displayed in the coordinate display widget.
******************************************************************************/
void MoveMode::updateCoordinateDisplay(CoordinateDisplayWidget* coordDisplay)
{
    if(selectedNode()) {
        coordDisplay->setUnit(coordDisplay->mainWindow().unitsManager().worldUnit());
        if(Controller* ctrl = selectedNode()->transformationController()) {
            if(AnimationSettings* anim = inputManager()->datasetContainer().activeAnimationSettings()) {
                TimeInterval iv;
                Vector3 translation;
                ctrl->getPositionValue(anim->currentTime(), translation, iv);
                coordDisplay->setValues(translation);
                return;
            }
        }
    }
    coordDisplay->setValues(Vector3::Zero());
}

/******************************************************************************
* This signal handler is called by the coordinate display widget when the user
* has changed the value of one of the vector components.
******************************************************************************/
void MoveMode::onCoordinateValueEntered(int component, FloatType value)
{
    if(selectedNode()) {
        if(Controller* ctrl = selectedNode()->transformationController()) {
            if(AnimationSettings* anim = inputManager()->datasetContainer().activeAnimationSettings()) {
                TimeInterval iv;
                Vector3 translation;
                ctrl->getPositionValue(anim->currentTime(), translation, iv);
                translation[component] = value;
                ctrl->setPositionValue(anim->currentTime(), translation, true);
            }
        }
    }
}

/******************************************************************************
* This signal handler is called by the coordinate display widget when the user
* has pressed the "Animate" button.
******************************************************************************/
void MoveMode::onAnimateTransformationButton()
{
    if(selectedNode()) {
        if(PRSTransformationController* prs_ctrl = dynamic_object_cast<PRSTransformationController>(selectedNode()->transformationController())) {
            if(KeyframeController* ctrl = dynamic_object_cast<KeyframeController>(prs_ctrl->positionController())) {
                if(MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface())) {
                    AnimationKeyEditorDialog dlg(ctrl, PROPERTY_FIELD(PRSTransformationController::positionController), mainWindow, *mainWindow);
                    dlg.exec();
                }
            }
        }
    }
}

/******************************************************************************
* Is called when the transformation operation begins.
******************************************************************************/
void RotateMode::startXForm()
{
    _transformationCenter = transformationCenter();
}

/******************************************************************************
* Is repeatedly called during the transformation operation.
******************************************************************************/
void RotateMode::doXForm()
{
    FloatType angle1 = (FloatType)(_currentPoint.y() - _startPoint.y()) / 100;
    FloatType angle2 = (FloatType)(_currentPoint.x() - _startPoint.x()) / 100;

    // Constrain rotation to z-axis.
    _rotation = Rotation(Vector3(0,0,1), angle1);

    // Apply transformation to selected nodes.
    applyXForm(viewport()->scene()->animationSettings()->currentTime(), viewport()->scene()->selection()->nodes(), 1);
}

/******************************************************************************
* Applies the current transformation to a set of nodes.
******************************************************************************/
void RotateMode::applyXForm(AnimationTime time, const QVector<SceneNode*>& nodeSet, FloatType multiplier)
{
    for(SceneNode* node : nodeSet) {
        OVITO_CHECK_OBJECT_POINTER(node);
        OVITO_CHECK_OBJECT_POINTER(node->transformationController());

        // Get transformation system.
        AffineTransformation transformSystem = transformationSystem();
        transformSystem.translation() = _transformationCenter - Point3::Origin();

        // Make transformation system relative to parent's tm.
        TimeInterval iv;
        const AffineTransformation& parentTM = node->parentNode()->getWorldTransform(time, iv);
        transformSystem = transformSystem * parentTM.inverse();

        // Rotate node in transformation system.
        Rotation scaledRot = Rotation(_rotation.axis(), _rotation.angle() * multiplier);
        node->transformationController()->rotate(time, scaledRot, transformSystem);
    }
}

/******************************************************************************
* Updates the values displayed in the coordinate display widget.
******************************************************************************/
void RotateMode::updateCoordinateDisplay(CoordinateDisplayWidget* coordDisplay)
{
    if(selectedNode()) {
        coordDisplay->setUnit(coordDisplay->mainWindow().unitsManager().angleUnit());
        if(Controller* ctrl = selectedNode()->transformationController()) {
            if(AnimationSettings* anim = inputManager()->datasetContainer().activeAnimationSettings()) {
                TimeInterval iv;
                Rotation rotation;
                ctrl->getRotationValue(anim->currentTime(), rotation, iv);
                Vector3 euler = rotation.toEuler(Matrix3::szyx);
                coordDisplay->setValues(Vector3(euler[2], euler[1], euler[0]));
                return;
            }
        }
    }
    coordDisplay->setValues(Vector3::Zero());
}

/******************************************************************************
* This signal handler is called by the coordinate display widget when the user
* has changed the value of one of the vector components.
******************************************************************************/
void RotateMode::onCoordinateValueEntered(int component, FloatType value)
{
    if(selectedNode()) {
        if(Controller* ctrl = selectedNode()->transformationController()) {
            if(MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface())) {
                TimeInterval iv;
                CoordinateDisplayWidget* coordDisplay = mainWindow->coordinateDisplay();
                Vector3 euler = coordDisplay->getValues();
                Rotation rotation = Rotation::fromEuler(Vector3(euler[2], euler[1], euler[0]), Matrix3::szyx);
                if(AnimationSettings* anim = mainWindow->datasetContainer().activeAnimationSettings()) {
                    ctrl->setRotationValue(anim->currentTime(), rotation, true);
                }
            }
        }
    }
}

/******************************************************************************
* This signal handler is called by the coordinate display widget when the user
* has pressed the "Animate" button.
******************************************************************************/
void RotateMode::onAnimateTransformationButton()
{
    if(selectedNode()) {
        PRSTransformationController* prs_ctrl = dynamic_object_cast<PRSTransformationController>(selectedNode()->transformationController());
        if(prs_ctrl) {
            KeyframeController* ctrl = dynamic_object_cast<KeyframeController>(prs_ctrl->rotationController());
            if(ctrl) {
                if(MainWindow* mainWindow = dynamic_cast<MainWindow*>(&inputManager()->userInterface())) {
                    AnimationKeyEditorDialog dlg(ctrl, PROPERTY_FIELD(PRSTransformationController::rotationController), mainWindow, *mainWindow);
                    dlg.exec();
                }
            }
        }
    }
}

}   // End of namespace
