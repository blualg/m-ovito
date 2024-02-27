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
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/data/camera/AbstractCameraObject.h>
#include <ovito/core/dataset/data/camera/AbstractCameraSource.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/gui/desktop/dialogs/AdjustViewDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "ViewportMenu.h"

namespace Ovito {

/******************************************************************************
* Initializes the menu.
******************************************************************************/
ViewportMenu::ViewportMenu(MainWindow& mainWindow, ViewportWindow* viewportWindow, QWidget* viewportWidget) :
    QMenu(viewportWidget),
    _mainWindow(mainWindow),
    _viewportWindow(viewportWindow),
    _viewportWidget(viewportWidget)
{
    QAction* action;

    // Build menu.
    action = addAction(tr("Preview Mode"), this, &ViewportMenu::onRenderPreviewMode);
    action->setCheckable(true);
    action->setChecked(viewport()->renderPreviewMode());
#ifdef OVITO_DEBUG
    action = addAction(tr("Show Grid"), this, &ViewportMenu::onShowGrid);
    action->setCheckable(true);
    action->setChecked(viewport()->isGridVisible());
#endif
    action = addAction(tr("Constrain Rotation"), this, &ViewportMenu::onConstrainRotation);
    action->setCheckable(true);
    action->setChecked(ViewportSettings::getSettings().constrainCameraRotation());
    addSeparator();

    _viewTypeMenu = addMenu(tr("View Type"));
    connect(_viewTypeMenu, &QMenu::aboutToShow, this, &ViewportMenu::onShowViewTypeMenu);

    QActionGroup* viewTypeGroup = new QActionGroup(this);
    action = viewTypeGroup->addAction(tr("Top"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_TOP);
    action->setData((int)Viewport::VIEW_TOP);
    action = viewTypeGroup->addAction(tr("Bottom"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_BOTTOM);
    action->setData((int)Viewport::VIEW_BOTTOM);
    action = viewTypeGroup->addAction(tr("Front"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_FRONT);
    action->setData((int)Viewport::VIEW_FRONT);
    action = viewTypeGroup->addAction(tr("Back"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_BACK);
    action->setData((int)Viewport::VIEW_BACK);
    action = viewTypeGroup->addAction(tr("Left"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_LEFT);
    action->setData((int)Viewport::VIEW_LEFT);
    action = viewTypeGroup->addAction(tr("Right"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_RIGHT);
    action->setData((int)Viewport::VIEW_RIGHT);
    action = viewTypeGroup->addAction(tr("Ortho"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_ORTHO);
    action->setData((int)Viewport::VIEW_ORTHO);
    action = viewTypeGroup->addAction(tr("Perspective"));
    action->setCheckable(true);
    action->setChecked(viewport()->viewType() == Viewport::VIEW_PERSPECTIVE);
    action->setData((int)Viewport::VIEW_PERSPECTIVE);
    _viewTypeMenu->addActions(viewTypeGroup->actions());
    connect(viewTypeGroup, &QActionGroup::triggered, this, &ViewportMenu::onViewType);

    addAction(tr("Adjust View..."), this, &ViewportMenu::onAdjustView)->setEnabled(viewport()->viewType() != Viewport::VIEW_SCENENODE);

    addSeparator();

    ViewportConfiguration* viewportConfig = mainWindow.datasetContainer().currentSet()->viewportConfig();

    if(ViewportLayoutCell* layoutCell = viewport()->layoutCell()) {
        QMenu* layoutMenu = addMenu(tr("Window Layout"));
        layoutMenu->setEnabled(viewport() != viewportConfig->maximizedViewport());
        _layoutCell = layoutCell;
        OVITO_ASSERT(layoutCell->splitDirection() == ViewportLayoutCell::None && layoutCell->children().empty());

        // Actions that duplicate the viewport by splitting the layout cell.
        action = layoutMenu->addAction(tr("Split Horizontal"));
        connect(action, &QAction::triggered, this, [&]() { onSplitViewport(ViewportLayoutCell::Horizontal); });
        action = layoutMenu->addAction(tr("Split Vertical"));
        connect(action, &QAction::triggered, this, [&]() { onSplitViewport(ViewportLayoutCell::Vertical); });

        layoutMenu->addSeparator();

        // Action that deletes the viewport from the layout.
        action = layoutMenu->addAction(tr("Remove Viewport"));
        action->setEnabled(layoutCell->parentCell() != nullptr);
        connect(action, &QAction::triggered, this, &ViewportMenu::onDeleteViewport);
    }

    // Pipeline visibility
    QMenu* visibilityMenu = addMenu(tr("Pipeline Visibility"));
    for(SceneNode* node : viewport()->scene()->children()) {
        QAction* action = visibilityMenu->addAction(node->objectTitle());
        action->setData(QVariant::fromValue(OORef<OvitoObject>(node)));
        action->setCheckable(true);
        action->setChecked(!node->isHiddenInViewport(viewport(), false) && node != viewport()->viewNode());
        action->setEnabled(node != viewport()->viewNode());
        connect(action, &QAction::toggled, this, &ViewportMenu::onPipelineVisibility);
    }
    visibilityMenu->setEnabled(!visibilityMenu->isEmpty());
}

/******************************************************************************
* Displays the menu.
******************************************************************************/
void ViewportMenu::show(const QPoint& pos)
{
    // Make sure deleteLater() calls are executed first.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // Show context menu.
    exec(_viewportWidget->mapToGlobal(pos));
}

/******************************************************************************
* Is called just before the "View Type" sub-menu is shown.
******************************************************************************/
void ViewportMenu::onShowViewTypeMenu()
{
    QActionGroup* viewNodeGroup = new QActionGroup(this);
    connect(viewNodeGroup, &QActionGroup::triggered, this, &ViewportMenu::onViewNode);

    // Pipeline evaulation performed in the following requires a valid execution context.
    _mainWindow.handleExceptions([&] {
        // Find all cameras in the scene.
        viewport()->scene()->visitPipelines([this, viewNodeGroup](Pipeline* pipeline) -> bool {
            if(const AbstractCameraSource* cameraSource = dynamic_object_cast<AbstractCameraSource>(pipeline->head())) {
                // Add a menu entry for this camera.
                QAction* action = viewNodeGroup->addAction(pipeline->sceneNodeName());
                action->setCheckable(true);
                action->setChecked(viewport()->viewNode() == pipeline);
                action->setData(QVariant::fromValue((void*)pipeline));
            }
            return true;
        });
    });

    // Add menu entries to menu.
    if(viewNodeGroup->actions().isEmpty() == false) {
        _viewTypeMenu->addSeparator();
        _viewTypeMenu->addActions(viewNodeGroup->actions());
    }

    _viewTypeMenu->addSeparator();
    _viewTypeMenu->addAction(tr("Create Camera"), this, SLOT(onCreateCamera()))->setEnabled(viewport()->viewNode() == nullptr);

    disconnect(_viewTypeMenu, &QMenu::aboutToShow, this, &ViewportMenu::onShowViewTypeMenu);
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onRenderPreviewMode(bool checked)
{
    _mainWindow.handleExceptions([&] {
        viewport()->setRenderPreviewMode(checked);
    });
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onShowGrid(bool checked)
{
    _mainWindow.handleExceptions([&] {
        viewport()->setGridVisible(checked);
    });
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onConstrainRotation(bool checked)
{
    ViewportSettings::getSettings().setConstrainCameraRotation(checked);
    ViewportSettings::getSettings().save();
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onViewType(QAction* action)
{
    _mainWindow.handleExceptions([&] {
        viewport()->setViewType(static_cast<Viewport::ViewType>(action->data().toInt()), true, false);

        // Remember which viewport was maximized across program sessions.
        // The same viewport will be maximized next time OVITO is started.
        ViewportConfiguration* viewportConfig = _mainWindow.datasetContainer().currentSet()->viewportConfig();
        if(viewportConfig->maximizedViewport() == viewport()) {
            ViewportSettings::getSettings().setDefaultMaximizedViewportType(viewport()->viewType());
            ViewportSettings::getSettings().save();
        }
    });
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onAdjustView()
{
    AdjustViewDialog* dialog = new AdjustViewDialog(_mainWindow, viewport(), _viewportWidget);
    dialog->show();
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onViewNode(QAction* action)
{
    Pipeline* pipeline = static_cast<Pipeline*>(action->data().value<void*>());
    OVITO_CHECK_OBJECT_POINTER(pipeline);

    _mainWindow.performTransaction(tr("Set camera"), [this, pipeline]() {
        viewport()->setViewNode(pipeline);
        OVITO_ASSERT(viewport()->viewType() == Viewport::VIEW_SCENENODE);
    });
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onCreateCamera()
{
    _mainWindow.performTransaction(tr("Create camera"), [this]() {
        Scene* scene = viewport()->scene();
        AnimationSuspender animSuspender(_mainWindow);

        // Create and initialize the camera object.
        OORef<Pipeline> cameraPipeline;
        {
            UndoSuspender noUndo;

            // Create an instance of the StandardCameraSource class.
            OvitoClassPtr cameraSourceType = PluginManager::instance().findClass(QStringLiteral("StdObj"), QStringLiteral("StandardCameraSource"));
            if(!cameraSourceType)
                throw Exception(tr("OVITO has been built without support for camera objects."));

            // Note: The StandardCameraSource constructor will adopt the current parameters of this Viewport automatically.
            OORef<PipelineNode> cameraSource = static_object_cast<PipelineNode>(cameraSourceType->createInstance());

            // Create an object node with a data source for the camera.
            cameraPipeline = OORef<Pipeline>::create();
            cameraPipeline->setHead(std::move(cameraSource));

            // Give the new scene node a name.
            cameraPipeline->setSceneNodeName(scene->makeNameUnique(tr("Camera")));

            // Position camera node to match the current view.
            AffineTransformation tm = _viewportWindow->projectionParams().inverseViewMatrix;
            if(_viewportWindow->isPerspectiveProjection() == false) {
                // Position camera with parallel projection outside of scene bounding box.
                tm = tm * AffineTransformation::translation(
                        Vector3(0, 0, -_viewportWindow->projectionParams().znear + FloatType(0.2) * (_viewportWindow->projectionParams().zfar -_viewportWindow->projectionParams().znear)));
            }
            cameraPipeline->transformationController()->setTransformationValue(AnimationTime(0), tm, true);
        }

        // Insert node into scene.
        scene->addChildNode(cameraPipeline);
        if(scene->selection()->nodes().empty())
            scene->selection()->setNode(cameraPipeline);

        // Set new camera as view node for current viewport.
        viewport()->setViewNode(cameraPipeline);
        OVITO_ASSERT(viewport()->viewType() == Viewport::VIEW_SCENENODE);
    });
}

/******************************************************************************
* Deletes the viewport from the current window layout.
******************************************************************************/
void ViewportMenu::onDeleteViewport()
{
    _mainWindow.performTransaction(tr("Remove viewport"), [&]() {
        if(ViewportLayoutCell* parentCell = _layoutCell->parentCell()) {
            parentCell->removeChild(parentCell->children().indexOf(_layoutCell));
            ViewportConfiguration* viewportConfig = _mainWindow.datasetContainer().currentSet()->viewportConfig();
            viewportConfig->layoutRootCell()->pruneViewportLayoutTree();
        }
    });
}

/******************************************************************************
* Splits the viewport's layout cell.
******************************************************************************/
void ViewportMenu::onSplitViewport(ViewportLayoutCell::SplitDirection direction)
{
    _mainWindow.performTransaction(tr("Split viewport"), [&]() {

        OORef<ViewportLayoutCell> newCell = OORef<ViewportLayoutCell>::create();
        newCell->setViewport(CloneHelper::cloneSingleObject(viewport(), true));

        if(ViewportLayoutCell* parentCell = _layoutCell->parentCell()) {
            if(parentCell->splitDirection() == direction) {
                int insertIndex = parentCell->children().indexOf(_layoutCell);
                OVITO_ASSERT(insertIndex >= 0);
                parentCell->insertChild(insertIndex + 1, std::move(newCell), parentCell->childWeights()[insertIndex]);
                return;
            }
        }

        OORef<ViewportLayoutCell> newCell2 = OORef<ViewportLayoutCell>::create();
        newCell2->setViewport(viewport());

        _layoutCell->setSplitDirection(direction);
        _layoutCell->setViewport(nullptr);
        _layoutCell->addChild(std::move(newCell2));
        _layoutCell->addChild(std::move(newCell));
    });
}

/******************************************************************************
* Handles the menu item event.
******************************************************************************/
void ViewportMenu::onPipelineVisibility(bool checked)
{
    QAction* action = qobject_cast<QAction*>(sender());
    OVITO_ASSERT(action);

    _mainWindow.performTransaction(tr("Change pipeline visibility"), [&]() {
        if(OORef<SceneNode> node = static_object_cast<SceneNode>(action->data().value<OORef<OvitoObject>>())) {
            node->setPerViewportVisibility(viewport(), checked);
        }
    });
}

}   // End of namespace
