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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/ManualSelectionModifier.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ViewportModeButton.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include "ManualSelectionModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ManualSelectionModifierEditor);
SET_OVITO_OBJECT_EDITOR(ManualSelectionModifier, ManualSelectionModifierEditor);

/**
 * Viewport input mode that allows to pick individual elements, adding and removing them
 * from the current selection set.
 */
class PickElementMode : public ViewportInputMode
{
public:

    /// Constructor.
    void initializeObject(ManualSelectionModifierEditor* editor) {
        ViewportInputMode::initializeObject();
        _editor = editor;
    }

    /// Handles the mouse up events for a Viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override {
        if(event->button() == Qt::LeftButton) {
            ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(_editor->editObject());
            if(mod && mod->subject()) {
                // Find out what's under the mouse cursor.
                if(std::optional<ViewportWindow::PickResult> pickResult = vpwin->pick(event->pos())) {
                    // Look up the index of the element that was picked.
                    std::pair<size_t, ConstDataObjectPath> indexAndContainer = mod->subject().dataClass()->elementFromPickResult(*pickResult);
                    if(indexAndContainer.first != std::numeric_limits<size_t>::max()) {
                        // Let the editor class handle it from here.
                        _editor->onElementPicked(*pickResult, indexAndContainer.first, indexAndContainer.second);
                    }
                    else {
                        inputManager()->userInterface().showStatusBarMessage(tr("You did not click on an element of type '%1'.").arg(mod->subject().dataClass()->elementDescriptionName()), 1000);
                    }
                }
            }
        }
        ViewportInputMode::mouseReleaseEvent(vpwin, event);
    }

    /// Handles the mouse events for a Viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override {
        ViewportInputMode::mouseMoveEvent(vpwin, event);

        // Check if a selectable element is beneath the mouse cursor position.
        // If yes, indicate that by changing the mouse cursor shape.
        ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(_editor->editObject());
        if(mod && mod->subject()) {
            // Find out what's under the mouse cursor.
            if(std::optional<ViewportWindow::PickResult> pickResult = vpwin->pick(event->pos())) {
                // Look up the index of the element.
                std::pair<size_t, ConstDataObjectPath> indexAndContainer = mod->subject().dataClass()->elementFromPickResult(*pickResult);
                if(indexAndContainer.first != std::numeric_limits<size_t>::max()) {
                    setCursor(SelectionMode::selectionCursor());
                    return;
                }
            }
        }

        // Switch back to default mouse cursor.
        setCursor(QCursor());
    }

    ManualSelectionModifierEditor* _editor;
};

/**
 * Viewport input mode that allows to select a group of elements by drawing a fence around them.
 */
class FenceSelectionMode : public ViewportInputMode, public ViewportGizmo
{
public:

    /// Constructor.
    void initializeObject(ManualSelectionModifierEditor* editor) {
        ViewportInputMode::initializeObject();
        _editor = editor;
    }

    /// Destructor.
    virtual ~FenceSelectionMode() {
        if(isActive())
            inputManager()->removeInputMode(this);
    }

    /// Handles the mouse down events for a Viewport.
    virtual void mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event) override {
        _fence.clear();
        if(event->button() == Qt::LeftButton) {
            _fence.push_back(Point2(getMousePosition(event).x(), getMousePosition(event).y())
                    * (FloatType)vpwin->devicePixelRatio());
            _activeViewport = vpwin->viewport();
            vpwin->viewport()->updateViewport();
        }
        else ViewportInputMode::mousePressEvent(vpwin, event);
    }

    /// Handles the mouse move events for a Viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override {
        if(!_fence.isEmpty()) {
            _fence.push_back(Point2(getMousePosition(event).x(), getMousePosition(event).y())
                    * (FloatType)vpwin->devicePixelRatio());
            vpwin->viewport()->updateViewport();
        }
        ViewportInputMode::mouseMoveEvent(vpwin, event);
    }

    /// Handles the mouse up events for a Viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override {
        if(!_fence.isEmpty()) {
            if(_fence.size() >= 3) {
                ElementSelectionSet::SelectionMode mode = ElementSelectionSet::SelectionReplace;
                if(event->modifiers().testFlag(Qt::ControlModifier))
                    mode = ElementSelectionSet::SelectionAdd;
                else if(event->modifiers().testFlag(Qt::AltModifier))
                    mode = ElementSelectionSet::SelectionSubtract;
                _editor->onFence(_fence, vpwin, mode);
            }
            _fence.clear();
            _activeViewport = nullptr;
            vpwin->viewport()->updateViewport();
        }
        ViewportInputMode::mouseReleaseEvent(vpwin, event);
    }

    /// Lets the input mode render its 2d overlay content in a viewport.
    virtual void renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset) override {
        if(isActive() && vp == _activeViewport && _fence.size() >= 2) {
            frameGraph.addCommandGroup(FrameGraph::OverLayer).render2DPolyline(_fence.constData(), _fence.size(), ViewportSettings::getSettings().viewportColor(ViewportSettings::COLOR_SELECTION), true, vpWin->viewportWindowDeviceSize());
        }
    }

protected:

    /// This is called by the system when the input handler has become active.
    virtual void activated(bool temporary) override {
        ViewportInputMode::activated(temporary);
        ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(_editor->editObject());
        if(mod && mod->subject()) {
#ifndef Q_OS_MACOS
            inputManager()->userInterface().showStatusBarMessage(
                    tr("Draw a fence around a group of %1 to select. Use CONTROL or ALT keys to extend or reduce existing selection set.")
                    .arg(mod->subject().dataClass()->elementDescriptionName()));
#else
            inputManager()->userInterface().showStatusBarMessage(
                    tr("Draw a fence around a group of %1 to select. Use COMMAND or ALT keys to extend or reduce existing selection set.")
                    .arg(mod->subject().dataClass()->elementDescriptionName()));
#endif
        }
        inputManager()->addViewportGizmo(this);
    }

    /// This is called by the system after the input handler is no longer the active handler.
    virtual void deactivated(bool temporary) override {
        _fence.clear();
        inputManager()->userInterface().clearStatusBarMessage();
        inputManager()->removeViewportGizmo(this);
        ViewportInputMode::deactivated(temporary);
    }

private:

    ManualSelectionModifierEditor* _editor = nullptr;
    QVector<Point2> _fence;
    Viewport* _activeViewport = nullptr;
};

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ManualSelectionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Manual selection"), rolloutParams, "manual:particles.modifiers.manual_selection");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    QGroupBox* operateOnGroup = new QGroupBox(tr("Operate on"));
    QVBoxLayout* sublayout = new QVBoxLayout(operateOnGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(6);
    layout->addWidget(operateOnGroup);

    DataObjectReferenceParameterUI* pclassUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    sublayout->addWidget(pclassUI->comboBox());

    // List only property containers that support element selection.
    pclassUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
        return container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)
            && container->getOOMetaClass().supportsViewportPicking();
    });

    QGroupBox* mouseSelectionGroup = new QGroupBox(tr("Viewport modes"));
    sublayout = new QVBoxLayout(mouseSelectionGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(6);
    layout->addWidget(mouseSelectionGroup);

    OORef<PickElementMode> pickElementMode = OORef<PickElementMode>::create(this);
    connect(this, &QObject::destroyed, pickElementMode, &ViewportInputMode::removeMode);
    ViewportModeAction* pickModeAction = new ViewportModeAction(mainWindow(), tr("Pick"), this, std::move(pickElementMode));
    sublayout->addWidget(new ViewportModeButton(pickModeAction));

    OORef<FenceSelectionMode> fenceMode = OORef<FenceSelectionMode>::create(this);
    connect(this, &QObject::destroyed, fenceMode, &ViewportInputMode::removeMode);
    ViewportModeAction* fenceModeAction = new ViewportModeAction(mainWindow(), tr("Fence selection"), this, std::move(fenceMode));
    sublayout->addWidget(new ViewportModeButton(fenceModeAction));

    // Deactivate input modes when editor is reset.
    connect(this, &PropertiesEditor::contentsReplaced, pickModeAction, &ViewportModeAction::deactivateMode);
    connect(this, &PropertiesEditor::contentsReplaced, fenceModeAction, &ViewportModeAction::deactivateMode);

    QGroupBox* globalSelectionGroup = new QGroupBox(tr("Actions"));
    sublayout = new QVBoxLayout(globalSelectionGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(6);
    layout->addWidget(globalSelectionGroup);

    QPushButton* selectAllBtn = new QPushButton(tr("Select all"));
    connect(selectAllBtn, &QPushButton::clicked, this, &ManualSelectionModifierEditor::selectAll);
    sublayout->addWidget(selectAllBtn);

    QPushButton* invertSelectionBtn = new QPushButton(tr("Invert selection"));
    connect(invertSelectionBtn, &QPushButton::clicked, this, &ManualSelectionModifierEditor::invertSelection);
    sublayout->addWidget(invertSelectionBtn);

    QPushButton* clearSelectionBtn = new QPushButton(tr("Clear selection"));
    connect(clearSelectionBtn, &QPushButton::clicked, this, &ManualSelectionModifierEditor::clearSelection);
    sublayout->addWidget(clearSelectionBtn);

    QPushButton* resetSelectionBtn = new QPushButton(tr("Reset selection to initial state"));
    connect(resetSelectionBtn, &QPushButton::clicked, this, &ManualSelectionModifierEditor::resetSelection);
    sublayout->addWidget(resetSelectionBtn);

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

/******************************************************************************
* Adopts the selection state from the modifier's input.
******************************************************************************/
void ManualSelectionModifierEditor::resetSelection()
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod) return;

    performTransaction(tr("Reset selection"), [this,mod]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {
            mod->resetSelection(node, node->evaluateInput(request).blockForResult());
        }
    });
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ManualSelectionModifierEditor::selectAll()
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod) return;

    performTransaction(tr("Select all"), [this,mod]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {
            mod->selectAll(node, node->evaluateInput(request).blockForResult());
        }
    });
}

/******************************************************************************
* Clears the selection.
******************************************************************************/
void ManualSelectionModifierEditor::clearSelection()
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod) return;

    performTransaction(tr("Clear selection"), [this,mod]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {
            mod->clearSelection(node, node->evaluateInput(request).blockForResult());
        }
    });
}

/******************************************************************************
* Inverts the selection.
******************************************************************************/
void ManualSelectionModifierEditor::invertSelection()
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod) return;

    performTransaction(tr("Invert selection"), [this,mod]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {
            mod->invertSelection(node, node->evaluateInput(request).blockForResult());
        }
    });
}

/******************************************************************************
* This is called when the user has selected an element.
******************************************************************************/
void ManualSelectionModifierEditor::onElementPicked(const ViewportWindow::PickResult& pickResult, size_t elementIndex, const ConstDataObjectPath& pickedObjectPath)
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod || !mod->subject()) return;

    performTransaction(tr("Toggle selection"), [this, mod, elementIndex, &pickedObjectPath, &pickResult]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {

            // Make sure we are in the right data pipeline.
            if(!node->pipelines(true).contains(pickResult.sceneNode()->pipeline()))
                continue;

            // Get the modifier's input data.
            const PipelineFlowState& modInput = node->evaluateInput(request).blockForResult();
            const ConstDataObjectPath& inputObjectPath = modInput.expectObject(mod->subject());

            // Look up the right element in the modifier's input.
            // Note that elements may have been added or removed further down the pipeline.
            // Thus, we need to translate the element index into the pipeline output data collection
            // into an index into the modifier's input data collection.
            size_t translatedIndex = mod->subject().dataClass()->remapElementIndex(pickedObjectPath, elementIndex, inputObjectPath);
            if(translatedIndex != std::numeric_limits<size_t>::max()) {
                mod->toggleElementSelection(node, modInput, translatedIndex);
                break;
            }
            else {
                mainWindow().showStatusBarMessage(tr("Cannot select this element, because it doesn't exist in the modifier's input data."), 2000);
            }
        }
    });
}

/******************************************************************************
* This is called when the user has drawn a fence around particles.
******************************************************************************/
void ManualSelectionModifierEditor::onFence(const QVector<Point2>& fence, ViewportWindow* vpwin, ElementSelectionSet::SelectionMode mode)
{
    ManualSelectionModifier* mod = static_object_cast<ManualSelectionModifier>(editObject());
    if(!mod || !mod->subject()) return;

    performTransaction(tr("Select"), [this, mod, &fence, vpwin, mode]() {
        PipelineEvaluationRequest request(currentAnimationTime(), false, true);
        for(ModificationNode* node : modificationNodes()) {

            // Get the modifier's input data.
            const PipelineFlowState& modInput = node->evaluateInput(request).blockForResult();
            const ConstDataObjectPath& inputObjectPath = modInput.expectObject(mod->subject());

            // Iterate over the pipelines.
            // We'll need their object-to-world transformation.
            for(Pipeline* pipeline : node->pipelines(true)) {
                for(SceneNode* sceneNode : pipeline->sceneNodes()) {
                    // Set up projection matrix transforming elements from object space to screen space.
                    const AffineTransformation& nodeTM = sceneNode->getWorldTransform(request.time());
                    Matrix4 ndcToScreen = Matrix4::Identity();
                    ndcToScreen(0,0) = 0.5 * vpwin->viewportWindowDeviceSize().width();
                    ndcToScreen(1,1) = 0.5 * vpwin->viewportWindowDeviceSize().height();
                    ndcToScreen(0,3) = ndcToScreen(0,0);
                    ndcToScreen(1,3) = ndcToScreen(1,1);
                    ndcToScreen(1,1) = -ndcToScreen(1,1);   // Vertical flip.
                    Matrix4 projectionTM = ndcToScreen * vpwin->projectionParams().projectionMatrix * (vpwin->projectionParams().viewMatrix * nodeTM);

                    // Determine which elements are within the closed fence polygon.
                    ConstPropertyPtr selection = mod->subject().dataClass()->viewportFenceSelection(fence, inputObjectPath, pipeline, projectionTM);
                    if(selection) {
                        mod->setSelection(node, modInput, std::move(selection), mode);
                    }
                    else {
                        throw Exception(tr("Sorry, making a fence-based selection is not supported for %1.").arg(mod->subject().dataClass()->elementDescriptionName()));
                    }
                    break;
                }
                break;
            }
        }
    });
}

}   // End of namespace
