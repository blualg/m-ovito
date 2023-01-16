////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/gui/base/mainwin/OverlayListItem.h>
#include <ovito/gui/base/mainwin/OverlayTypesModel.h>
#include "CommandPanel.h"
#include "OverlayCommandPage.h"

namespace Ovito {

/******************************************************************************
* Initializes the command panel page.
******************************************************************************/
OverlayCommandPage::OverlayCommandPage(MainWindow& mainWindow, QWidget* parent) : QWidget(parent), _mainWindow(mainWindow)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2,2,2,2);
    layout->setSpacing(4);

    _overlayListModel = new OverlayListModel(this, mainWindow);
    connect(_overlayListModel, &OverlayListModel::selectedItemChanged, this, &OverlayCommandPage::onItemSelectionChanged, Qt::QueuedConnection);

    _newLayerBox = new QComboBox(this);
    layout->addWidget(_newLayerBox);
    _newLayerBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _newLayerBox->setModel(new OverlayTypesModel(this, mainWindow, _overlayListModel));
    _newLayerBox->setMaxVisibleItems(0xFFFF);
    connect(_newLayerBox, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        QComboBox* selector = static_cast<QComboBox*>(sender());
        if(QAction* action = static_cast<OverlayTypesModel*>(selector->model())->actionFromIndex(index))
            action->trigger();
        selector->setCurrentIndex(0);
        _overlayListWidget->setFocus();
    });

    _splitter = new QSplitter(Qt::Vertical);
    _splitter->setChildrenCollapsible(false);

    class OverlayListWidget : public QListView {
    public:
        OverlayListWidget(QWidget* parent, OverlayCommandPage* commandPanelPage) : QListView(parent), _commandPanelPage(commandPanelPage) {}
        virtual QSize sizeHint() const override { return QSize(256, 120); }
    protected:
        virtual bool edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger, QEvent* event) override {
            // Avoid triggering edit mode when user clicks the check box next to a list item.
            if(trigger == QAbstractItemView::SelectedClicked && event->type() == QEvent::MouseButtonRelease) {
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                if(mouseEvent->pos().x() < visualRect(index).left() + 50)
                    trigger = QAbstractItemView::NoEditTriggers;
            }
            return QListView::edit(index, trigger, event);
        }
        virtual void contextMenuEvent(QContextMenuEvent* event) override {
            QMenu menu;
            if(OverlayListItem* currentItem = _commandPanelPage->overlayListModel()->selectedItem()) {
                menu.addAction(_commandPanelPage->_mainWindow.actionManager()->getAction(ACTION_VIEWPORT_LAYER_RENAME));
                menu.addSeparator();
            }
            if(QAction* deleteAction = _commandPanelPage->_mainWindow.actionManager()->getAction(ACTION_VIEWPORT_LAYER_DELETE)) {
                menu.addAction(deleteAction);
            }
            menu.exec(event->globalPos());
        }
    private:
        OverlayCommandPage* _commandPanelPage;
    };

    QWidget* upperContainer = new QWidget();
    _splitter->addWidget(upperContainer);
    QHBoxLayout* subLayout = new QHBoxLayout(upperContainer);
    subLayout->setContentsMargins(0,0,0,0);
    subLayout->setSpacing(2);

    _overlayListWidget = new OverlayListWidget(upperContainer, this);
    _overlayListWidget->setEditTriggers(QAbstractItemView::SelectedClicked);
    _overlayListWidget->setModel(_overlayListModel);
    _overlayListWidget->setSelectionModel(_overlayListModel->selectionModel());
    _overlayListWidget->setItemDelegate(new ExtendedListItemDelegate(_overlayListWidget, OverlayListModel::StatusInfoRole));
    subLayout->addWidget(_overlayListWidget);
    connect(_overlayListWidget, &OverlayListWidget::doubleClicked, this, &OverlayCommandPage::onLayerDoubleClicked);

    QToolBar* editToolbar = new QToolBar(this);
    editToolbar->setOrientation(Qt::Vertical);
    subLayout->addWidget(editToolbar);

    ActionManager* actionManager = mainWindow.actionManager();
    _deleteLayerAction = actionManager->createCommandAction(ACTION_VIEWPORT_LAYER_DELETE, tr("Delete Layer"), "modify_delete_modifier", tr("Remove the selected viewport layer from the stack."));
    _deleteLayerAction->setEnabled(false);
    connect(_deleteLayerAction, &QAction::triggered, this, &OverlayCommandPage::onDeleteLayer);
    editToolbar->addAction(_deleteLayerAction);

    editToolbar->addSeparator();

    _moveLayerUpAction = actionManager->createCommandAction(ACTION_VIEWPORT_LAYER_MOVE_UP, tr("Move Layer Up"), "overlay_move_up", tr("Move the selected viewport layer up in the stack."));
    connect(_moveLayerUpAction, &QAction::triggered, this, &OverlayCommandPage::onLayerMoveUp);
    editToolbar->addAction(_moveLayerUpAction);
    _moveLayerDownAction = actionManager->createCommandAction(ACTION_VIEWPORT_LAYER_MOVE_DOWN, tr("Move Layer Down"), "overlay_move_down", tr("Move the selected viewport layer down in the stack."));
    connect(_moveLayerDownAction, &QAction::triggered, this, &OverlayCommandPage::onLayerMoveDown);
    editToolbar->addAction(_moveLayerDownAction);

    QAction* renameLayerAction = actionManager->createCommandAction(ACTION_VIEWPORT_LAYER_RENAME, tr("Rename..."), "edit_rename_pipeline_item", tr("Give the selected viewport layer a different name."));
    connect(renameLayerAction, &QAction::triggered, this, [this]() {
        _overlayListWidget->edit(_overlayListWidget->currentIndex());
    });

    layout->addWidget(_splitter, 1);

    // Create the properties panel.
    _propertiesPanel = new PropertiesPanel(mainWindow);
    _propertiesPanel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    _splitter->addWidget(_propertiesPanel);
    _splitter->setStretchFactor(1,1);

    connect(&_mainWindow.datasetContainer(), &DataSetContainer::activeViewportChanged, this, &OverlayCommandPage::onActiveViewportChanged);
}

/******************************************************************************
* Loads the layout of the widgets from the settings store.
******************************************************************************/
void OverlayCommandPage::restoreLayout() 
{
    QSettings settings;
    settings.beginGroup("app/mainwindow/viewportlayers");
    QVariant state = settings.value("splitter");
    if(state.canConvert<QByteArray>())
        _splitter->restoreState(state.toByteArray());
}

/******************************************************************************
* Saves the layout of the widgets to the settings store.
******************************************************************************/
void OverlayCommandPage::saveLayout() 
{
    QSettings settings;
    settings.beginGroup("app/mainwindow/viewportlayers");
    settings.setValue("splitter", _splitter->saveState());
}

/******************************************************************************
* Returns the selected viewport layer.
******************************************************************************/
ViewportOverlay* OverlayCommandPage::selectedLayer() const
{
    OverlayListItem* currentItem = overlayListModel()->selectedItem();
    return currentItem ? currentItem->overlay() : nullptr;
}

/******************************************************************************
* This is called when another viewport became active.
******************************************************************************/
void OverlayCommandPage::onActiveViewportChanged(Viewport* activeViewport)
{
    if(!activeViewport)
        _propertiesPanel->setEditObject(nullptr);
    overlayListModel()->setSelectedViewport(activeViewport);
    _newLayerBox->setEnabled(activeViewport != nullptr && _newLayerBox->count() > 1);
}

/******************************************************************************
* Is called when a new layer has been selected in the list box.
******************************************************************************/
void OverlayCommandPage::onItemSelectionChanged()
{
    ViewportOverlay* layer = selectedLayer();
    _propertiesPanel->setEditObject(layer);
    if(layer) {
        _deleteLayerAction->setEnabled(true);
        const auto& overlays = overlayListModel()->selectedViewport()->overlays();
        const auto& underlays = overlayListModel()->selectedViewport()->underlays();
        _moveLayerUpAction->setEnabled(!overlays.contains(layer) || overlays.indexOf(layer) < overlays.size() - 1);
        _moveLayerDownAction->setEnabled(!underlays.contains(layer) || underlays.indexOf(layer) > 0);
    }
    else {
        _deleteLayerAction->setEnabled(false);
        _moveLayerUpAction->setEnabled(false);
        _moveLayerDownAction->setEnabled(false);
    }
}

/******************************************************************************
* This deletes the currently selected viewport layer.
******************************************************************************/
void OverlayCommandPage::onDeleteLayer()
{
    if(ViewportOverlay* layer = selectedLayer()) {
        _mainWindow.performTransaction(tr("Delete layer"), [layer]() {
            layer->deleteReferenceObject();
        });
    }
}

/******************************************************************************
* This called when the user double clicks an item in the layer list.
******************************************************************************/
void OverlayCommandPage::onLayerDoubleClicked(const QModelIndex& index)
{
    if(OverlayListItem* item = overlayListModel()->item(index.row())) {
        if(ViewportOverlay* layer = item->overlay()) {
            // Toggle enabled state of layer.
            _mainWindow.performTransaction(tr("Toggle layer visibility"), [layer]() {
                layer->setEnabled(!layer->isEnabled());
            });
        }
    }
}

/******************************************************************************
* Action handler moving the selected viewport layer up in the stack.
******************************************************************************/
void OverlayCommandPage::onLayerMoveUp()
{
    OverlayListItem* selectedItem = overlayListModel()->selectedItem();
    Viewport* vp = overlayListModel()->selectedViewport();
    if(!selectedItem || !vp) return;
    OORef<ViewportOverlay> layer = selectedItem->overlay();
    if(!layer) return;

    _mainWindow.performTransaction(tr("Move layer up"), [&]() {
        int overlayIndex = vp->overlays().indexOf(layer);
        int underlayIndex = vp->underlays().indexOf(layer);
        if(overlayIndex >= 0 && overlayIndex < vp->overlays().size() - 1) {
            vp->removeOverlay(overlayIndex);
            vp->insertOverlay(overlayIndex+1, layer);
        }
        else if(underlayIndex >= 0) {
            if(underlayIndex == vp->underlays().size() - 1) {
                vp->removeUnderlay(underlayIndex);
                vp->insertOverlay(0, layer);
            }
            else {
                vp->removeUnderlay(underlayIndex);
                vp->insertUnderlay(underlayIndex+1, layer);
            }
        }
        // Make sure the new overlay gets selected in the UI.
        overlayListModel()->setNextToSelectObject(layer);
        _overlayListWidget->setFocus(); 
    });
}

/******************************************************************************
* Action handler moving the selected viewport layer down in the stack.
******************************************************************************/
void OverlayCommandPage::onLayerMoveDown()
{
    OverlayListItem* selectedItem = overlayListModel()->selectedItem();
    Viewport* vp = overlayListModel()->selectedViewport();
    if(!selectedItem || !vp) return;
    OORef<ViewportOverlay> layer = selectedItem->overlay();
    if(!layer) return;

    _mainWindow.performTransaction(tr("Move layer down"), [&]() {
        int underlayIndex = vp->underlays().indexOf(layer);
        int overlayIndex = vp->overlays().indexOf(layer);
        if(underlayIndex >= 1) {
            vp->removeUnderlay(underlayIndex);
            vp->insertUnderlay(underlayIndex-1, layer);
        }
        else if(overlayIndex >= 0) {
            if(overlayIndex == 0) {
                vp->removeOverlay(overlayIndex);
                vp->insertUnderlay(vp->underlays().size(), layer);
            }
            else {
                vp->removeOverlay(overlayIndex);
                vp->insertOverlay(overlayIndex-1, layer);
            }
        }
        // Make sure the new overlay gets selected in the UI.
        overlayListModel()->setNextToSelectObject(layer);
        _overlayListWidget->setFocus(); 
    });
}

}   // End of namespace
